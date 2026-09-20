// The plugin's ToneSession (port of useToneSession.ts + useT3kSelect.ts on
// top of Tone3000Client): the signed-in identity, the OAuth flows through
// the system browser with a loopback redirect, and native's copy of the
// access token. The webview's full-page redirect is gone, so the values the
// web parked in sessionStorage across it (login intent, last flow, PKCE)
// are plain fields here.
#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>
#include <optional>

#include "LoopbackServer.h"
#include "OAuth.h"
#include "Tone3000Client.h"
#include "ToneSession.h"
#include "UiPrefs.h"
#include "backend/Backend.h"
#include "core/AsyncScope.h"

namespace t3k::ui {

class Tone3000Session final : public ToneSession {
public:
  struct Config {
    juce::String apiOrigin;
    juce::String publishableKey;
    int architecture = 2;  // NAM model architecture filter; < 0 disables
    bool preview = true;   // tone3000's in-flow preview players

    // The build's ui/.env values (T3kConfig.h).
    static Config fromBuild();
  };

  // useConnectionGate.ts PROBE_TIMEOUT_MS.
  static constexpr int kProbeTimeoutMs = 8000;

  Tone3000Session(Backend& backend, UiPrefs& prefs, HttpTransport& http, Config config);
  ~Tone3000Session() override;


  bool authenticated() const override { return client_.authenticated(); }
  std::optional<User> user() const override;

  void getTone(int toneId, Reply<Tone> reply) override;
  void listToneModels(int toneId, const juce::String& format, Reply<std::vector<Model>> reply) override;
  void setToneFavorite(int toneId, bool favorite, Done done) override;
  void listTrending(const juce::String& gear, Reply<std::vector<Tone>> reply) override;
  void listStream(Stream stream, int page, int pageSize, const juce::String& gear, Reply<TonePage> reply) override;
  void selectTone(int toneId, Done done) override;
  void ensureNativeAuth(Done done) override;

  void login(LoginIntent intent) override;
  void startSelectFlow() override;
  void logout() override;
  const AuthFlow& authFlow() const override { return flow_; }
  void retryFlow() override;
  void cancelFlow() override;
  void clearAuthError() override;

  bool online() const override;
  void probeSecureConnection(std::function<void(Probe)> reply) override;
  void fetchPluginVersion(Reply<juce::var> reply) override;

private:
  enum class Flow { select, login, loginBrowse };

  void setFlow(AuthFlow::Phase phase, juce::String error = {});
  // Open the browser on the authorize URL after starting the loopback
  // listener; `extra` are the flow's query parameters.
  void leave(Flow flow, juce::StringPairArray extra);
  void handleCallback(const juce::String& query);
  // Tone + its first loadable model, as the Select callback resolves them.
  void fetchToneAndModels(int toneId, Reply<Tone> reply);
  void pushToken(const juce::String& token);
  void refreshUser();
  void finishSelection(const Tone& tone);

  Backend& backend_;
  UiPrefs& prefs_;
  HttpTransport& http_;
  Config config_;
  Tone3000Client client_;
  LoopbackServer loopback_;
  AsyncScope scope_;

  AuthFlow flow_;
  std::optional<oauth::Pkce> pkce_;
  juce::String redirectUri_;
  std::optional<Flow> lastFlow_;
  bool browseIntent_ = false;
};

}  // namespace t3k::ui
