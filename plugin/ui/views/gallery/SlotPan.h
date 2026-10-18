// Lane balance on the last slot: a horizontal L↔R control bound to the
// lane's pan parameter (chainPanLeft / chainPanRight). Centre is 0.5 and the
// full 0..1 travel is left..right, so a row's output can sit anywhere in the
// stereo image.
//
// Only meaningful in stereo chain mode with a real stereo output: the image
// matrix bypasses pan in mono, and the caller hides the control there.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "services/ParamBinding.h"
#include "services/Services.h"

namespace t3k::ui {

class SlotPan : public juce::Component {
public:
  SlotPan(Services& services, const juce::String& paramId);
  ~SlotPan() override;

  static constexpr int kHeight = 16;

  void paint(juce::Graphics& g) override;

  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  bool keyPressed(const juce::KeyPress& key) override;

private:
  void writeFromX(int x);
  void nudge(float delta);

  ParamBinding binding_;
  float value_ = 0.5f;  // mirrored from the parameter; live while dragging
};

}  // namespace t3k::ui
