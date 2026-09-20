#include "ToneBrowser.h"

#include <algorithm>

#include "core/Fonts.h"
#include "core/Help.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr const char* kSignInHeading = "Sign in to see your tones and discover zillions more.";
constexpr const char* kDiscoverMoreHeading = "Discover zillions more tones.";
constexpr const char* kSignInLabel = "Sign in or create free account";
constexpr const char* kStreamError = "Failed to load tones from TONE3000.";
constexpr const char* kPickError = "Failed to load that tone. Please try again.";
constexpr float kPickErrorPx = 12;
constexpr float kEmptyPx = 13;

const char* emptyCopy(Stream stream) {
  switch (stream) {
    case Stream::trending: return "No trending tones right now. Check back soon.";
    case Stream::downloaded: return "Tones you download on TONE3000 will show up here.";
    case Stream::favorited: return "Tones you favorite on TONE3000 will show up here.";
    case Stream::created: return "Tones you upload to TONE3000 will show up here.";
  }
  return "";
}
}  // namespace

// The scrolled column: hosts the pills, cards, prompts and paginator, and
// paints the two bare text rows (pick error, empty copy) itself.
class ToneBrowser::Content : public juce::Component {
public:
  juce::String pickError, emptyCopy;
  juce::Rectangle<int> pickErrorBox, emptyBox;

  void paint(juce::Graphics& g) override {
    if (pickError.isNotEmpty()) paint::text(g, pickError, pickErrorBox, Fonts::sans(kPickErrorPx), theme::kBrandRed);
    if (emptyCopy.isNotEmpty())
      paint::text(g, emptyCopy, emptyBox, Fonts::sans(kEmptyPx), theme::kMuted, juce::Justification::centred);
  }
};

// Vertical scroll with hidden scrollbars (the web's hide-scrollbar column).
class ToneBrowser::Scroller : public juce::Viewport {
public:
  Scroller() {
    setScrollBarsShown(false, false, true, false);
    setScrollOnDragMode(ScrollOnDragMode::nonHover);
  }
};

std::unique_ptr<PillButton> ToneBrowser::makeBrowseButton() {
  // Taller than the default outline pill: Browse is the persistent path to
  // the full catalog, so the icon and mark scale up with the 40px height.
  auto button = std::make_unique<PillButton>("Browse", PillButton::Style::outline);
  button->setMetrics({22, 0, 15, 10});
  button->setLeadingIcon(Icon::Search, 16);
  button->setTrailingMark(14);
  button->setSize(button->getWidth(), kBrowseHeight);
  return button;
}

std::unique_ptr<PillButton> ToneBrowser::makeFilledButton(const juce::String& label) {
  return std::make_unique<PillButton>(label, PillButton::Style::filled);
}

ToneBrowser::ToneBrowser(Services& services)
    : services_(services),
      back_("Select Tone", help::Key::closeToneBrowser),
      browse_(makeBrowseButton()),
      scroller_(std::make_unique<Scroller>()),
      content_(std::make_unique<Content>()) {
  // Land on the stream the user was on last time; default to the public
  // Trending feed rather than a gated stream that might now be unreachable.
  stream_ = streamFromId(services_.prefs.get(UiPrefs::kBrowserStream)).value_or(Stream::trending);

  back_.onClick = [this] {
    if (onClose) onClose();
  };
  browse_->onClick = [this] {
    if (onBrowseTone3000) onBrowseTone3000();
  };
  tabs_.setActive(stream_);
  tabs_.onChange = [this](Stream next) { switchStream(next); };
  addAndMakeVisible(back_);
  addAndMakeVisible(*browse_);
  addAndMakeVisible(tabs_);

  scroller_->setViewedComponent(content_.get(), false);
  addAndMakeVisible(*scroller_);
  gearRow_.onChange = [this](const juce::String& gear) { setGearFilter(gear); };
  content_->addChildComponent(gearRow_);
  content_->addChildComponent(dots_);
  paginator_.onPageChange = [this](int page) { setPage(page); };
  content_->addChildComponent(paginator_);

  services_.session.addListener(this);
  fetch();
}

