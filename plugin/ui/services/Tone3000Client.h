// TONE3000 OAuth token store + authenticated API client (port of
// ui/src/t3k/tone3000-client.ts T3KClient). Tokens persist in UiPrefs under
// the same key the webview used in localStorage, so a signed-in user stays
// signed in across editor sessions; the access token refreshes
// transparently (proactively within 60 s of expiry, and once more after a
// stray 401). Every reply lands on the message thread.
#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <optional>
#include <vector>

#include "HttpClient.h"
#include "UiPrefs.h"
#include "core/Result.h"
#include "model/ToneQuery.h"

namespace t3k::ui {

struct Tokens {
  juce::String access;
  juce::String refresh;
  juce::int64 expiresAtMs = 0;  // Unix ms

  static std::optional<Tokens> fromVar(const juce::var& v);
  juce::var toVar() const;
  // The /oauth/token payload ({access_token, refresh_token, expires_in}).
  static std::optional<Tokens> fromTokenResponse(const juce::var& v, juce::int64 nowMs);
};

class Tone3000Client {
public:
  // Errors carry the web client's codes / messages ("token_refresh_failed",
  // "getTone failed: 404").
  template <typename T>
  using Reply = std::function<void(Result<T>)>;

  static constexpr int kRefreshLeadMs = 60'000;

  Tone3000Client(HttpTransport& http, UiPrefs& prefs, juce::String apiOrigin, juce::String publishableKey);

  // Fires for setTokens() and every automatic refresh (native's Bearer
  // copy follows it).
  std::function<void(const Tokens&)> onTokensUpdated;
  // The refresh token was rejected (or there was none): the tokens are gone
  // and the user has to sign in again.
  std::function<void()> onAuthRequired;
  // Clock, replaceable by tests.
  std::function<juce::int64()> now = [] { return juce::Time::currentTimeMillis(); };

  bool authenticated() const { return tokens().has_value(); }
  std::optional<Tokens> tokens() const;
  void setTokens(const Tokens& tokens);
  void clearTokens();

  // A valid access token, refreshing first when it is near expiry; concurrent
  // callers share one in-flight refresh.
  void getAccessToken(Reply<juce::String> reply);

  // OAuth token endpoint
  void exchangeCode(const juce::String& code, const juce::String& codeVerifier, const juce::String& redirectUri,
                    Reply<Tokens> reply);

  // Authenticated calls
  // Bearer fetch with one retry on 401 (the expiry-check race).
  void fetch(const juce::String& path, const juce::String& method, const juce::String& jsonBody,
             Reply<HttpResponse> reply);
  // Bearer when a session exists, anonymous otherwise (and anonymous again
  // when the session cannot be refreshed): the version check.
  void fetchOptionalAuth(const juce::String& path, juce::StringPairArray headers, Reply<HttpResponse> reply);

  // Typed endpoints
  void getUser(Reply<juce::var> reply);
  void getTone(int toneId, Reply<juce::var> reply);
  void setFavorite(int toneId, bool favorite, Reply<bool> reply);
  // A tone listing by its ready-made path (ToneQuery::requestPath): the
  // PaginatedResponse payload.
  void listTones(const juce::String& path, Reply<juce::var> reply);
  // /tags, /makes or /users (creators), most-used first, one page of
  // `pageSize`, narrowed by `query` when non-empty.
  void listTaxonomy(Taxonomy kind, const juce::String& query, int pageSize, Reply<juce::var> reply);
  // /models?tone_id&page_size[&architecture]; architecture < 0 omits it.
  void listModels(int toneId, int pageSize, int architecture, Reply<juce::var> reply);
  void fetchPluginVersion(const juce::String& deviceId, Reply<juce::var> reply);

private:
  void refresh(const juce::String& refreshToken);
  void postTokenForm(const juce::StringPairArray& form, Reply<Tokens> reply);
  void bearerRequest(const juce::String& path, const juce::String& method, const juce::String& jsonBody,
                     const juce::String& token, std::function<void(HttpResponse)> onDone);
  void anonymousRequest(const juce::String& path, const juce::StringPairArray& headers,
                        std::function<void(HttpResponse)> onDone);
  // GET expecting a JSON body; non-2xx → "<label> failed: <status>".
  void getJson(const juce::String& path, juce::String label, Reply<juce::var> reply);

  HttpTransport& http_;
  UiPrefs& prefs_;
  juce::String origin_;
  juce::String key_;
  std::vector<Reply<juce::String>> refreshWaiters_;

  JUCE_DECLARE_WEAK_REFERENCEABLE(Tone3000Client)
};

}  // namespace t3k::ui
