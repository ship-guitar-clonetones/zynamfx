// The browser's stream header as tabs (ToneBrowser.tsx StreamTabs): four
// equal columns, bold 14px labels centred, white with a full-width 2px
// underline when active and muted otherwise, over the tablist's hairline.
// Bold on every label so switching tabs never reflows the row.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "services/ToneSession.h"

namespace t3k::ui {

class StreamTabs : public juce::Component {
public:
  // 16px line box + 12px bottom pad + the 2px underline (which overlaps the
  // tablist's 1px border by its -1px margin).
  static constexpr int kHeight = 30;

  StreamTabs();

  void setActive(Stream stream);
  std::function<void(Stream)> onChange;

  void paint(juce::Graphics& g) override;
  void mouseUp(const juce::MouseEvent& e) override;

private:
  static constexpr int kCount = 4;
  static constexpr float kPx = 14;
  static constexpr int kLine = 16;
  static constexpr int kIcon = 15;
  static constexpr int kIconGap = 8;
  static constexpr int kUnderline = 2;

  juce::Rectangle<int> tabBounds(int index) const;

  Stream active_ = Stream::trending;
};

}  // namespace t3k::ui
