// A CSS `transition: opacity Nms ease` for a component: tween its alpha to a
// target over a duration, snapping when it isn't showing (nothing to see).
// Owned by the component it drives; DimGroup, the tile chrome fade and the
// branch dots all animate through this.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Tween.h"

namespace t3k::ui {

class AlphaTween {
public:
  explicit AlphaTween(juce::Component& target)
      : target_(target), tween_([this](float a) { target_.setAlpha(a); }, 1.0f) {}

  float value() const { return tween_.value(); }

  void animateTo(float alpha, int durationMs, bool animate = true) {
    if (!animate || !target_.isShowing()) {
      snap(alpha);
      return;
    }
    tween_.animateTo(alpha, durationMs);
  }

  void snap(float alpha) { tween_.snap(alpha); }

private:
  juce::Component& target_;
  Tween tween_;
};

}  // namespace t3k::ui
