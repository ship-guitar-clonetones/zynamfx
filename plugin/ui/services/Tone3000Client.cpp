#include "Tone3000Client.h"

namespace t3k::ui {

// Tokens
std::optional<Tokens> Tokens::fromVar(const juce::var& v) {
  if (!v.isObject()) return std::nullopt;
  Tokens t;
  t.access = v["access_token"].toString();
  t.refresh = v["refresh_token"].toString();
  t.expiresAtMs = static_cast<juce::int64>(v["expires_at"]);
  if (t.access.isEmpty()) return std::nullopt;
  return t;
}

juce::var Tokens::toVar() const {
  auto* o = new juce::DynamicObject();
  o->setProperty("access_token", access);
  o->setProperty("refresh_token", refresh);
  o->setProperty("expires_at", expiresAtMs);
  return juce::var(o);
}

std::optional<Tokens> Tokens::fromTokenResponse(const juce::var& v, juce::int64 nowMs) {
  if (!v.isObject()) return std::nullopt;
  Tokens t;
  t.access = v["access_token"].toString();
  t.refresh = v["refresh_token"].toString();
  t.expiresAtMs = nowMs + static_cast<juce::int64>(static_cast<double>(v["expires_in"]) * 1000.0);
  if (t.access.isEmpty()) return std::nullopt;
  return t;
}

// Client
Tone3000Client::Tone3000Client(HttpTransport& http, UiPrefs& prefs, juce::String apiOrigin,
                               juce::String publishableKey)
    : http_(http), prefs_(prefs), origin_(std::move(apiOrigin)), key_(std::move(publishableKey)) {}

std::optional<Tokens> Tone3000Client::tokens() const { return Tokens::fromVar(prefs_.getJson(UiPrefs::kTokens)); }

void Tone3000Client::setTokens(const Tokens& tokens) {
  prefs_.setJson(UiPrefs::kTokens, tokens.toVar());
  if (onTokensUpdated) onTokensUpdated(tokens);
}

void Tone3000Client::clearTokens() { prefs_.remove(UiPrefs::kTokens); }

void Tone3000Client::getAccessToken(Reply<juce::String> reply) {
  const auto stored = tokens();
  if (!stored) {
    if (onAuthRequired) onAuthRequired();
    reply(Result<juce::String>::fail("not_authenticated"));
    return;
  }
  if (now() <= stored->expiresAtMs - kRefreshLeadMs) {
    reply(Result<juce::String>::ok(stored->access));
    return;
  }
  const bool inFlight = !refreshWaiters_.empty();
  refreshWaiters_.push_back(std::move(reply));
  if (!inFlight) refresh(stored->refresh);
}

void Tone3000Client::refresh(const juce::String& refreshToken) {
  juce::StringPairArray form;
  form.set("grant_type", "refresh_token");
  form.set("refresh_token", refreshToken);
  form.set("client_id", key_);
  postTokenForm(form, [this](Result<Tokens> result) {
    auto waiters = std::move(refreshWaiters_);
    refreshWaiters_.clear();
    if (result) {
      setTokens(*result);
      for (auto& w : waiters) w(Result<juce::String>::ok(result->access));
      return;
    }
    clearTokens();
    if (onAuthRequired) onAuthRequired();
    for (auto& w : waiters) w(Result<juce::String>::fail("token_refresh_failed"));
  });
}

void Tone3000Client::exchangeCode(const juce::String& code, const juce::String& codeVerifier,
                                  const juce::String& redirectUri, Reply<Tokens> reply) {
  juce::StringPairArray form;
  form.set("grant_type", "authorization_code");
  form.set("code", code);
  form.set("code_verifier", codeVerifier);
  form.set("redirect_uri", redirectUri);
  form.set("client_id", key_);
  postTokenForm(form, [cb = std::move(reply)](Result<Tokens> result) {
    // The web reported the server's error code, else token_exchange_failed.
    if (!result && result.error == "token_refresh_failed") result.error = "token_exchange_failed";
    cb(std::move(result));
  });
}

void Tone3000Client::postTokenForm(const juce::StringPairArray& form, Reply<Tokens> reply) {
  HttpRequest request;
  request.url = juce::URL(origin_ + "/api/v1/oauth/token");
  request.method = "POST";
  request.contentType = "application/x-www-form-urlencoded";
  juce::StringArray pairs;
  for (const auto& key : form.getAllKeys())
    pairs.add(juce::URL::addEscapeChars(key, true) + "=" + juce::URL::addEscapeChars(form[key], true));
  request.body = pairs.joinIntoString("&");
  http_.send(std::move(request), [self = juce::WeakReference<Tone3000Client>(this), cb = std::move(reply)](
                                     HttpResponse response) {
    if (self == nullptr) return;
    if (!response.ok()) {
      const auto err = response.json()["error"].toString();
      cb(Result<Tokens>::fail(err.isNotEmpty() ? err : juce::String("token_refresh_failed")));
      return;
    }
    if (auto tokens = Tokens::fromTokenResponse(response.json(), self->now()))
      cb(Result<Tokens>::ok(*tokens));
    else
      cb(Result<Tokens>::fail("token_refresh_failed"));
  });
}

void Tone3000Client::bearerRequest(const juce::String& path, const juce::String& method,
                                   const juce::String& jsonBody, const juce::String& token,
                                   std::function<void(HttpResponse)> onDone) {
  HttpRequest request;
  request.url = juce::URL(origin_ + path);
  request.method = method;
  request.headers.set("Authorization", "Bearer " + token);
  if (jsonBody.isNotEmpty() || method == "PUT") request.contentType = "application/json";
  request.body = jsonBody;
  http_.send(std::move(request), std::move(onDone));
}

void Tone3000Client::anonymousRequest(const juce::String& path, const juce::StringPairArray& headers,
                                      std::function<void(HttpResponse)> onDone) {
  HttpRequest request;
  request.url = juce::URL(origin_ + path);
  request.headers = headers;
  http_.send(std::move(request), std::move(onDone));
}

void Tone3000Client::fetch(const juce::String& path, const juce::String& method, const juce::String& jsonBody,
                           Reply<HttpResponse> reply) {
  getAccessToken([self = juce::WeakReference<Tone3000Client>(this), path, method, jsonBody,
                  cb = std::move(reply)](Result<juce::String> token) mutable {
    if (self == nullptr) return;
    if (!token) {
      cb(Result<HttpResponse>::fail(token.error));
      return;
    }
    self->bearerRequest(path, method, jsonBody, *token, [self, path, method, jsonBody, cb](HttpResponse first) {
      if (self == nullptr) return;
      if (first.status != 401) {
        cb(Result<HttpResponse>::ok(std::move(first)));
        return;
      }
      // Retry once: force a refresh by expiring the stored set.
      auto stored = self->tokens();
      if (!stored) {
        cb(Result<HttpResponse>::ok(std::move(first)));
        return;
      }
      stored->expiresAtMs = 0;
      self->setTokens(*stored);
      self->getAccessToken([self, path, method, jsonBody, cb](Result<juce::String> fresh) {
        if (self == nullptr) return;
        if (!fresh) {
          cb(Result<HttpResponse>::fail(fresh.error));
          return;
        }
        self->bearerRequest(path, method, jsonBody, *fresh, [self, cb](HttpResponse second) {
          if (self != nullptr) cb(Result<HttpResponse>::ok(std::move(second)));
        });
      });
    });
  });
}

void Tone3000Client::fetchOptionalAuth(const juce::String& path, juce::StringPairArray headers,
                                       Reply<HttpResponse> reply) {
  auto anonymous = [self = juce::WeakReference<Tone3000Client>(this), path, headers, reply] {
    if (self == nullptr) return;
    self->anonymousRequest(path, headers, [self, reply](HttpResponse r) {
      if (self != nullptr) reply(Result<HttpResponse>::ok(std::move(r)));
    });
  };
  if (!authenticated()) {
    anonymous();
    return;
  }
  // Extra headers ride on the Bearer request too.
  getAccessToken([self = juce::WeakReference<Tone3000Client>(this), path, headers, reply,
                  anonymous](Result<juce::String> token) {
    if (self == nullptr) return;
    if (!token) {
      anonymous();  // a dead session degrades to the public payload
      return;
    }
    HttpRequest request;
    request.url = juce::URL(self->origin_ + path);
    request.headers = headers;
    request.headers.set("Authorization", "Bearer " + *token);
    self->http_.send(std::move(request), [self, reply](HttpResponse r) {
      if (self != nullptr) reply(Result<HttpResponse>::ok(std::move(r)));
    });
  });
}

void Tone3000Client::getJson(const juce::String& path, const char* label, Reply<juce::var> reply) {
  fetch(path, "GET", {}, [label, cb = std::move(reply)](Result<HttpResponse> r) {
    if (!r) {
      cb(Result<juce::var>::fail(r.error));
      return;
    }
    if (!r->ok()) {
      cb(Result<juce::var>::fail(juce::String(label) + " failed: " + juce::String(r->status)));
      return;
    }
    cb(Result<juce::var>::ok(r->json()));
  });
}

void Tone3000Client::getUser(Reply<juce::var> reply) { getJson("/api/v1/user", "getUser", std::move(reply)); }

void Tone3000Client::getTone(int toneId, Reply<juce::var> reply) {
  getJson("/api/v1/tones/" + juce::String(toneId), "getTone", std::move(reply));
}

void Tone3000Client::setFavorite(int toneId, bool favorite, Reply<bool> reply) {
  const auto label = juce::String(favorite ? "favoriteTone" : "unfavoriteTone");
  fetch("/api/v1/tones/" + juce::String(toneId) + "/favorite", favorite ? "PUT" : "DELETE", {},
        [label, cb = std::move(reply)](Result<HttpResponse> r) {
          if (!r) return cb(Result<bool>::fail(r.error));
          if (!r->ok()) return cb(Result<bool>::fail(label + " failed: " + juce::String(r->status)));
          cb(Result<bool>::ok(true));
        });
}

void Tone3000Client::listTones(const juce::String& endpoint, int page, int pageSize, const juce::String& gear,
                               Reply<juce::var> reply) {
  juce::String path = "/api/v1/tones/" + endpoint + "?page=" + juce::String(page) + "&page_size=" + juce::String(pageSize);
  if (gear.isNotEmpty()) path << "&gear=" << juce::URL::addEscapeChars(gear, true);
  getJson(path, ("list " + endpoint).toRawUTF8(), std::move(reply));
}

void Tone3000Client::listTrending(const juce::String& gear, Reply<juce::var> reply) {
  juce::String path = "/api/v1/tones/trending";
  if (gear.isNotEmpty()) path << "?gear=" << juce::URL::addEscapeChars(gear, true);
  fetchOptionalAuth(path, {}, [cb = std::move(reply)](Result<HttpResponse> r) {
    if (!r) return cb(Result<juce::var>::fail(r.error));
    if (!r->ok()) return cb(Result<juce::var>::fail("listTrendingTones failed: " + juce::String(r->status)));
    cb(Result<juce::var>::ok(r->json()));
  });
}

void Tone3000Client::listModels(int toneId, int pageSize, int architecture, Reply<juce::var> reply) {
  juce::String path = "/api/v1/models?tone_id=" + juce::String(toneId) + "&page=1&page_size=" + juce::String(pageSize);
  if (architecture >= 0) path << "&architecture=" << architecture;
  getJson(path, "listModels", std::move(reply));
}

void Tone3000Client::fetchPluginVersion(const juce::String& deviceId, Reply<juce::var> reply) {
  juce::StringPairArray headers;
  if (deviceId.isNotEmpty()) headers.set("X-Device-Id", deviceId);
  fetchOptionalAuth("/api/v1/plugin/version", headers, [cb = std::move(reply)](Result<HttpResponse> r) {
    if (!r) return cb(Result<juce::var>::fail(r.error));
    if (!r->ok()) return cb(Result<juce::var>::fail("version check failed: " + juce::String(r->status)));
    cb(Result<juce::var>::ok(r->json()));
  });
}

}  // namespace t3k::ui