ToneBrowser::~ToneBrowser() { services_.session.removeListener(this); }

// State
bool ToneBrowser::showSignInPrompt() const { return streamIsGated(stream_) && !services_.session.authenticated(); }

bool ToneBrowser::authPending() const {
  return services_.session.authFlow().phase == ToneSession::AuthFlow::Phase::returning;
}

void ToneBrowser::sessionChanged() { fetch(); }

void ToneBrowser::authFlowChanged() {
  // The effect re-ran when authPending cleared; a stream fetch held back
  // during the return goes out now.
  if (!authPending() && loading_ && !result_) fetch();
}

void ToneBrowser::switchStream(Stream next) {
  if (next == stream_) return;
  stream_ = next;
  services_.prefs.set(UiPrefs::kBrowserStream, streamId(next));
  tabs_.setActive(next);
  page_ = 1;
  fetch();
}

void ToneBrowser::setGearFilter(const juce::String& gear) {
  gear_ = gear;
  page_ = 1;
  fetch();
}

void ToneBrowser::setPage(int page) {
  if (page == page_) return;
  page_ = page;
  fetch();
}

void ToneBrowser::fetch() {
  // Pre-mounted during an OAuth return: the token exchange hasn't finished,
  // so we don't yet know whether to render the gated prompt or fetch. Keep
  // the loading state; authFlowChanged reruns this once it clears.
  if (authPending()) return;

  // Gated stream, signed out: skip the fetch entirely (it would only fail
  // with not_authenticated and trip the client's re-auth callback).
  if (showSignInPrompt()) {
    fetchScope_.reset();
    result_.reset();
    loading_ = false;
    error_ = false;
    rebuildCards();
    rebuildBody();
    return;
  }

  fetchScope_.reset();  // a newer request supersedes anything in flight
  loading_ = true;
  error_ = false;
  rebuildBody();
  if (stream_ == Stream::trending) {
    services_.session.listTrending(gear_, fetchScope_.wrap([this](ui::Result<std::vector<Tone>> r) {
      if (r) streamLoaded(std::move(*r.value), std::nullopt, std::nullopt);
      else streamFailed();
    }));
  } else {
    services_.session.listStream(stream_, page_, kPageSize, gear_, fetchScope_.wrap([this](ui::Result<TonePage> r) {
      if (r) streamLoaded(std::move(r.value->data), r.value->page, r.value->totalPages);
      else streamFailed();
    }));
  }
}

void ToneBrowser::streamLoaded(std::vector<Tone> tones, std::optional<int> page, std::optional<int> totalPages) {
  result_ = StreamResult{std::move(tones), page, totalPages};
  loading_ = false;
  rebuildCards();
  rebuildBody();
  // Jump to the top whenever fresh results land (page turn / pill).
  scroller_->setViewPosition(0, 0);
}

void ToneBrowser::streamFailed() {
  error_ = true;
  loading_ = false;
  rebuildBody();
}

void ToneBrowser::pick(const Tone& tone) {
  // Trending is viewable signed out, but resolving a tone (its models + a
  // download token) needs a session: route the click through sign-in.
  if (stream_ == Stream::trending && !services_.session.authenticated()) {
    if (onSignIn) onSignIn();
    return;
  }
  if (pickingId_) return;
  pickError_.clear();
  pickingId_ = tone.id;
  rebuildBody();
  // On success the parent closes the browser; the scope guards the reply.
  services_.session.selectTone(tone.id, scope_.wrap([this](const juce::String& error) {
    if (error.isEmpty()) return;
    pickError_ = kPickError;
    pickingId_.reset();
    rebuildBody();
  }));
}

// Children
void ToneBrowser::rebuildCards() {
  cards_.clear();
  if (!result_) return;
  for (const auto& tone : result_->data) {
    auto card = std::make_unique<ToneCard>(services_.images, tone);
    card->onClick = [this, id = tone.id] {
      const auto it = std::find_if(cards_.begin(), cards_.end(), [id](const auto& c) { return c->tone().id == id; });
      if (it != cards_.end()) pick((*it)->tone());
    };
    content_->addAndMakeVisible(*card);
    cards_.push_back(std::move(card));
  }
}

