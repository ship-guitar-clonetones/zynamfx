#include "StreamTabs.h"

#include <optional>

#include "core/Design.h"
#include "core/Fonts.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
struct Tab {
  const char* label;
  std::optional<Icon> icon;
};
constexpr Tab kTabs[] = {
    {"Trending", std::nullopt},
    {"Recently used", std::nullopt},
    {"Favorites", Icon::Bookmark},
    {"Created", std::nullopt},
};
}  // namespace

StreamTabs::StreamTabs() {
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setSize(0, kHeight);
}

void StreamTabs::setActive(Stream stream) {
  if (stream == active_) return;
  active_ = stream;
  repaint();
}

// flex: 1 on each tab: equal columns, the remainder spread from the left.
juce::Rectangle<int> StreamTabs::tabBounds(int index) const {
  const int w = getWidth();
  const int x0 = design::snap(w * static_cast<float>(index) / kCount);
  const int x1 = design::snap(w * static_cast<float>(index + 1) / kCount);
  return {x0, 0, x1 - x0, kHeight};
}

void StreamTabs::paint(juce::Graphics& g) {
  // The tablist's border-bottom.
  paint::hairlineH(g, 0, static_cast<float>(getWidth()), static_cast<float>(kHeight - 1), theme::kBorder);

  const auto font = Fonts::sans(kPx, /*bold=*/true);
  for (int i = 0; i < kCount; ++i) {
    const auto& tab = kTabs[i];
    const bool selected = static_cast<Stream>(i) == active_;
    const auto colour = selected ? theme::kWhite : theme::kMuted;
    const auto box = tabBounds(i);
    if (selected) {
      g.setColour(theme::kWhite);
      g.fillRect(box.getX(), kHeight - kUnderline, box.getWidth(), kUnderline);
    }
    // Icon + label centred as one group (gap 8), on the 16px line box.
    const float textW = Fonts::width(font, tab.label);
    const float iconW = tab.icon ? kIcon + kIconGap : 0.0f;
    float x = box.getCentreX() - (textW + iconW) / 2;
    if (tab.icon) {
      Icons::draw(g, *tab.icon, juce::Rectangle<float>(kIcon, kIcon).withCentre({x + kIcon / 2.0f, kLine / 2.0f}), colour);
      x += iconW;
    }
    paint::cssLine(g, tab.label, x, 0, kLine, textW + 1, font, colour);
  }
}

void StreamTabs::mouseUp(const juce::MouseEvent& e) {
  if (!e.mouseWasClicked() || !getLocalBounds().contains(e.getPosition())) return;
  for (int i = 0; i < kCount; ++i)
    if (tabBounds(i).contains(e.getPosition())) {
      const auto stream = static_cast<Stream>(i);
      if (stream != active_ && onChange) onChange(stream);
      return;
    }
}

}  // namespace t3k::ui
