// The in-editor TONE3000 browser.
//
// The hosted OAuth Select flow (tone3000.com's own sign-in, catalog search,
// filters and tone picker) runs inside the plugin window instead of the
// system browser. The flow is unchanged: the editor loads `authorizeUrl`,
// TONE3000 returns to the loopback listener, and the existing code exchange
// completes. The one difference is that the redirect navigation is caught
// before WebKit follows it and replayed from the app, so the webview never
// needs to reach 127.0.0.1 itself (localhost may resolve to ::1, where the
// listener is not bound).
//
// Builds with JUCE_WEB_BROWSER=0 (the UI testbed, and any platform without a
// webview) compile the panel as a notice; the sign-in screen's copy-link and
// device-code fallbacks remain the way through there.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

#include "widgets/PillButton.h"

namespace t3k::ui {

// The redirect the hosted flow sends back to the loopback listener. Kept as
// pure functions so the interception can be tested without a webview.
namespace authbrowser {
inline bool isRedirectNavigation(const juce::String& url, const juce::String& redirectUri) {
  return redirectUri.isNotEmpty() && url.startsWith(redirectUri);
}
// The app replays the callback itself. `localhost` may resolve to ::1, where
// the listener is not bound, so the replay pins 127.0.0.1.
inline juce::String replayTarget(const juce::String& url) {
  return url.replace("://localhost:", "://127.0.0.1:");
}
// JUCE 9's macOS goToURL runs the whole string through
// stringByAddingPercentEncodingWithAllowedCharacters (URLQueryAllowedCharacterSet),
// which has no '%' in it: an already-escaped URL becomes %25-escaped and
// TONE3000 rejects redirect_uri ("redirect_uri must be a valid URL", received
// "http%3A%2F%2Flocalhost%3A51589%2F"). Hand the webview the decoded form; the
// authorize parameters are URL-safe once decoded (no '&' or '=' inside a value),
// so CoreFoundation re-encodes exactly the URL the system browser used to get.
inline juce::String webviewUrl(const juce::String& url) {
  return juce::URL::removeEscapeChars(url);
}
}  // namespace authbrowser

class AuthBrowserPanel : public juce::Component {
public:
  static constexpr int kPad = 12, kHeaderHeight = 44;

  AuthBrowserPanel(const juce::String& url, const juce::String& redirectUri);
  ~AuthBrowserPanel() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

  // Close: abandon the flow (the owner cancels it).
  std::function<void()> onClose;
  // Escape hatch: continue in the system browser.
  std::function<void()> onOpenExternal;
  // The loopback redirect was seen and replayed; the session is returning.
  std::function<void()> onRedirected;

private:
#if JUCE_WEB_BROWSER
  class Browser;
  std::unique_ptr<Browser> browser_;
#endif
  PillButton openExternal_{"Open in Browser", PillButton::Style::outline};
  PillButton close_{"Close", PillButton::Style::outline};
};

}  // namespace t3k::ui
