// In-plugin tone browser (port of ToneBrowser.tsx): the takeover that
// replaces the signal chain between the meters, keeping the faceplate
// around it. A pinned header (← SELECT TONE, the Browse CTA, the stream
// tabs) over a scrolling column of gear filter pills, the two-up card grid
// (or a sign-in prompt / loading dots / empty copy / error), the paginator
// and Trending's footer.
//
// Trending is public; Recently used / Favorites / Created need a session
// and show the sign-in prompt while signed out. Resolving a picked tone
// always needs a session too, so a Trending card tapped while signed out
// routes into sign-in. The only OAuth exits are the sign-in CTAs (no-prompt
// login, reopens this browser) and Browse (always the full-catalog Select
// flow); the parent gates both on the connection. All queries go straight
// to the TONE3000 API; native is only involved for the final load.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "BrowserPrompt.h"
#include "GearFilterRow.h"
#include "Paginator.h"
#include "StreamTabs.h"
#include "ToneCard.h"
#include "core/AsyncScope.h"
#include "services/Services.h"
#include "widgets/BackLink.h"
#include "widgets/BusyOverlay.h"
#include "widgets/LoadingDots.h"
#include "widgets/PillButton.h"

namespace t3k::ui {

class ToneBrowser : public juce::Component, private ToneSession::Listener {
public:
  // Plugin.tsx's shared 24px pad under the header; Select fills to the
  // faceplate, so the bottom pad lives inside the scroll content.
  static constexpr int kPadTop = 24;
  static constexpr int kColumnWidth = 800;  // CARD_WIDTH: lines up with ← BLOCK
  static constexpr int kPageSize = 12;

  explicit ToneBrowser(Services& services);
  ~ToneBrowser() override;

  // ← back to the chain.
  std::function<void()> onClose;
  // Browse: leave for the Select catalog.
  std::function<void()> onBrowseTone3000;
  // Any sign-in CTA: the no-prompt login that comes back to this browser.
  std::function<void()> onSignIn;

  void resized() override;

private:
  class Content;
  class Scroller;
  static constexpr int kHeaderGap = 16;     // header row → tabs
  static constexpr int kBrowseHeight = 40;  // the prominent Browse pill
  static constexpr int kContentPadTop = 20, kContentPadBottom = 24;
  static constexpr int kBodyGap = 24;      // pills → grid
  static constexpr int kPaginatorGap = 16;  // grid → paginator
  static constexpr int kPickErrorGap = 16;
  static constexpr int kGridGap = 16;
  static constexpr int kDotsPadY = 64;
  static constexpr int kEmptyPadY = 64, kEmptyPadX = 24;
  static constexpr int kCopyMaxWidth = 420, kErrorMaxWidth = 340;

  static std::unique_ptr<PillButton> makeBrowseButton();
  static std::unique_ptr<PillButton> makeFilledButton(const juce::String& label);

  void sessionChanged() override;
  void authFlowChanged() override;

  bool showSignInPrompt() const;
  // Pre-mounted while an OAuth return still resolves its code exchange.
  bool authPending() const;
  void switchStream(Stream next);
  void setGearFilter(const juce::String& gear);
  void setPage(int page);
  void fetch();
  void streamLoaded(std::vector<Tone> tones, std::optional<int> page, std::optional<int> totalPages);
  void streamFailed();
  void pick(const Tone& tone);
  void rebuildCards();
  void rebuildBody();
  void layoutContent();

  Services& services_;
  AsyncScope scope_;       // the component's lifetime (picks)
  AsyncScope fetchScope_;  // the current stream request (the effect's cancel flag)

  // State (ToneBrowser.tsx's useState)
  Stream stream_ = Stream::trending;
  juce::String gear_;
  int page_ = 1;
  struct StreamResult {
    std::vector<Tone> data;
    std::optional<int> page, totalPages;
  };
  std::optional<StreamResult> result_;
  bool loading_ = true;
  bool error_ = false;
  std::optional<int> pickingId_;
  juce::String pickError_;

  // Pinned header
  BackLink back_;
  std::unique_ptr<PillButton> browse_;
  StreamTabs tabs_;

  // Scrolling content
  std::unique_ptr<Scroller> scroller_;
  std::unique_ptr<Content> content_;
  GearFilterRow gearRow_;
  std::vector<std::unique_ptr<ToneCard>> cards_;
  std::unique_ptr<BusyOverlay> gridBusy_;
  std::unique_ptr<BrowserPrompt> bodyPrompt_;  // gated sign-in, or the stream error
  LoadingDots dots_;
  Paginator paginator_;
  // Trending's footer: Browse again (signed in) or the discovery sign-in.
  std::unique_ptr<PillButton> footerBrowse_;
  std::unique_ptr<BrowserPrompt> footerPrompt_;
};

}  // namespace t3k::ui
