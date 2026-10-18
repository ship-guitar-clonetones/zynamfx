// Per-slot stereo placement: 0 = hard left, 0.5 = centre, 1 = hard right.
// Bound to the slot's own block ("pan"), not to the lane's parameter: a drag
// writes setBlockParam(blockId, "pan", v) and marks the slot set; a
// double-click clears it, so the slot inherits whatever is upstream (or the
// rail). An unset slot renders dimmed at the lane's resolved placement.
// Drawn on every populated slot of its lane (see GalleryLane::setSlotPan).
//
// Only meaningful in stereo chain mode with a real stereo output: the image
// matrix bypasses pan in mono, and the caller hides the control there.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>

#include "model/ChainState.h"
#include "services/Services.h"

namespace t3k::ui {

class SlotPan : public juce::Component {
public:
  SlotPan(Services& services, std::string blockId, ChainSide side);
  ~SlotPan() override;

  static constexpr int kHeight = 16;

  // Resync from the chain state: the shown value and whether this slot places
  // it (false = the lane's resolved placement, drawn dimmed).
  void setValue(float value, bool set);

  void paint(juce::Graphics& g) override;

  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  bool keyPressed(const juce::KeyPress& key) override;

private:
  void writeFromX(int x);
  void nudge(float delta);
  // Write this slot's own placement; a negative value clears it (inherit).
  void write(float v, bool set);
  float laneDefault() const { return side_ == ChainSide::left ? 0.0f : 1.0f; }

  Services& services_;
  std::string blockId_;
  ChainSide side_;
  float value_ = 0.5f;      // shown; live while dragging
  bool set_ = false;        // false = inherited from the lane
  float inherited_ = 0.0f;  // the lane's resolved placement while unset
};

}  // namespace t3k::ui