void ToneBrowser::rebuildBody() {
  const bool signIn = showSignInPrompt();
  const bool showError = error_ && !loading_;
  const bool hasCards = result_ && !result_->data.empty();

  // Body prompt: the gated sign-in, or the stream error with Try again.
  bodyPrompt_.reset();
  if (signIn) {
    bodyPrompt_ = std::make_unique<BrowserPrompt>(true, kSignInHeading, kCopyMaxWidth, makeFilledButton(kSignInLabel));
    bodyPrompt_->button().onClick = [this] {
      if (onSignIn) onSignIn();
    };
  } else if (showError) {
    bodyPrompt_ = std::make_unique<BrowserPrompt>(false, kStreamError, kErrorMaxWidth, makeFilledButton("Try again"));
    bodyPrompt_->button().onClick = [this] { fetch(); };
  }
  if (bodyPrompt_) content_->addAndMakeVisible(*bodyPrompt_);

  // First load (nothing to dim yet): dots alone. Empty stream: its copy.
  dots_.setVisible(!signIn && !showError && !hasCards && loading_);
  content_->emptyCopy = !signIn && !showError && !hasCards && !loading_ ? emptyCopy(stream_) : juce::String();

  // Cards stay mounted while a new stream / page loads, dimmed and inert
  // under the busy overlay. Other cards dim while one pick resolves.
  const bool cardsVisible = !signIn && !showError && hasCards;
  for (auto& card : cards_) {
    card->setVisible(cardsVisible);
    const bool picking = pickingId_ && *pickingId_ == card->tone().id;
    card->setLoading(picking);
    card->setDisabled(ToneCard::unavailable(card->tone()) || pickingId_.has_value() || loading_);
  }
  if (cardsVisible && loading_) {
    if (!gridBusy_) {
      gridBusy_ = std::make_unique<BusyOverlay>(BusyOverlay::Align::top);
      content_->addAndMakeVisible(*gridBusy_);
    }
  } else {
    gridBusy_.reset();
  }

  // Paginated streams keep the paginator at the end of the page; Trending
  // is a fixed top-10 feed and never paginates.
  const bool paginate = !signIn && !error_ && result_ && result_->totalPages.value_or(1) > 1;
  paginator_.setVisible(paginate);
  if (paginate) {
    paginator_.set(page_, *result_->totalPages);
    paginator_.setAlpha(loading_ ? theme::kDisabledOpacity : 1.0f);
    paginator_.setInterceptsMouseClicks(!loading_, false);
  }

  // Trending always closes with a path to the rest of the catalog: a
  // sign-in nudge while signed out, the Browse CTA again when signed in.
  // Held back until results land so it never floats under the dots.
  footerBrowse_.reset();
  footerPrompt_.reset();
  if (stream_ == Stream::trending && !error_ && !loading_) {
    if (services_.session.authenticated()) {
      footerBrowse_ = makeBrowseButton();
      footerBrowse_->onClick = [this] {
        if (onBrowseTone3000) onBrowseTone3000();
      };
      content_->addAndMakeVisible(*footerBrowse_);
    } else {
      footerPrompt_ = std::make_unique<BrowserPrompt>(true, kDiscoverMoreHeading, kCopyMaxWidth, makeFilledButton(kSignInLabel));
      footerPrompt_->button().onClick = [this] {
        if (onSignIn) onSignIn();
      };
      content_->addAndMakeVisible(*footerPrompt_);
    }
  }

  content_->pickError = pickError_;
  gearRow_.setVisible(!signIn);
  layoutContent();
}

