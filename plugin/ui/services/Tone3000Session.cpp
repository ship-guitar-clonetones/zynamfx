#include "Tone3000Session.h"

#include <juce_events/juce_events.h>

#include "T3kConfig.h"
#include "services/ForkKeyStore.h"  // fork-only: bring-your-own API key

namespace t3k::ui {

namespace {
// Support greppable, like the editor's "[Editor]" and the processor's
// "[Processor]" lines. Never log a key or a token — only whether one exists.
void authLog(const juce::String& message) { juce::Logger::writeToLog("[auth] " + message); }

const char* phaseName(ToneSession::AuthFlow::Phase phase) {
  switch (phase) {
    case ToneSession::AuthFlow::Phase::idle: return "idle";
    case ToneSession::AuthFlow::Phase::leaving: return "leaving";
    case ToneSession::AuthFlow::Phase::returning: return "returning";
    case ToneSession::AuthFlow::Phase::error: return "error";
  }
  return "?";
}

constexpr const char* kNoKeyMessage =
    "TONE3000 API key not configured. Open Plugin Settings > TONE3000 API key, "
    "get your free key from the link there, paste it, then sign in again.";
constexpr const char* kLoopbackFailed = "Could not start the local sign-in listener. Try again.";
// A Select return that carried a tone but no code: the selection is known,
// but nothing can be downloaded without a session.
constexpr const char* kSignInUnfinished =
    "TONE3000 didn't finish signing you in, so that tone can't be downloaded. "
    "Sign in and choose the tone again.";
// The launcher itself refused (AuthFlow::browserProblem; the screen adds
// what to do instead). A browser that starts and then dies reports nothing
// here: that is what the always-on fallbacks are for.
constexpr const char* kBrowserFailed = "Your browser couldn't be opened.";
// The device flow's outcomes (AuthFlow::Device::error).
constexpr const char* kDeviceCodeFailed = "Couldn't get a code from the catalog service.";
constexpr const char* kDeviceExpired = "That code expired.";
constexpr const char* kDeviceDenied = "Sign-in was declined on the other device.";
constexpr const char* kDeviceFailed = "The catalog service stopped waiting for that code.";
// RFC 8628 §3.5: slow_down adds 5 s to the poll interval.
constexpr int kSlowDownMs = 5000;
// Only the first model: native stores and loads the active model; the
// detail card pages the full catalog separately.
constexpr int kFirstModelOnly = 1;
// Tones max out at 300 models, so one page covers the picker.
constexpr int kAllModels = 300;
// The taxonomy endpoints' page cap: one page fills a filter menu.
constexpr int kTaxonomyPageSize = 25;
}  // namespace

Tone3000Session::Config Tone3000Session::Config::fromBuild() {
  return {config::kApiOrigin, config::kPublishableKey, config::kArchitecture};
}

// Fork-only: a personal key saved in settings (Plugin Settings > TONE3000 API key)
// wins over the baked build key, so keyless public builds still sign in under
// the user's own TONE3000 client_id.
static Tone3000Session::Config withForkKey(UiPrefs& prefs, Tone3000Session::Config config) {
  if (const auto custom = fork::customPublishableKey(prefs); custom.isNotEmpty())
    config.publishableKey = custom;
  return config;
}

Tone3000Session::Tone3000Session(Backend& backend, UiPrefs& prefs, HttpTransport& http, Config config)
    : backend_(backend),
      prefs_(prefs),
      http_(http),
      buildKey_(config.publishableKey),
      config_(withForkKey(prefs, std::move(config))),
      client_(http, prefs, config_.apiOrigin, config_.publishableKey) {
  authLog("session constructed: origin=" + config_.apiOrigin + " key=" +
          (config_.publishableKey.isEmpty() ? "none" : "present") + " auth=" +
          (authenticated() ? "yes" : "no"));
  // Every new or refreshed token set reaches native as it happens.
  client_.onTokensUpdated = [this](const Tokens& t) { pushToken(t.access); };
  client_.onAuthRequired = [this] {
    // The refresh token was rejected: the next + / login re-authenticates.
    // The identity is gone too.
    prefs_.remove(UiPrefs::kCachedUser);
    notifySessionChanged();
  };
  loopback_.onCallback = [this](const juce::String& query) { handleCallback(query); };

  // A remembered login never fires the token listener, so native would sit
  // tokenless until the next refresh: sync once, and refresh the identity.
  if (authenticated()) ensureNativeAuth({});
}

Tone3000Session::~Tone3000Session() = default;

// Identity
std::optional<User> Tone3000Session::user() const {
  if (!authenticated()) return std::nullopt;
  const auto cached = prefs_.getJson(UiPrefs::kCachedUser);
  if (!cached.isObject()) return std::nullopt;
  return User::parse(cached);
}

void Tone3000Session::refreshUser() {
  client_.getUser(scope_.wrap([this](Result<juce::var> r) {
    // The avatar is decorative; auth failures surface via the flows.
    if (!r || !r->isObject()) return;
    prefs_.setJson(UiPrefs::kCachedUser, *r);
    notifySessionChanged();
  }));
}

void Tone3000Session::pushToken(const juce::String& token) { backend_.setAccessToken(token); }

void Tone3000Session::ensureNativeAuth(Done done) {
  client_.getAccessToken(scope_.wrap([this, fin = std::move(done)](Result<juce::String> token) {
    if (token) pushToken(*token);
    if (fin) fin(token ? juce::String() : token.error);
  }));
}

void Tone3000Session::logout() {
  // Auth goes everywhere at once: the persisted tokens, any mid-flight PKCE
  // state, the cached identity and native's Bearer copy. (The system
  // browser's own tone3000.com session is the user's, and stays.)
  cancelFlow();
  client_.clearTokens();
  pkce_.reset();
  lastIntent_ = LoginIntent::plain;
  prefs_.remove(UiPrefs::kCachedUser);
  pushToken({});
  notifySessionChanged();
}

// Catalog
void Tone3000Session::getTone(int toneId, Reply<Tone> reply) {
  client_.getTone(toneId, [cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<Tone>::fail(r.error));
    cb(Result<Tone>::ok(Tone::parse(*r)));
  });
}

