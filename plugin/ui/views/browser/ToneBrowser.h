// In-plugin tone browser: the Select tone takeover that replaces the signal
// chain between the meters, keeping the faceplate around it. A pinned
// header (← SELECT TONE, the search box, the filter bar) over a scrolling
// card grid (or loading dots / empty copy / an error with Try again) and
// its paginator. The query, filter row and last page live in the
// BrowserState so the screen comes back as it was left.
//
// The ← SELECT TONE row zooms with the window like everything else; the
// body under it does not. It is counter-scaled by the window zoom and laid
// out in screen pixels, so a bigger window shows more rather than bigger:
// the search box and filter row keep their 1x height and widen with the
// column, the cards keep their 1x height, widen to fill two columns, and go
// three-up once three fit at their default width.
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
#include "widgets/DragScroller.h"
#include "widgets/LoadingDots.h"
#include "widgets/PillButton.h"
#include "widgets/TextField.h"

namespace t3k::ui {

class ToneBrowser : public juce::Component, private ToneSession::Listener, private Zoom::Listener {
public:
  // Plugin.tsx's shared 24px pad under the header; Select fills to the
  // faceplate, so the bottom pad lives inside the scroll content.
  static constexpr int kPadTop = 24;
  static constexpr int kColumnWidth = 800;  // CARD_WIDTH: lines up with ← BLOCK
  static constexpr int kGridGap = 16;
  static constexpr int kCardWidth = (kColumnWidth - kGridGap) / 2;  // the two-up card at 1x
  static constexpr int kPageSize = 12;
  static constexpr int kSearchHeight = 40;

  explicit ToneBrowser(Services& services);
  ~ToneBrowser() override;

  // ← back to the chain.
  std::function<void()> onClose;
  // The sign-in gate: the browse-intent login that comes back here.
  std::function<void()> onSignIn;

  void resized() override;

private:
  class Body;
  class Content;
  static constexpr int kHeaderGap = 16;  // header row → search → filters
  static constexpr int kContentPadTop = 24, kContentPadBottom = 24;
  static constexpr int kPaginatorGap = 16;  // grid → paginator
  static constexpr int kPickErrorGap = 16;
  static constexpr int kDotsPadY = 64;
  static constexpr int kEmptyPadY = 64, kEmptyPadX = 24;
  static constexpr int kCopyMaxWidth = 420, kErrorMaxWidth = 340;

  void sessionChanged() override;
  void authFlowChanged() override;
  void zoomChanged() override { resized(); }

  bool signedOut() const { return !services_.session.authenticated(); }
  bool gated() const { return signedOut() && !authPending(); }
  float zoom() const { return static_cast<float>(services_.zoom.factor()); }
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
  void layoutBody();
  void layoutContent();
  void paintTopFade(juce::Graphics& g);
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

  BackLink back_;

  // The 1x body: pinned search box and filter row over the scrolled column.
  std::unique_ptr<Body> body_;
  TextField search_;
  FilterBar filters_;
  std::unique_ptr<DragScroller> scroller_;
  std::unique_ptr<Content> content_;
  std::vector<std::unique_ptr<ToneCard>> cards_;
  std::unique_ptr<BusyOverlay> gridBusy_;
  std::unique_ptr<BrowserPrompt> bodyPrompt_;  // the sign-in gate, or the fetch error
  LoadingDots dots_;
  Paginator paginator_;
};

}  // namespace t3k::ui
