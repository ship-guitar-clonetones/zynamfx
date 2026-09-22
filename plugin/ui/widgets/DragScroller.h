// A scroll area along one axis with hidden scrollbars that a touch drag
// pans (the web's hide-scrollbar containers; JUCE remaps a plain wheel onto
// a sideways one). Once a drag has become a scroll the press it started
// with is spent: the content stops hit-testing until the finger lifts, so
// the button under it reads as not hovered and doesn't fire on release.
// Components that drag for themselves (knobs, tiles) opt out of the pan
// with setViewportIgnoreDragFlag.
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

private:
  // Listening to every nested child (a Component is a MouseListener).
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;

  bool pressSpent_ = false;
  bool contentIntercepts_ = true, childrenIntercept_ = true;
};

}  // namespace t3k::ui