void Tone3000Session::listToneModels(int toneId, const juce::String& format, Reply<std::vector<Model>> reply) {
  const bool nam = format.equalsIgnoreCase("nam");
  client_.listModels(toneId, kAllModels, nam ? config_.architecture : -1, [cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<std::vector<Model>>::fail(r.error));
    std::vector<Model> models;
    if (const auto* rows = (*r)["data"].getArray())
      for (const auto& m : *rows) models.push_back(Model::parse(m));
    cb(Result<std::vector<Model>>::ok(std::move(models)));
  });
}

void Tone3000Session::setToneFavorite(int toneId, bool favorite, Done done) {
  client_.setFavorite(toneId, favorite, [fin = std::move(done)](Result<bool> r) {
    if (fin) fin(r ? juce::String() : r.error);
  });
}

void Tone3000Session::searchTones(const ToneQuery& query, int page, int pageSize, Reply<TonePage> reply) {
  const auto path = query.requestPath(page, pageSize, config_.architecture);
  authLog("catalog: list " + path);
  client_.listTones(path, [cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<TonePage>::fail(r.error));
    cb(Result<TonePage>::ok(TonePage::parse(*r)));
  });
}

void Tone3000Session::listTrending(const juce::String& gear, Reply<std::vector<Tone>> reply) {
  authLog("catalog: trending" + (gear.isEmpty() ? juce::String() : " (gear=" + gear + ")"));
  client_.listTrending(gear, [cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<std::vector<Tone>>::fail(r.error));
    std::vector<Tone> tones;
    if (const auto* rows = (*r)["data"].getArray())
      for (const auto& t : *rows) tones.push_back(Tone::parse(t));
    cb(Result<std::vector<Tone>>::ok(std::move(tones)));
  });
}

