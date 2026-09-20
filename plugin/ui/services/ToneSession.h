// The TONE3000 session as the views see it (port of useToneSession.ts +
// useT3kSelect.ts): who is signed in, the catalog calls the detail card and
// the tone browser make, and the OAuth flows. Replies land on the message
// thread; callers wrap them in an AsyncScope so a swap mid-flight or a
// closed card never hears back.
//
// Implementations: Tone3000Session (HTTP client, token store, loopback OAuth
// redirect) in the plugin, MockSession in the testbed, and SignedOutSession
// below when no session backing exists.
#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <optional>
#include <vector>

#include "core/Result.h"
#include "model/Tone.h"

namespace t3k::ui {

// The tone browser's streams (ToneBrowser.tsx StreamKind). Trending is a
// public top-10 feed; the other three page through the signed-in user's
// tones and need a session.
enum class Stream { trending, downloaded, favorited, created };
inline constexpr int kStreamCount = 4;
inline bool streamIsGated(Stream s) { return s != Stream::trending; }
// The API endpoint / localStorage id ("trending", "downloaded", …).
inline const char* streamId(Stream s) {
  switch (s) {
    case Stream::trending: return "trending";
    case Stream::downloaded: return "downloaded";
    case Stream::favorited: return "favorited";
    case Stream::created: return "created";
  }
  return "trending";
}
inline std::optional<Stream> streamFromId(const juce::String& id) {
  for (int i = 0; i < kStreamCount; ++i)
    if (id == streamId(static_cast<Stream>(i))) return static_cast<Stream>(i);
  return std::nullopt;
}

// One page of a paginated stream.
struct TonePage {
  std::vector<Tone> data;
  int page = 1;
  int totalPages = 1;
};

class ToneSession {
public:
  struct Listener {
    virtual ~Listener() = default;
    // Signed in / out, or the identity refreshed.
    virtual void sessionChanged() = 0;
    // The OAuth flow moved phase (see AuthFlow).
    virtual void authFlowChanged() {}
  };

  // Where an OAuth redirect stands (useT3kSelect's OAuthPhase). The overlay
  // dims the whole plugin while one is in flight:
  //   idle       nothing going on
  //   leaving    the browser is being sent to tone3000.com
  //   returning  back with an authorization code; tokens and the picked tone
  //              are being resolved
  //   error      the flow failed; `error` is the user-facing reason, and
  //              retryFlow() restarts whichever flow it was
  struct AuthFlow {
    enum class Phase { idle, leaving, returning, error };
    Phase phase = Phase::idle;
    juce::String error;
  };

  template <typename T>
  using Reply = std::function<void(Result<T>)>;
  // "" = success.
  using Done = std::function<void(const juce::String& error)>;

  // Why the user is being sent to sign in: `browse` (the + / swap flows and
  // the browser's own sign-in CTAs) reopens the tone browser on return; a
  // plain sign-in (account menu, info panel) lands on the main screen.
  enum class LoginIntent { plain, browse };

  virtual ~ToneSession() = default;

  void addListener(Listener* l) { listeners_.add(l); }
  void removeListener(Listener* l) { listeners_.remove(l); }

  // A fully resolved tone (first model embedded) landed: from the Select
  // flow's callback or selectTone(). Native already holds a fresh access
  // token when this fires. Owned by the load flow.
  std::function<void(const Tone& tone)> onToneSelected;
  // A browse-intent flow finished without a pick (signed in, or closed the
  // catalog): the caller opens the tone browser.
  std::function<void()> onAuthenticated;

  virtual bool authenticated() const = 0;
  virtual std::optional<User> user() const = 0;
  // An OAuth return is still being resolved (the token exchange is in
  // flight): the browser holds its fetch until this clears.
  bool authPending() const { return authFlow().phase == AuthFlow::Phase::returning; }

  // Catalog
  // GET /tones/{id}: the full tone (description, makes, tags, url, models).
  virtual void getTone(int toneId, Reply<Tone> reply) = 0;
  // GET /models?tone_id=: every model of the tone (NAM keeps the A2
  // architecture filter, which is all the plugin loads).
  virtual void listToneModels(int toneId, const juce::String& format,
                              Reply<std::vector<Model>> reply) = 0;
  // PUT / DELETE /tones/{id}/favorite (idempotent).
  virtual void setToneFavorite(int toneId, bool favorite, Done done) = 0;
  // GET /tones/trending: the public top-10 feed, optionally one gear type;
  // the Bearer rides along when signed in, and a dead session degrades to
  // the anonymous request instead of failing.
  virtual void listTrending(const juce::String& gear, Reply<std::vector<Tone>> reply) = 0;
  // GET /tones/{downloaded|favorited|created}: one page of a gated stream.
  virtual void listStream(Stream stream, int page, int pageSize, const juce::String& gear,
                          Reply<TonePage> reply) = 0;
  // Resolve a tone picked in the browser like a Select callback: the tone,
  // its first loadable model and a fresh token for native, then
  // onToneSelected. `done` reports failure (the card shows a pick error).
  virtual void selectTone(int toneId, Done done) = 0;

