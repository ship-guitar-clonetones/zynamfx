#include "AuthBrowserPanel.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include "core/Design.h"
#include "core/Fonts.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr float kCorner = 10.0f;
constexpr int kButtonGap = 8;
}  // namespace

#if JUCE_WEB_BROWSER
// The webview itself. Everything the hosted page does is left alone; only
// the redirect back to the loopback listener is intercepted, because that
// hop exists to hand us the authorization code, not to render in the panel.
class AuthBrowserPanel::Browser final : public juce::WebBrowserComponent {
public:
  Browser(const juce::String& url, juce::String redirectUri, std::function<void()> onRedirected)
      : redirect_(std::move(redirectUri)), onRedirected_(std::move(onRedirected)) {
    goToURL(url);
  }

  bool pageAboutToLoad(const juce::String& newURL) override {
    if (handled_ || !authbrowser::isRedirectNavigation(newURL, redirect_)) return true;
    handled_ = true;

    const auto target = authbrowser::replayTarget(newURL);
    juce::Thread::launch([target] { juce::URL(target).readEntireTextStream(); });

    juce::Logger::writeToLog("[auth] in-app browser: redirect intercepted, replayed from the app");
    if (onRedirected_) onRedirected_();
    return false;
  }

private:
  juce::String redirect_;
  std::function<void()> onRedirected_;
  bool handled_ = false;
};
#endif

AuthBrowserPanel::AuthBrowserPanel(const juce::String& url, const juce::String& redirectUri) {
  juce::ignoreUnused(redirectUri);
  addAndMakeVisible(openExternal_);
  addAndMakeVisible(close_);
  openExternal_.onClick = [this] {
    if (onOpenExternal) onOpenExternal();
  };
  close_.onClick = [this] {
    if (onClose) onClose();
  };
#if JUCE_WEB_BROWSER
  browser_ = std::make_unique<Browser>(url, redirectUri, [this] {
    if (onRedirected) onRedirected();
  });
  addAndMakeVisible(*browser_);
#endif
}

AuthBrowserPanel::~AuthBrowserPanel() = default;

void AuthBrowserPanel::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat();
  g.setColour(theme::kBlack);
  g.fillRoundedRectangle(r, kCorner);
  g.setColour(theme::kBorder);
  g.drawRoundedRectangle(r.reduced(0.5f), kCorner, 1.0f);

  // Header: the provider's name and what this screen is for.
  g.setFont(Fonts::sans(13.0f, true));
  g.setColour(theme::kWhite);
  g.drawText("TONE3000", design::snap(kPad), 0, 120, kHeaderHeight, juce::Justification::centredLeft);
  g.setFont(Fonts::sans(12.0f));
  g.setColour(theme::kMuted);
  g.drawText("Sign in and choose a tone", design::snap(kPad) + 78, 0, 220, kHeaderHeight,
             juce::Justification::centredLeft);
}

void AuthBrowserPanel::resized() {
  auto r = getLocalBounds().reduced(kPad);
  auto header = r.removeFromTop(kHeaderHeight);
  close_.setTopRightPosition(header.getRight(), header.getY() + (kHeaderHeight - close_.getHeight()) / 2);
  header.removeFromRight(close_.getWidth() + kButtonGap);
  openExternal_.setTopRightPosition(header.getRight(), header.getY() + (kHeaderHeight - openExternal_.getHeight()) / 2);
#if JUCE_WEB_BROWSER
  if (browser_ != nullptr) browser_->setBounds(r.withTrimmedTop(2));
#endif
}

}  // namespace t3k::ui