void Tone3000Session::listTaxonomy(Taxonomy kind, const juce::String& text, Reply<std::vector<TaxonomyEntry>> reply) {
  // Tags and makes filter by name; creators by username (what the API
  // matches `creators` against), so that is the name offered.
  const bool creators = kind == Taxonomy::creators;
  client_.listTaxonomy(kind, text, kTaxonomyPageSize, [creators, cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<std::vector<TaxonomyEntry>>::fail(r.error));
    std::vector<TaxonomyEntry> entries;
    if (const auto* rows = (*r)["data"].getArray())
      for (const auto& row : *rows) {
        const auto name = row[creators ? "username" : "name"].toString().trim();
        if (name.isNotEmpty()) entries.push_back({name, creators ? row["avatar_url"].toString() : juce::String()});
      }
    cb(Result<std::vector<TaxonomyEntry>>::ok(std::move(entries)));
  });
}

void Tone3000Session::fetchToneAndModels(int toneId, Reply<Tone> reply) {
  authLog("tone: fetching metadata for " + juce::String(toneId));
  getTone(toneId, [this, toneId, cb = std::move(reply)](Result<Tone> tone) {
    if (!tone) {
      authLog("tone " + juce::String(toneId) + ": metadata failed: " + tone.error);
      return cb(std::move(tone));
    }
    // Only NAM tones take the architecture filter; IR and other formats
    // are not NAM architectures.
    const int architecture = tone->isNam() ? config_.architecture : -1;
    client_.listModels(toneId, kFirstModelOnly, architecture, [found = *tone, cb](Result<juce::var> models) {
      if (!models) {
        authLog("tone " + juce::String(found.id) + ": model lookup failed: " + models.error);
        return cb(Result<Tone>::fail("\"" + found.title + "\": " + models.error));
      }
      std::vector<Model> rows;
      if (const auto* arr = (*models)["data"].getArray())
        for (const auto& m : *arr) {
          auto model = Model::parse(m);
          if (model.id > 0 && model.modelUrl.isNotEmpty()) rows.push_back(std::move(model));
          else if (model.id > 0)
            authLog("tone " + juce::String(found.id) + ": model " + juce::String(model.id) +
                    " has no download URL");
        }
      // A tone with nothing loadable used to reach native as an empty model
      // list: the browser closed, native never made a block, and nothing said
      // why. Refuse it here with the stage named, so the card can show it.
      if (rows.empty()) {
        authLog("tone " + juce::String(found.id) + ": no loadable " +
                (found.isNam() ? "A2 NAM" : "downloadable") + " model");
        return cb(Result<Tone>::fail(
            found.isNam() ? "\"" + found.title + "\" has no A2 NAM model. Choose another tone."
                          : "\"" + found.title + "\" has no downloadable model."));
      }
      authLog("tone " + juce::String(found.id) + ": loading model " + juce::String(rows.front().id));
      cb(Result<Tone>::ok(found.withModels(std::move(rows))));
    });
  });
}

void Tone3000Session::selectTone(int toneId, Done done) {
  // Both in flight together, like the web's Promise.all: the tone with its
  // first model, and a fresh token pushed to native before the load starts.
  authLog("select: resolving tone " + juce::String(toneId));
  struct Pending {
    std::optional<Tone> tone;
    bool tokenReady = false;
    bool failed = false;
  };
  auto pending = std::make_shared<Pending>();
  auto settle = scope_.wrap([this, pending, done](const juce::String& error) {
    if (pending->failed) return;
    if (error.isNotEmpty()) {
      pending->failed = true;
      if (done) done(error);
      return;
    }
    if (!pending->tone || !pending->tokenReady) return;
    authLog("select: tone resolved, handing it to the chain");
    if (onToneSelected) onToneSelected(*pending->tone);
    if (done) done({});
  });
  fetchToneAndModels(toneId, [pending, settle](Result<Tone> tone) {
    if (!tone) return settle(tone.error);
    pending->tone = *tone;
    settle(juce::String());
  });
  ensureNativeAuth([pending, settle](const juce::String& error) {
    if (error.isNotEmpty()) return settle(error);
    pending->tokenReady = true;
    settle(juce::String());
  });
}

