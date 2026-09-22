#include "DragScroller.h"

#include <cmath>

namespace t3k::ui {

DragScroller::DragScroller(Axis axis) : axis_(axis) {
  const bool vertical = axis == Axis::vertical;
  setScrollBarsShown(false, false, vertical, !vertical);
  setScrollOnDragMode(ScrollOnDragMode::nonHover);
  // Not a Tab stop: the controls inside are, and the page follows them.
  setWantsKeyboardFocus(false);
  addMouseListener(this, /*wantsEventsForAllNestedChildComponents=*/true);
}

DragScroller::~DragScroller() { removeMouseListener(this); }

void DragScroller::visibleAreaChanged(const juce::Rectangle<int>&) {
  if (onScroll) onScroll();
}

void DragScroller::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
  // Only the event bubbled up to us: it also arrives through the nested
  // listener, from the child it landed on (Viewport applies the same guard).
  if (e.eventComponent != this) return;
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

// The pressed component sees each event before we do, so its hover state
// lags this by one drag event; a scroll is many.
void DragScroller::mouseDrag(const juce::MouseEvent&) {
  if (pressSpent_ || !isCurrentlyScrollingOnDrag()) return;
  auto* content = getViewedComponent();
  if (content == nullptr) return;
  pressSpent_ = true;
  content->getInterceptsMouseClicks(contentIntercepts_, childrenIntercept_);
  content->setInterceptsMouseClicks(false, false);
}

void DragScroller::mouseUp(const juce::MouseEvent&) {
  if (!pressSpent_) return;
  pressSpent_ = false;
  if (auto* content = getViewedComponent())
    content->setInterceptsMouseClicks(contentIntercepts_, childrenIntercept_);
}

}  // namespace t3k::ui