  // Native downloads
  // Native fetches model files itself with a Bearer header. Guarantee it
  // holds a fresh token before a download starts (refreshing near expiry);
  // an error means the session expired and the tokens were cleared.
  virtual void ensureNativeAuth(Done done) = 0;

  // Flows
  // Login-only OAuth flow (no prompt): sign in on tone3000.com and come
  // straight back.
  virtual void login(LoginIntent intent = LoginIntent::plain) = 0;
  // The Select flow (prompt=select_tone): browse the full catalog on
  // tone3000.com; the callback carries the picked tone. Always a browse.
  virtual void startSelectFlow() = 0;
  virtual void logout() = 0;
  virtual const AuthFlow& authFlow() const = 0;
  // Restart whichever flow last left for tone3000.com (the error overlay's
  // Try again).
  virtual void retryFlow() = 0;
  // Give up waiting for the system browser to come back (the leaving
  // overlay's Cancel): stop listening and return to idle.
  virtual void cancelFlow() = 0;
  // Drop the error without restarting.
  virtual void clearAuthError() = 0;

  // Reachability (useConnectionGate.ts)
  // Instant OS-level check: false means no network interface is up at all.
  // True doesn't promise the internet is reachable; the recovery paths do.
  virtual bool online() const = 0;
  // One HTTPS reachability probe of the TONE3000 origin, replying on the
  // message thread. Only the TLS handshake matters, not the response:
  //   ok           the handshake completed
  //   insecure     it failed at the network layer (DNS, refused, TLS)
  //   inconclusive timed out or errored oddly; no evidence either way
  enum class Probe { ok, insecure, inconclusive };
  virtual void probeSecureConnection(std::function<void(Probe)> reply) = 0;

  // Update check (useUpdateNotice.ts)
  // GET /plugin/version, with the Bearer when signed in (beta payloads) and
  // X-Device-Id; the raw JSON body, validated by UpdateCheck.
  virtual void fetchPluginVersion(Reply<juce::var> reply) = 0;

protected:
  void notifySessionChanged() {
    listeners_.call([](Listener& l) { l.sessionChanged(); });
  }
  void notifyAuthFlowChanged() {
    listeners_.call([](Listener& l) { l.authFlowChanged(); });
  }

private:
  juce::ListenerList<Listener> listeners_;
};

// No TONE3000 backing at all: signed out, every catalog call fails, so the
// UI behaves as it does with the network unreachable. A stand-in for hosts
// without a session (tests, tooling); the plugin runs Tone3000Session.
class SignedOutSession final : public ToneSession {
public:
  bool authenticated() const override { return false; }
  std::optional<User> user() const override { return std::nullopt; }
  void getTone(int, Reply<Tone> reply) override { reply(Result<Tone>::fail(kNotSignedIn)); }
  void listToneModels(int, const juce::String&, Reply<std::vector<Model>> reply) override {
    reply(Result<std::vector<Model>>::fail(kNotSignedIn));
  }
  void setToneFavorite(int, bool, Done done) override { done(kNotSignedIn); }
  void listTrending(const juce::String&, Reply<std::vector<Tone>> reply) override {
    reply(Result<std::vector<Tone>>::fail(kNotSignedIn));
  }
  void listStream(Stream, int, int, const juce::String&, Reply<TonePage> reply) override {
    reply(Result<TonePage>::fail(kNotSignedIn));
  }
  void selectTone(int, Done done) override { done(kNotSignedIn); }
  void ensureNativeAuth(Done done) override { done(kNotSignedIn); }
  void login(LoginIntent) override {}
  void startSelectFlow() override {}
  void logout() override {}
  const AuthFlow& authFlow() const override { return flow_; }
  void retryFlow() override {}
  void cancelFlow() override {}
  void clearAuthError() override {}
  bool online() const override { return true; }
  void probeSecureConnection(std::function<void(Probe)> reply) override { reply(Probe::inconclusive); }
  void fetchPluginVersion(Reply<juce::var> reply) override { reply(Result<juce::var>::fail(kNotSignedIn)); }

private:
  static constexpr const char* kNotSignedIn = "Not signed in.";
  AuthFlow flow_;
};

}  // namespace t3k::ui
