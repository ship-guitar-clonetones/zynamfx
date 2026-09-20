#include "GearFilterRow.h"

#include "core/Fonts.h"
#include "core/GearGlyphs.h"
#include "core/Icons.h"
#include "core/Labels.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kPadX = 16;
constexpr int kIcon = 20;
constexpr int kIconGap = 8;
constexpr float kPx = 14;
}  // namespace

// Gear-type filter chip: icon + label, radio-select.
class GearFilterRow::Pill : public juce::Button {
public:
  Pill(const juce::String& id, const juce::String& label) : juce::Button(label), id_(id), label_(label) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(false);
    setClickingTogglesState(false);
    naturalWidth_ = 1 + kPadX + kIcon + kIconGap + Fonts::width(Fonts::sans(kPx), label_) + kPadX + 1;
  }
  const juce::String& id() const { return id_; }
  // Fractional, like the flex item: the row snaps each pill's edges.
  float naturalWidth() const { return naturalWidth_; }
  void setActive(bool active) {
    if (active == active_) return;
    active_ = active;
    repaint();
  }
  void paintButton(juce::Graphics& g, bool, bool) override {
    const auto box = getLocalBounds().toFloat();
    const auto fg = active_ ? theme::kWhite : theme::kGray;
    paint::border(g, box, box.getHeight() / 2, active_ ? theme::kWhite : theme::kBorder);
    const float x = 1 + kPadX;
    Icons::draw(g, gear::svgFor(id_), juce::Rectangle<float>(kIcon, kIcon).withCentre({x + kIcon / 2.0f, box.getCentreY()}), fg);
    paint::text(g, label_, getLocalBounds().withTrimmedLeft(juce::roundToInt(x) + kIcon + kIconGap).withTrimmedRight(kPadX),
                Fonts::sans(kPx), fg);
  }

private:
  juce::String id_, label_;
  float naturalWidth_ = 0;
  bool active_ = false;
};

// Horizontal-only scroll area with hidden scrollbars; JUCE remaps a plain
// vertical wheel onto it (the web's useHorizontalWheelScroll).
class GearFilterRow::Scroller : public juce::Viewport {
public:
  Scroller() {
    setScrollBarsShown(false, false, false, true);
    setScrollOnDragMode(ScrollOnDragMode::nonHover);
  }
  std::function<void()> onScroll;
  void visibleAreaChanged(const juce::Rectangle<int>&) override {
    if (onScroll) onScroll();
  }
};

GearFilterRow::GearFilterRow() : scroller_(std::make_unique<Scroller>()) {
  scroller_->setViewedComponent(&row_, false);
  scroller_->onScroll = [this] { repaint(); };
  addAndMakeVisible(*scroller_);

  // Flex items advance by fractional widths; each pill's edges snap to the
  // pixel grid where they land (Blink's LayoutUnit layout).
  float x = kBleed;
  for (const auto& filter : labels::gearFilters()) {
    auto pill = std::make_unique<Pill>(filter.id, filter.label);
    const int left = juce::roundToInt(x), right = juce::roundToInt(x + pill->naturalWidth());
    pill->setBounds(left, 0, right - left, kHeight);
    pill->onClick = [this, id = pill->id()] {
      const juce::String next = active_ == id ? juce::String() : id;
      setActive(next);
      if (onChange) onChange(next);
    };
    x += pill->naturalWidth() + kGap;
    row_.addAndMakeVisible(*pill);
    pills_.push_back(std::move(pill));
  }
  row_.setSize(juce::roundToInt(x - kGap + kBleed), kHeight);
  setSize(row_.getWidth(), kHeight);
}

GearFilterRow::~GearFilterRow() = default;

void GearFilterRow::setActive(const juce::String& gear) {
  active_ = gear;
  for (auto& pill : pills_) pill->setActive(pill->id() == active_);
}

void GearFilterRow::resized() {
  scroller_->setBounds(getLocalBounds());
  // Shorter rows never scroll: the content is at least the viewport.
  row_.setSize(std::max(row_.getWidth(), getWidth()), kHeight);
}

// The gallery's gutter fades.
void GearFilterRow::paintOverChildren(juce::Graphics& g) {
  paint::edgeFades(g, getLocalBounds().toFloat(), kBleed, juce::Colours::black);
}

}  // namespace t3k::ui
