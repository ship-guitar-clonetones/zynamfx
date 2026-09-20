// The knob artwork (port of KnobFace.tsx). Source of truth for the geometry
// is design/primary-knob.svg and design/secondary-knob.svg: the same
// hardware knob at two sizes, every radius here normalised to a 200x200
// box. Only the three face layers differ between the tones.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui {

enum class KnobTone { primary, secondary };

// Paints the knob into `box` (square). `angleDeg` is the pointer angle in
// degrees clockwise from noon (-135..135); `arcFromDeg` is where the value
// arc grows from (noon for centred knobs, -135 for the rest).
void drawKnobFace(juce::Graphics& g, juce::Rectangle<float> box, float angleDeg, float arcFromDeg,
                  KnobTone tone);

}  // namespace t3k::ui