// Layout
void ToneBrowser::resized() {
  const int w = getWidth();
  const int colW = std::min(kColumnWidth, w);
  const int colX = (w - colW) / 2;

  // Header row: top-aligned (Browse is taller), no side inset, flush with
  // the column like ← BLOCK.
  int y = kPadTop;
  back_.setTopLeftPosition(colX, y);
  browse_->setTopLeftPosition(colX + colW - browse_->getWidth(), y);
  y += kBrowseHeight + kHeaderGap;
  tabs_.setBounds(colX, y, colW, StreamTabs::kHeight);
  y += StreamTabs::kHeight;

  scroller_->setBounds(0, y, w, std::max(0, getHeight() - y));
  layoutContent();
}

void ToneBrowser::layoutContent() {
  const int w = scroller_->getWidth();
  if (w <= 0) return;
  const int colW = std::min(kColumnWidth, w);
  const int colX = (w - colW) / 2;
  int y = kContentPadTop;

  if (gearRow_.isVisible()) {
    gearRow_.setColumn({colX, y, colW, GearFilterRow::kHeight});
    y += GearFilterRow::kHeight;
  }
  if (pickError_.isNotEmpty()) {
    y += kPickErrorGap;
    const int line = Fonts::normalLineHeight(kPickErrorPx);
    content_->pickErrorBox = {colX, y, colW, line};
    y += line;
  }

  // Tone grid / empty state / sign-in prompt.
  y += kBodyGap;
  if (bodyPrompt_) {
    bodyPrompt_->setBounds(colX, y, colW, bodyPrompt_->heightFor(colW));
    y += bodyPrompt_->getHeight();
  } else if (dots_.isVisible()) {
    dots_.setTopLeftPosition(colX + (colW - dots_.getWidth()) / 2, y + kDotsPadY);
    y += kDotsPadY + dots_.getHeight() + kDotsPadY;
  } else if (content_->emptyCopy.isNotEmpty()) {
    const int line = Fonts::normalLineHeight(kEmptyPx);
    content_->emptyBox = {colX + kEmptyPadX, y + kEmptyPadY, colW - 2 * kEmptyPadX, line};
    y += kEmptyPadY + line + kEmptyPadY;
  } else if (!cards_.empty() && cards_.front()->isVisible()) {
    // Grid rows are as tall as their taller card, fractionally (a wrapped
    // 14px title is 36.4px): the rows accumulate at that precision and each
    // card's edges snap where they land, as the two-column CSS grid does.
    const int gridTop = y;
    const int cardW = (colW - kGridGap) / 2;
    float rowY = static_cast<float>(y);
    for (size_t i = 0; i < cards_.size(); i += 2) {
      float rowH = cards_[i]->contentHeightFor(cardW);
      if (i + 1 < cards_.size()) rowH = std::max(rowH, cards_[i + 1]->contentHeightFor(cardW));
      const int top = juce::roundToInt(rowY), bottom = juce::roundToInt(rowY + rowH);
      for (size_t j = i; j < std::min(i + 2, cards_.size()); ++j) {
        cards_[j]->setContentHeight(rowH);
        cards_[j]->setBounds(colX + static_cast<int>(j - i) * (cardW + kGridGap), top, cardW, bottom - top);
      }
      rowY += rowH + kGridGap;
    }
    y = juce::roundToInt(rowY - kGridGap);
    if (gridBusy_) gridBusy_->setBounds(colX, gridTop, colW, y - gridTop);
  }

  if (paginator_.isVisible()) {
    y += kPaginatorGap;
    paginator_.setTopLeftPosition(colX + colW - paginator_.getWidth(), y);
    y += Paginator::kHeight;
  }

  if (footerBrowse_) {
    y += BrowserPrompt::kPadY;
    footerBrowse_->setTopLeftPosition(colX + (colW - footerBrowse_->getWidth()) / 2, y);
    y += footerBrowse_->getHeight() + BrowserPrompt::kPadY;
  } else if (footerPrompt_) {
    footerPrompt_->setBounds(colX, y, colW, footerPrompt_->heightFor(colW));
    y += footerPrompt_->getHeight();
  }

  y += kContentPadBottom;
  content_->setSize(w, std::max(y, scroller_->getHeight()));
  content_->repaint();
}

}  // namespace t3k::ui
