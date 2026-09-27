// The transpose group's advanced panel (right-click the group; touch-and-hold
// the knob): the three settings behind the semitone knob, mirroring
// Transpose::Params:
//  - Fine: trims the shift by ±50 cents, for songs tuned between semitones.
//  - Tonality: the frequency above which the input bypasses the shifter
//    (1-20 kHz, log), so pick attack and fret noise keep their brightness
//    while the notes move. Off (the top) is a pure shift.
//  - Latency: the engine's delay buffer, four detents (20/30/40/60 ms), read
//    out as the latency each reports (11/16/21/31 ms). Attacks always pass
//    in a few ms; a longer buffer holds lower notes and splices less often.
// Plain knobs, no power switches, same footprint as the gate deck so the two
// read as one family.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "services/Services.h"
#include "widgets/ParamControls.h"
#include "widgets/Popover.h"

namespace t3k::ui {

class TransposeDeckPanel : public Popover {
public:
  static constexpr int kWidth = 262;
  static constexpr int kHeight = 85;
  // Gap between the panel's bottom edge and its anchor's top.
  static constexpr int kGap = 6;

  explicit TransposeDeckPanel(Services& services);

  // Restores the whole deck to its defaults (Alt/Option-click on the
  // Transpose knob resets the effect, not just the semitones).
  static void resetDeck(Backend& backend);

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  ParamKnob fine_, tonality_, window_;
};

}  // namespace t3k::ui
