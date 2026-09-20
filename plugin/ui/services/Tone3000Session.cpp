#include "Tone3000Session.h"

#include <juce_events/juce_events.h>

#include "T3kConfig.h"

namespace t3k::ui {

namespace {
constexpr const char* kNoKeyMessage =
    "TONE3000 publishable key not configured. Set VITE_T3K_PUBLISHABLE_KEY at build time.";
constexpr const char* kBrowserFailed = "Could not open your browser to reach TONE3000.";
constexpr const char* kLoopbackFailed = "Could not start the local sign-in listener. Try again.";
// Only the first model: native stores and loads the active model; the
// detail card pages the full catalog separately.
constexpr int kFirstModelOnly = 1;
// Tones max out at 300 models, so one page covers the picker.
constexpr int kAllModels = 300;

std::vector<Tone> parseTones(const juce::var& rows) {
  std::vector<Tone> tones;
  if (const auto* arr = rows.getArray())
    for (const auto& t : *arr) tones.push_back(Tone::parse(t));
  return tones;
}
}  // namespace

Tone3000Session::Config Tone3000Session::Config::fromBuild() {
  return {config::kApiOrigin, config::kPublishableKey, config::kArchitecture, config::kPreviewPlayersEnabled};
}

Tone3000Session::Tone3000Session(Backend& backend, UiPrefs& prefs, HttpTransport& http, Config config)
    : backend_(backend),
      prefs_(prefs),
      http_(http),
      config_(std::move(config)),
      client_(http, prefs, config_.apiOrigin, config_.publishableKey) {
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
  if (authenticated()) {
    ensureNativeAuth({});
    refreshUser();
  }
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
  lastFlow_.reset();
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

void Tone3000Session::listTrending(const juce::String& gear, Reply<std::vector<Tone>> reply) {
  client_.listTrending(gear, [cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<std::vector<Tone>>::fail(r.error));
    cb(Result<std::vector<Tone>>::ok(parseTones((*r)["data"])));
  });
}

void Tone3000Session::listStream(Stream stream, int page, int pageSize, const juce::String& gear,
                                 Reply<TonePage> reply) {
  client_.listTones(streamId(stream), page, pageSize, gear, [cb = std::move(reply)](Result<juce::var> r) {
    if (!r) return cb(Result<TonePage>::fail(r.error));
    TonePage out;
    out.data = parseTones((*r)["data"]);
    out.page = static_cast<int>((*r).getProperty("page", 1));
    out.totalPages = static_cast<int>((*r).getProperty("total_pages", 1));
    cb(Result<TonePage>::ok(std::move(out)));
  });
}

void Tone3000Session::fetchToneAndModels(int toneId, Reply<Tone> reply) {
  getTone(toneId, [this, toneId, cb = std::move(reply)](Result<Tone> tone) {
    if (!tone) return cb(std::move(tone));
    // Only NAM tones take the architecture filter; IR and other formats
    // are not NAM architectures.
    const int architecture = tone->isNam() ? config_.architecture : -1;
    client_.listModels(toneId, kFirstModelOnly, architecture, [found = *tone, cb](Result<juce::var> models) {
      if (!models) return cb(Result<Tone>::fail(models.error));
      std::vector<Model> rows;
      if (const auto* arr = (*models)["data"].getArray())
        for (const auto& m : *arr) rows.push_back(Model::parse(m));
      cb(Result<Tone>::ok(found.withModels(std::move(rows))));
    });
  });
}

void Tone3000Session::finishSelection(const Tone& tone) {
  if (onToneSelected) onToneSelected(tone);
}

void Tone3000Session::selectTone(int toneId, Done done) {
  // Both in flight together, like the web's Promise.all: the tone with its
  // first model, and a fresh token pushed to native before the load starts.
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
    finishSelection(*pending->tone);
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
void Tone3000Session::setFlow(AuthFlow::Phase phase, juce::String error) {
  flow_ = {phase, std::move(error)};
  notifyAuthFlowChanged();
}

void Tone3000Session::clearAuthError() { setFlow(AuthFlow::Phase::idle); }

void Tone3000Session::leave(Flow flow, juce::StringPairArray extra) {
  if (config_.publishableKey.isEmpty()) {
    setFlow(AuthFlow::Phase::error, kNoKeyMessage);
    return;
  }
  lastFlow_ = flow;
  browseIntent_ = flow != Flow::login;
  // Dim the plugin at once; the browser takes a beat to come up.
  setFlow(AuthFlow::Phase::leaving);
  if (!loopback_.start()) {
    setFlow(AuthFlow::Phase::error, kLoopbackFailed);
    return;
  }
  pkce_ = oauth::Pkce::generate();
  redirectUri_ = loopback_.redirectUri();
  extra.set("menubar", "true");
  const auto url = oauth::authorizeUrl(config_.apiOrigin, config_.publishableKey, redirectUri_, *pkce_, extra);
  if (!juce::URL(url).launchInDefaultBrowser()) {
    loopback_.stop();
    setFlow(AuthFlow::Phase::error, kBrowserFailed);
  }
}

void Tone3000Session::startSelectFlow() {
  juce::StringPairArray extra;
  extra.set("prompt", "select_tone");
  if (config_.architecture >= 0) extra.set("architecture", juce::String(config_.architecture));
  if (config_.preview) extra.set("preview", "true");
  leave(Flow::select, std::move(extra));
}

void Tone3000Session::login(LoginIntent intent) {
  leave(intent == LoginIntent::browse ? Flow::loginBrowse : Flow::login, {});
}

void Tone3000Session::retryFlow() {
  // Defaults to Select when nothing is recorded, as the web did.
  switch (lastFlow_.value_or(Flow::select)) {
    case Flow::login: return login(LoginIntent::plain);
    case Flow::loginBrowse: return login(LoginIntent::browse);
    case Flow::select: return startSelectFlow();
  }
}

void Tone3000Session::cancelFlow() {
  loopback_.stop();
  pkce_.reset();
  if (flow_.phase == AuthFlow::Phase::leaving || flow_.phase == AuthFlow::Phase::returning)
    setFlow(AuthFlow::Phase::idle);
}

void Tone3000Session::handleCallback(const juce::String& query) {
  // A callback after cancel (or a stale tab) has no PKCE state: ignore it.
  if (!pkce_ || flow_.phase != AuthFlow::Phase::leaving) return;
  const auto pkce = *pkce_;
  pkce_.reset();  // single use
  loopback_.stop();
  const bool wantsBrowser = browseIntent_;
  browseIntent_ = false;

  const auto cb = oauth::Callback::parse(query, pkce.state);
  switch (cb.kind) {
    case oauth::Callback::Kind::canceled:
      setFlow(AuthFlow::Phase::idle);
      return;
    case oauth::Callback::Kind::error:
      setFlow(AuthFlow::Phase::error, cb.error);
      return;
    case oauth::Callback::Kind::code:
      break;
  }
  setFlow(AuthFlow::Phase::returning);
  client_.exchangeCode(cb.code, pkce.verifier, redirectUri_, scope_.wrap([this, cb, wantsBrowser](Result<Tokens> tokens) {
    if (flow_.phase != AuthFlow::Phase::returning) return;  // cancelled meanwhile
    if (!tokens) {
      setFlow(AuthFlow::Phase::error, tokens.error);
      return;
    }
    client_.setTokens(*tokens);
    notifySessionChanged();
    refreshUser();
    if (cb.toneId.isNotEmpty()) {
      fetchToneAndModels(cb.toneId.getIntValue(), scope_.wrap([this](Result<Tone> tone) {
        if (flow_.phase != AuthFlow::Phase::returning) return;
        if (!tone) {
          setFlow(AuthFlow::Phase::error, tone.error);
          return;
        }
        finishSelection(*tone);
        setFlow(AuthFlow::Phase::idle);
      }));
      return;
    }
    if (wantsBrowser && onAuthenticated) onAuthenticated();
    setFlow(AuthFlow::Phase::idle);
  }));
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
  client_.fetchPluginVersion(backend_.uniqueDeviceId(), std::move(reply));
}

}  // namespace t3k::ui
