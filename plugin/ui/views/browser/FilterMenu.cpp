#include "FilterMenu.h"

#include <algorithm>
#include <cmath>

#include "core/Fonts.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/Avatar.h"

namespace t3k::ui {

// One option: check column, optional avatar, label; a rounded hover fill.
// Picked rows read white, the rest muted.
class FilterMenu::Row : public juce::Button {
public:
  static constexpr int kPadX = 12;
  static constexpr int kCheck = 16;
  static constexpr int kAvatar = 22;  // the cards' creator avatar size
  static constexpr int kColumnGap = 8;
  static constexpr float kPx = 14;

  Row(const Option& option, bool picked, ImageLoader& images)
      : juce::Button(option.label), label_(option.label), picked_(picked) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(false);
    if (option.avatarUrl) {
      avatar_ = std::make_unique<Avatar>();
      addAndMakeVisible(*avatar_);
      if (option.avatarUrl->isNotEmpty())
        images.load(*option.avatarUrl, avatarRequest_, [this](const juce::Image& img) { avatar_->setImage(img); });
    }
  }

  // Everything but the label, so the menu can fit its widest row.
  int chromeWidth() const { return 2 * kPadX + kCheck + kColumnGap + (avatar_ ? kAvatar + kColumnGap : 0); }
  const juce::String& label() const { return label_; }

  void resized() override {
    if (avatar_) avatar_->setBounds(kPadX + kCheck + kColumnGap, (getHeight() - kAvatar) / 2, kAvatar, kAvatar);
  }

  void paintButton(juce::Graphics& g, bool highlighted, bool) override {
    auto box = getLocalBounds();
    if (highlighted) paint::fill(g, box.toFloat(), 8.0f, juce::Colours::white.withAlpha(0.08f));
    auto content = box.reduced(kPadX, 0);
    const auto check = content.removeFromLeft(kCheck).withSizeKeepingCentre(kCheck, kCheck);
    if (picked_) Icons::draw(g, Icon::Check, check.toFloat(), theme::kWhite);
    content.removeFromLeft(kColumnGap);
    if (avatar_) content.removeFromLeft(kAvatar + kColumnGap);
    paint::text(g, label_, content, Fonts::sans(kPx), picked_ ? theme::kWhite : theme::kMuted);
  }

private:
  juce::String label_;
  bool picked_;
  std::unique_ptr<Avatar> avatar_;
  ImageLoader::Request avatarRequest_;
};

FilterMenu::FilterMenu(ImageLoader& images, juce::String searchPlaceholder)
    : images_(images), searchable_(searchPlaceholder.isNotEmpty()) {
  if (searchable_) {
    search_.setPlaceholder(searchPlaceholder);
    search_.setCornerRadius(8);
    search_.setPadding(6, 32, 10);
    search_.setLeadingIcon(Icon::Search, 14, 11, theme::kGray);
    search_.setBackground(theme::kSurfaceRaised);
    search_.onChange = [this](const juce::String& text) {
      if (onSearch) onSearch(text.trim());
    };
    search_.onEscape = [this] { dismiss(); };
    addAndMakeVisible(search_);
  }
  viewport_.setViewedComponent(&list_, false);
  viewport_.setScrollBarsShown(false, false, true, false);
  addAndMakeVisible(viewport_);
  layoutRows();
}

FilterMenu::~FilterMenu() = default;

void FilterMenu::setOptions(std::vector<Option> options, const std::vector<juce::String>& picked) {
  rows_.clear();
  for (auto& option : options) {
    const bool isPicked = std::find(picked.begin(), picked.end(), option.id) != picked.end();
    auto row = std::make_unique<Row>(option, isPicked, images_);
    row->onClick = [this, id = option.id] {
      close();
      if (onPick) onPick(id);
    };
    list_.addAndMakeVisible(*row);
    rows_.push_back(std::move(row));
  }
  status_.clear();
  layoutRows();
}

void FilterMenu::setStatus(juce::String status) {
  rows_.clear();
  status_ = std::move(status);
  layoutRows();
}

void FilterMenu::openBelow(juce::Component& anchor) {
  open(anchor, Align::left, kGap);
  if (searchable_) search_.focus();
}

// Fit the widest row, then grow with the rows up to kMaxVisibleRows.
void FilterMenu::layoutRows() {
  int content = 0;
  for (const auto& row : rows_) {
    const auto labelW = static_cast<int>(std::ceil(Fonts::width(Fonts::sans(Row::kPx), row->label())));
    content = std::max(content, row->chromeWidth() + labelW + 1);  // +1: no ellipsis on the widest
  }
  width_ = std::clamp(content + 2 * (kBorder + kPad), kMinWidth, kMaxWidth);

  const int rowCount = status_.isNotEmpty() ? 1 : static_cast<int>(rows_.size());
  const int listH = kRowHeight * std::max(1, rowCount);
  const int visibleH = kRowHeight * std::min(std::max(1, rowCount), kMaxVisibleRows);
  list_.setSize(width_ - 2 * (kBorder + kPad), listH);
  int y = 0;
  for (auto& row : rows_) {
    row->setBounds(0, y, list_.getWidth(), kRowHeight);
    y += kRowHeight;
  }
  const int searchH = searchable_ ? kSearchHeight + kSearchGap : 0;
  setSize(width_, 2 * (kBorder + kPad) + searchH + visibleH);
  reposition();
  repaint();
}

void FilterMenu::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  paint::fill(g, box, theme::kPanelCorner, theme::kPanelBg);
  paint::border(g, box, theme::kPanelCorner, theme::kBorder);
  if (status_.isNotEmpty()) {
    auto area = viewport_.getBounds().withHeight(kRowHeight);
    paint::text(g, status_, area, Fonts::sans(kStatusPx), theme::kMuted, juce::Justification::centred);
  }
}

void FilterMenu::resized() {
  auto area = contentBounds().reduced(kPad);
  if (searchable_) {
    search_.setBounds(area.removeFromTop(kSearchHeight));
    area.removeFromTop(kSearchGap);
  }
  viewport_.setBounds(area);
}

}  // namespace t3k::ui
