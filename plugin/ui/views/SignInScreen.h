// The sign-in takeover (Select Tone mockup 13301:49740): the page the
// plugin shows from the moment a sign-in starts until the session lands,
// whichever CTA started it (the account menu lands back on the chain, the
// tone browser's on the browser). It covers everything under the header as
// the tone browser does: a bare ← top-left that abandons the sign-in, and
// in the middle the loading dots over "Sign in on your browser then return
// here."
//
// The browser is opened best effort, and there is no telling from here
// whether it came up (when it did, it covers the plugin anyway), so the
// page always offers the two ways round it under the copy, both fed by the
// session's AuthFlow:
//   Copy link       the authorize URL on the clipboard, to paste into any
//                   browser on this machine (the loopback redirect works
//                   from whichever browser completes it)
//   Use your phone  the device flow: a QR code for the phone's camera, and
//                   the code to type at tone3000.com/activate for anyone
//                   who can't scan, polled until approved or expired
// The browser path stays live throughout, so finishing in the browser
// still works from the phone page. A failed flow shows its reason with Try
// again / Dismiss in place of the dots.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "core/DelayedCall.h"
#include "core/TextFlow.h"
#include "services/Services.h"
#include "widgets/BackLink.h"
#include "widgets/LoadingDots.h"
#include "widgets/PillButton.h"
#include "widgets/QrCode.h"

namespace t3k::ui {

class SignInScreen : public juce::Component, private ToneSession::Listener {
public:
  // The ← row sits where the tone browser's does.
  static constexpr int kPadTop = 24, kPadX = 32;
  // How long Copy link reads "Copied".
  static constexpr int kCopiedMs = 2000;
  static constexpr int kQrSize = 140;
  static constexpr int kCopyMaxWidth = 256, kWideMaxWidth = 400;
  static constexpr const char* kDefaultError = "Something went wrong completing TONE3000 sign-in.";

  explicit SignInScreen(Services& services);
  ~SignInScreen() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  // One centred row of the column: a component, a text block, or the
  // pill buttons side by side.
  struct Text {
    std::unique_ptr<TextFlow> flow;
    juce::Colour colour;
    int maxWidth;
    juce::Rectangle<float> box;
  };
  struct Row {
    int gapBefore, height, width;
    juce::Component* component = nullptr;
    int text = -1;  // index into texts_
    std::vector<PillButton*> buttons;
  };

  void sessionChanged() override {}
  void authFlowChanged() override;
  // Rebuild the column for the flow's state.
  void rebuild();
  void layoutColumn();
  void back();
  void copyLink();
  Row& addComponent(juce::Component& c, int gapBefore);
  Row& addText(const juce::String& text, const juce::Font& font, juce::Colour colour, int maxWidth, int gapBefore);
  Row& addButtons(std::vector<PillButton*> buttons, int gapBefore);
  // The verification URI as a person types it ("tone3000.com/activate").
  static juce::String typedUri(const juce::String& uri);

  Services& services_;
  BackLink back_;
  LoadingDots dots_;
  QrCode qr_;
  PillButton copyLink_, phone_, newCode_, retry_, dismiss_;
  std::vector<Text> texts_;
  std::vector<Row> rows_;
  int columnHeight_ = 0;
  DelayedCall copiedDelay_;
  juce::String announced_;
};

}  // namespace t3k::ui
