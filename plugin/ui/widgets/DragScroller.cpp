#include "DragScroller.h"

namespace t3k::ui {

DragScroller::DragScroller(Axis axis) {
  const bool vertical = axis == Axis::vertical;
  setScrollBarsShown(false, false, vertical, !vertical);
  setScrollOnDragMode(ScrollOnDragMode::nonHover);
  addMouseListener(this, /*wantsEventsForAllNestedChildComponents=*/true);
}

DragScroller::~DragScroller() { removeMouseListener(this); }

void DragScroller::visibleAreaChanged(const juce::Rectangle<int>&) {
  if (onScroll) onScroll();
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
