// A scroll area along one axis with hidden scrollbars that a touch drag
// pans (the web's hide-scrollbar containers). Once a drag has become a
// scroll the press it started with is spent: the content stops hit-testing
// until the finger lifts, so the button under it reads as not hovered and
// doesn't fire on release. Components that drag for themselves (knobs,
// tiles) opt out of the pan with setViewportIgnoreDragFlag.
//
// A plain (vertical) wheel pans a sideways scroller, as the web's
// useHorizontalWheelScroll did, by the gesture's dominant axis. JUCE's own
// remap takes deltaX whenever it is non-zero, and a mostly vertical
// trackpad gesture jitters a few sideways pixels of either sign, so the pan
// flip-flopped between those and the real motion until the gesture was big
// enough to be purely vertical.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace t3k::ui {

class DragScroller : public juce::Viewport {
public:
  enum class Axis { horizontal, vertical };

  explicit DragScroller(Axis axis);
  ~DragScroller() override;

  // The view moved (scroll, drag or programmatic).
  std::function<void()> onScroll;

  void visibleAreaChanged(const juce::Rectangle<int>&) override;
  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
  // Viewport's rate (14 × its 16px single step), so a native sideways
  // gesture pans as it did.
  static constexpr float kWheelPixelsPerUnit = 14 * 16;

  // Listening to every nested child (a Component is a MouseListener).
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

  const Axis axis_;
  bool pressSpent_ = false;
  bool contentIntercepts_ = true, childrenIntercept_ = true;
  float wheelRemainder_ = 0;  // sub-pixel carry between wheel events
};

}  // namespace t3k::ui