// Flows
// A phase move keeps the leaving state's extras (URL, browser problem,
// device code) only while it stays leaving.
void Tone3000Session::setFlow(AuthFlow::Phase phase, juce::String error) {
  if (phase != AuthFlow::Phase::leaving) flow_ = {};
  flow_.phase = phase;
  flow_.error = std::move(error);
  notifyAuthFlowChanged();
}

void Tone3000Session::clearAuthError() { setFlow(AuthFlow::Phase::idle); }

juce::String Tone3000Session::openBrowser(const juce::String& url) {
  return juce::URL(url).launchInDefaultBrowser() ? juce::String() : juce::String(kBrowserFailed);
}

void Tone3000Session::refreshCredentials() {
  // A key saved by another host process (or by Settings in this one) is on
  // disk; pull it in before deciding whether sign-in can start.
  prefs_.sync();
  const auto key = fork::effectivePublishableKey(prefs_, buildKey_);
  if (key == config_.publishableKey) return;
  authLog("publishable key changed since this session was built: " +
          juce::String(config_.publishableKey.isEmpty() ? "none" : "present") + " -> " +
          juce::String(key.isEmpty() ? "none" : "present"));
  config_.publishableKey = key;
  client_.setPublishableKey(key);
  // The stored tokens were issued to the previous client id: they cannot be
  // refreshed or used under the new one.
  client_.clearTokens();
  prefs_.remove(UiPrefs::kCachedUser);
  pushToken({});
  notifySessionChanged();
}

void Tone3000Session::login(LoginIntent intent) {
  refreshCredentials();
  const auto keyProblem = fork::checkPublishableKey(config_.publishableKey);
  if (config_.publishableKey.isEmpty()) {
    // Say why the key isn't usable, when the last save told us.
    if (fork::rejectedSecretKey(prefs_)) {
      authLog("login refused: the saved key is a secret key (t3k_cs_)");
      setFlow(AuthFlow::Phase::error, fork::keyProblemText(fork::KeyProblem::secret));
      return;
    }
    authLog("login refused: no publishable key configured (Settings > TONE3000 API key)");
    setFlow(AuthFlow::Phase::error, kNoKeyMessage);
    return;
  }
  if (keyProblem != fork::KeyProblem::none) {
    authLog("login refused: saved publishable key rejected locally");
    setFlow(AuthFlow::Phase::error, fork::keyProblemText(keyProblem));
    return;
  }
  lastIntent_ = intent;
  stopDeviceFlow();
  // The sign-in screen comes up at once; the browser takes a beat.
  setFlow(AuthFlow::Phase::leaving);
  if (!loopback_.start()) {
    authLog("login failed: the local sign-in listener could not bind");
    setFlow(AuthFlow::Phase::error, kLoopbackFailed);
    return;
  }
  pkce_ = oauth::Pkce::generate();
  redirectUri_ = loopback_.redirectUri();
  juce::StringPairArray extra;
  extra.set("menubar", "true");
  extra.set("prompt", "select_tone");
  extra.set("architecture", juce::String(config_.architecture));
  extra.set("preview", "true");
  flow_.authorizeUrl = oauth::authorizeUrl(config_.apiOrigin, config_.publishableKey, redirectUri_, *pkce_, extra);
  flow_.redirectUri = redirectUri_;
#if JUCE_WEB_BROWSER
  // The editor's in-app browser loads authorizeUrl itself (PluginRoot opens
  // AuthBrowserPanel while the phase is `leaving`). Nothing is launched
  // externally, so there is no launcher problem to report; the panel still
  // offers "Open in Browser" and the screen keeps the copy-link and device
  // fallbacks.
  flow_.browserProblem = {};
#else
  // Best effort, whatever the platform: a browser that would not open is
  // no dead end, since the listener is up for a pasted link and the screen
  // offers the device flow either way.
  flow_.browserProblem = openBrowser(flow_.authorizeUrl);
#endif
  authLog("login: browser flow started, redirect=" + redirectUri_ +
          (flow_.browserProblem.isEmpty() ? juce::String() : " (launcher refused: " + flow_.browserProblem + ")"));
  notifyAuthFlowChanged();
}

