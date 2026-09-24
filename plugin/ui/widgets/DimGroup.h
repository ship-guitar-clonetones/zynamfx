// The `.ui-off` wrapper: a container whose children dim to DISABLED_OPACITY
// and go inert (no clicks, no hover hints) while the feature they control
// is powered off, easing over 0.2 s like the CSS transition did. The group
// itself keeps receiving the pointer while off, so a hint set on it
// (Spread on a mono rig: "why is this dim?") still resolves.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "core/AlphaTween.h"

namespace t3k::ui {

class DimGroup : public juce::Component {
public:
  DimGroup();

  void setOff(bool off, bool animate = true);
  bool isOff() const { return off_; }

private:
  bool off_ = false;
  AlphaTween fade_{*this};
};

}  // namespace t3k::ui
