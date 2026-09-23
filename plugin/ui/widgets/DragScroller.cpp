#include "DragScroller.h"

#include <cmath>

namespace t3k::ui {

DragScroller::DragScroller(Axis axis) : axis_(axis) {
  const bool vertical = axis == Axis::vertical;
  setScrollBarsShown(false, false, vertical, !vertical);
  setScrollOnDragMode(ScrollOnDragMode::nonHover);
  // Not a Tab stop: the controls inside are, and the page follows them.
  setWantsKeyboardFocus(false);
}

void DragScroller::visibleAreaChanged(const juce::Rectangle<int>&) {
  if (onScroll) onScroll();
}

void DragScroller::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
  if (axis_ != Axis::horizontal || e.mods.isAltDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()) {
    juce::Viewport::mouseWheelMove(e, wheel);
    return;
  }
  // The dominant axis alone; a tie is native sideways input. Whole pixels
  // move the view and the fraction carries over, so a slow gesture creeps
  // instead of jumping a rounded-up pixel per event.
  const float delta = std::abs(wheel.deltaY) > std::abs(wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
  wheelRemainder_ += delta * kWheelPixelsPerUnit;
  const int step = static_cast<int>(wheelRemainder_);
  wheelRemainder_ -= static_cast<float>(step);
  if (step == 0) return;
  const auto before = getViewPosition();
  setViewPosition(before.translated(-step, 0));
  if (getViewPosition() == before) juce::Component::mouseWheelMove(e, wheel);  // at an end: the parent's
}

}  // namespace t3k::ui