void Tone3000Session::retryFlow() { login(lastIntent_); }

void Tone3000Session::cancelFlow() {
  loopback_.stop();
  pkce_.reset();
  stopDeviceFlow();
  if (flow_.phase == AuthFlow::Phase::leaving || flow_.phase == AuthFlow::Phase::returning)
    setFlow(AuthFlow::Phase::idle);
}

void Tone3000Session::handleCallback(const juce::String& query) {
  authLog(juce::String("callback received (phase=") + phaseName(flow_.phase) + ")");
  // No PKCE state means no flow is waiting (cancelled, finished, or a stale
  // tab): the callback is dropped, never silently — one log line says why.
  if (!pkce_) {
    authLog("callback ignored: no sign-in is waiting for it (cancelled or already finished)");
    return;
  }
  const auto pkce = *pkce_;
  const auto cb = oauth::Callback::parse(query, pkce.state);
  pkce_.reset();  // single use
  loopback_.stop();
  stopDeviceFlow();  // the browser won the race

  switch (cb.kind) {
    case oauth::Callback::Kind::canceled:
      authLog("callback: the user cancelled the flow");
      setFlow(AuthFlow::Phase::idle);
      return;
    case oauth::Callback::Kind::error:
      authLog("callback: the flow failed at TONE3000 (" + cb.error + ")");
      // The loopback listener serves one redirect, so a stale tab (an earlier
      // attempt's state) ends this flow too; say what to do instead of
      // echoing the OAuth error code.
      setFlow(AuthFlow::Phase::error,
              cb.error == "state_mismatch"
                  ? juce::String("This sign-in link belongs to an earlier attempt. Close the browser tab, then try again.")
                  : cb.error);
      return;
    case oauth::Callback::Kind::tone:
      // A tone with no code: load it only if a session already exists.
      authLog("callback: tone " + juce::String(cb.toneId) + " returned without an authorization code");
      if (!client_.authenticated()) {
        setFlow(AuthFlow::Phase::error, kSignInUnfinished);
        return;
      }
      setFlow(AuthFlow::Phase::idle);
      selectTone(cb.toneId, scope_.wrap([this](const juce::String& error) {
        if (error.isNotEmpty()) {
          authLog("selected tone failed to resolve: " + error);
          setFlow(AuthFlow::Phase::error, error);
        }
      }));
      return;
    case oauth::Callback::Kind::code:
      break;
  }
  setFlow(AuthFlow::Phase::returning);
  authLog("callback: exchanging the authorization code");
  client_.exchangeCode(cb.code, pkce.verifier, redirectUri_, scope_.wrap([this, toneId = cb.toneId](Result<Tokens> tokens) {
    if (flow_.phase != AuthFlow::Phase::returning) return;  // cancelled meanwhile
    if (!tokens) {
      authLog("token exchange failed: " + tokens.error);
      setFlow(AuthFlow::Phase::error, tokens.error);
      return;
    }
    finishSignIn(*tokens);
    if (toneId > 0) {
      authLog("callback: loading the selected tone " + juce::String(toneId));
      selectTone(toneId, scope_.wrap([this](const juce::String& error) {
        if (error.isNotEmpty()) {
          authLog("selected tone failed to resolve: " + error);
          setFlow(AuthFlow::Phase::error, error);
        }
      }));
    }
  }));
}

