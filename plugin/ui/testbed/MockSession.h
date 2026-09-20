// Fixture-driven ToneSession for the testbed: the native twin of the
// Playwright suite's tone3000.com route handler (capture.mjs handleApi).
// Signed in when the scenario says `auth`, answers the catalog calls from
// `apiTones`, and honours the scenario's per-endpoint `api` overrides
// ('stall' never replies, 'error' fails, a payload is returned as is).
// Replies are posted, never inline, so views see the same asynchrony the
// network gives them.
#pragma once

#include "services/ToneSession.h"

namespace t3k::ui::testbed {

class MockSession : public ToneSession {
public:
  MockSession(const juce::var& scenario, const juce::var& fixtures);
  ~MockSession() override;

  bool authenticated() const override { return authenticated_; }
  std::optional<User> user() const override;

  void getTone(int toneId, Reply<Tone> reply) override;
  void listToneModels(int toneId, const juce::String& format,
                      Reply<std::vector<Model>> reply) override;
  void setToneFavorite(int toneId, bool favorite, Done done) override;
  // Trending answers from `apiTones` (`api.trending`), the gated streams
  // with the suite's 3-page slice of them (`api.gated`).
  void listTrending(const juce::String& gear, Reply<std::vector<Tone>> reply) override;
  void listStream(Stream stream, int page, int pageSize, const juce::String& gear, Reply<TonePage> reply) override;
  // The tone (`api.tone`) with its first model, then onToneSelected.
  void selectTone(int toneId, Done done) override;
  void ensureNativeAuth(Done done) override;

  // Login honours `api.authorize: 'stall'` (the browser never comes back:
  // the scrim stays up). The initial phase comes from the suite's `query`:
  // `?code=` lands as returning (with `api.token: 'stall'` it stays there),
  // `?t3k-nav-error=1` as the failed-navigation error, and `?canceled=true`
  // with a `browse` login intent lands in the tone browser.
  void login(LoginIntent intent) override;
  void startSelectFlow() override { login(LoginIntent::plain); }
  void logout() override;
  const AuthFlow& authFlow() const override { return flow_; }
  void retryFlow() override { login(LoginIntent::plain); }
  void cancelFlow() override;
  void clearAuthError() override;

  // `offlineAfterLoad` cuts the network; probes never accuse anyone.
  bool online() const override { return !offline_; }
  void probeSecureConnection(std::function<void(Probe)> reply) override;
  void fetchPluginVersion(Reply<juce::var> reply) override;

private:
  // The fixture tone by id, else the first one (the suite's fallback).
  juce::var apiTone(int toneId) const;
  // Override for an endpoint group: void when none.
  juce::var override(const char* group) const;
  template <typename T>
  void answer(const char* group, Reply<T> reply, std::function<Result<T>()> fallback);

  void setFlow(AuthFlow::Phase phase, juce::String error = {});
  std::vector<Tone> tonesOf(const juce::var& rows) const;

  JUCE_DECLARE_WEAK_REFERENCEABLE(MockSession)

  bool authenticated_;
  bool offline_;
  juce::var gatedPage_;
  AuthFlow flow_;
  juce::var user_;
  juce::var apiTones_;
  juce::var api_;
};

}  // namespace t3k::ui::testbed
