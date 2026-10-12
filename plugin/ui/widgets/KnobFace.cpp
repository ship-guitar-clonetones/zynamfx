// Original flat encoder artwork for the FX visual prototype.
#include "KnobFace.h"
#include <cmath>
#include "core/Theme.h"
namespace t3k::ui {
void drawKnobFace(juce::Graphics& g, juce::Rectangle<float> box, float angleDeg,
                  float arcFromDeg, KnobTone tone) {
 const auto c=box.getCentre();
 const float r=box.getWidth()*0.44f;
 const auto accent=tone==KnobTone::primary ? theme::kAccent : theme::kEffect;
 juce::Path track;
 track.addCentredArc(c.x,c.y,r,r,0,juce::degreesToRadians(-135.0f),juce::degreesToRadians(135.0f),true);
 g.setColour(theme::kBorder);g.strokePath(track,juce::PathStrokeType(2.0f));
 juce::Path value;
 value.addCentredArc(c.x,c.y,r,r,0,juce::degreesToRadians(juce::jmin(angleDeg,arcFromDeg)),
                     juce::degreesToRadians(juce::jmax(angleDeg,arcFromDeg)),true);
 g.setColour(accent);g.strokePath(value,juce::PathStrokeType(2.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
 auto face=box.reduced(box.getWidth()*0.17f);
 g.setColour(theme::kSurface);g.fillEllipse(face);
 g.setColour(theme::kBorder);g.drawEllipse(face,1.0f);
 const float a=juce::degreesToRadians(angleDeg);
 g.setColour(theme::kWhite);
 g.drawLine(c.x+std::sin(a)*r*0.40f,c.y-std::cos(a)*r*0.40f,
            c.x+std::sin(a)*r*0.67f,c.y-std::cos(a)*r*0.67f,2.0f);
}
} // namespace t3k::ui
