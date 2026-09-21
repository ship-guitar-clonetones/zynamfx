// In-plugin tone browser: the Select tone takeover that replaces the signal
// chain between the meters, keeping the faceplate around it. A pinned
// header (← SELECT TONE, the search box, the filter bar) over a scrolling
// two-up card grid (or loading dots / empty copy / an error with Try again)
// and its paginator. The query, filter row and last page live in the
// BrowserState so the screen comes back as it was left.
//
// The whole screen needs a TONE3000 session: signed out it shows only the
// sign-in prompt, and a browse-intent login comes straight back here. Every
// query goes to the TONE3000 API through the session; native is only
// involved for the final load (selectTone), which the parent completes by
// closing the browser.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "BrowserPrompt.h"
#include "FilterBar.h"
#include "Paginator.h"
#include "ToneCard.h"
#include "core/AsyncScope.h"
#include "services/Services.h"
#include "widgets/BackLink.h"
#include "widgets/BusyOverlay.h"
#include "widgets/LoadingDots.h"
#include "widgets/PillButton.h"
#include "widgets/TextField.h"

namespace t3k::ui {

class ToneBrowser : public juce::Component, private ToneSession::Listener {
public:
  // Plugin.tsx's shared 24px pad under the header; Select fills to the
  // faceplate, so the bottom pad lives inside the scroll content.
  static constexpr int kPadTop = 24;
  static constexpr int kColumnWidth = 800;  // CARD_WIDTH: lines up with ← BLOCK
  static constexpr int kPageSize = 12;
  static constexpr int kSearchHeight = 40;

  explicit ToneBrowser(Services& services);
  ~ToneBrowser() override;

  // ← back to the chain.
  std::function<void()> onClose;
  // The sign-in gate: the browse-intent login that comes back here.
  std::function<void()> onSignIn;

  void resized() override;
  void paintOverChildren(juce::Graphics& g) override;

private:
  class Content;
  class Scroller;
  static constexpr int kHeaderGap = 16;  // header row → search → filters
  static constexpr int kContentPadTop = 24, kContentPadBottom = 24;
  static constexpr int kPaginatorGap = 16;  // grid → paginator
  static constexpr int kPickErrorGap = 16;
  static constexpr int kGridGap = 16;
  static constexpr int kDotsPadY = 64;
  static constexpr int kEmptyPadY = 64, kEmptyPadX = 24;
  static constexpr int kCopyMaxWidth = 420, kErrorMaxWidth = 340;

  void sessionChanged() override;
  void authFlowChanged() override;

  bool signedOut() const { return !services_.session.authenticated(); }
  // Pre-mounted while an OAuth return still resolves its code exchange.
  bool authPending() const { return services_.session.authPending(); }
  // The search box's text becomes the query (Enter, ×, Escape).
  void submit();
  void queryChanged();
  void setPage(int page);
  void fetch();
  void pageLoaded(TonePage page);
  void pageFailed();
  void pick(const Tone& tone);
  void rebuildCards();
  void rebuildBody();
  void layoutContent();
  const char* emptyCopy() const;

  Services& services_;
  BrowserState& state_;
  AsyncScope scope_;       // the component's lifetime (picks)
  AsyncScope fetchScope_;  // the current page request

  // This visit's state; the rest is in state_.
  bool loading_ = true;
  bool error_ = false;
  std::optional<int> pickingId_;
  juce::String pickError_;

  // Pinned header
  BackLink back_;
  TextField search_;
  FilterBar filters_;

  // Scrolling content
  std::unique_ptr<Scroller> scroller_;
  std::unique_ptr<Content> content_;
  std::vector<std::unique_ptr<ToneCard>> cards_;
  std::unique_ptr<BusyOverlay> gridBusy_;
  std::unique_ptr<BrowserPrompt> bodyPrompt_;  // the sign-in gate, or the fetch error
  LoadingDots dots_;
  Paginator paginator_;
};

}  // namespace t3k::ui