void Tone3000Session::finishSignIn(const Tokens& tokens) {
  const bool wantsBrowser = lastIntent_ == LoginIntent::browse;
  loopback_.stop();
  pkce_.reset();
  stopDeviceFlow();
  client_.setTokens(tokens);
  authLog("signed in: tokens stored (access " + juce::String(tokens.access.length()) + " chars, refresh " +
          (tokens.refresh.isEmpty() ? "none" : "present") + ")");
  notifySessionChanged();
  // The public build keeps catalog identity and browsing on TONE3000's
  // hosted Select flow; it does not call the custom profile/search API.
  if (wantsBrowser && onAuthenticated) onAuthenticated();
  setFlow(AuthFlow::Phase::idle);
}

// Device flow
void Tone3000Session::startDeviceFlow() {
  // Device authorization is intentionally unavailable in the public Free-tier build.
}

void Tone3000Session::pollDevice() {
  if (juce::Time::currentTimeMillis() >= deviceDeadlineMs_) {
    failDevice(kDeviceExpired);
    return;
  }
  client_.pollDeviceToken(deviceCode_, scope_.wrap([this](Result<Tokens> tokens) {
    if (flow_.phase != AuthFlow::Phase::leaving || !flow_.device ||
        flow_.device->state != AuthFlow::Device::State::waiting)
      return;
    if (tokens) {
      finishSignIn(*tokens);
      return;
    }
    if (tokens.error == "slow_down") devicePollMs_ += kSlowDownMs;
    // Pending, slow_down and a transport hiccup all mean: ask again later.
    if (tokens.error == "authorization_pending" || tokens.error == "slow_down" ||
        tokens.error == "token_refresh_failed") {
      devicePoll_.start(devicePollMs_, [this] { pollDevice(); });
      return;
    }
    failDevice(tokens.error == "expired_token"  ? kDeviceExpired
               : tokens.error == "access_denied" ? kDeviceDenied
                                                 : kDeviceFailed);
  }));
}

void Tone3000Session::failDevice(juce::String why) {
  devicePoll_.cancel();
  deviceCode_.clear();
  if (!flow_.device) return;
  flow_.device->state = AuthFlow::Device::State::failed;
  flow_.device->error = std::move(why);
  notifyAuthFlowChanged();
}

void Tone3000Session::stopDeviceFlow() {
  devicePoll_.cancel();
  deviceCode_.clear();
  flow_.device.reset();
}

// Reachability
bool Tone3000Session::online() const {
  // navigator.onLine: false only when no interface but loopback is up.
  for (const auto& address : juce::IPAddress::getAllAddresses(true))
    if (address != juce::IPAddress::local() && address != juce::IPAddress::local(true) && !address.isNull()) return true;
  return false;
}

void Tone3000Session::probeSecureConnection(std::function<void(Probe)> reply) {
  HttpRequest request;
  request.url = juce::URL(config_.apiOrigin);
  request.method = "HEAD";
  request.timeoutMs = kProbeTimeoutMs;
  http_.send(std::move(request), scope_.wrap([cb = std::move(reply)](HttpResponse r) {
    // Any response means the handshake completed. No response after the
    // full timeout is a slow network, not evidence about TLS; a quick
    // failure is the network layer refusing (DNS, refused, TLS).
    if (!r.failed()) return cb(Probe::ok);
    cb(r.elapsedMs >= kProbeTimeoutMs - 500 ? Probe::inconclusive : Probe::insecure);
  }));
}

void Tone3000Session::fetchPluginVersion(Reply<juce::var> reply) {
  client_.fetchPluginVersion(backend_.uniqueDeviceId(), backend_.pluginVersion(), std::move(reply));
}

}  // namespace t3k::ui
