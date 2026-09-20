// Row of radio-select gear filter pills shared across the browser's streams
// (ToneBrowser.tsx GearFilterRow): icon + label outline chips, the active
// one white, tapping it again clears back to "no filter". Scrolls sideways
// under the same edge fades as the chain gallery: the row is laid out
// kBleed wider than the content column on each side, with that much scroll
// padding, so at rest the end pills sit flush with the column and only
// slide under the gradient once scrolled.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "views/gallery/GalleryGeometry.h"

namespace t3k::ui {

class GearFilterRow : public juce::Component {
public:
  static constexpr int kHeight = 38;  // 20px icon + 8px pads + 1px borders
  static constexpr int kBleed = gallery::kEdgeFadeWidth;
  static constexpr int kGap = 10;

  GearFilterRow();
  ~GearFilterRow() override;

  // The bounds of the content column: the row extends kBleed past it.
  void setColumn(juce::Rectangle<int> column) { setBounds(column.expanded(kBleed, 0).withHeight(kHeight)); }

  // "" = no filter (the default).
  void setActive(const juce::String& gear);
  std::function<void(const juce::String& gear)> onChange;

  void resized() override;
  void paintOverChildren(juce::Graphics& g) override;

private:
  class Pill;
  class Scroller;

  juce::String active_;
  std::unique_ptr<Scroller> scroller_;
  juce::Component row_;
  std::vector<std::unique_ptr<Pill>> pills_;
};

}  // namespace t3k::ui
