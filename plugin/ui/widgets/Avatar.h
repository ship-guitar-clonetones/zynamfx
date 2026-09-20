// Circular avatar (port of AvatarFallback.tsx): the creator image clipped to
// a circle, or the GRAY person glyph when there is none / it failed to load.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui {

class Avatar : public juce::Component {
public:
  Avatar() { setInterceptsMouseClicks(false, false); }

  void setImage(juce::Image image) {
    image_ = std::move(image);
    cover_ = {};
    repaint();
  }

  void paint(juce::Graphics& g) override;
  void resized() override { cover_ = {}; }

private:
  juce::Image image_;
  juce::Image cover_;  // image_ cover-fitted at this size and pixel scale
  float coverScale_ = 0.0f;
};

}  // namespace t3k::ui
