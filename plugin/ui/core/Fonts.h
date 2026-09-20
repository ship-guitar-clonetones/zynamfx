// The UI's two typefaces (index.css / theme.ts): Arial for body copy, the
// bundled Roboto Mono for accent chrome. Sizes are CSS `font-size` px, i.e.
// em/point heights, not JUCE's ascent+descent height; the helpers convert so
// a `13rem` label in the web UI and `Fonts::sans(13)` here rasterise the same
// glyph size.
#pragma once

#include <juce_graphics/juce_graphics.h>

#include <cmath>

namespace t3k::ui {

struct Fonts {
  // Arial, sans-serif (weights 600/700 → Bold; Arial has no medium).
  static juce::Font sans(float px, bool bold = false, bool italic = false);
  // Roboto Mono 400/700, embedded.
  static juce::Font mono(float px, bool bold = false);

  // CSS `letter-spacing: Xem` → JUCE extra kerning factor.
  static juce::Font tracked(const juce::Font& font, float em);

  // Rendered width of a single line.
  static float width(const juce::Font& font, const juce::String& text) {
    return juce::GlyphArrangement::getStringWidth(font, text);
  }

  // CSS `line-height: normal`, the way Blink sizes it: ascent, descent and
  // the face's line gap each rounded to whole pixels. JUCE doesn't expose
  // the gap, so callers pass it in em (Arial 0.0327, Roboto Mono 0).
  static int normalLineHeight(const juce::Font& font, float lineGapEm = 0) {
    return juce::roundToInt(font.getAscent()) + juce::roundToInt(font.getDescent()) +
           juce::roundToInt(font.getHeightInPoints() * lineGapEm);
  }
  // Arial body copy at `px` (14px → 16, not 14 × 1.15 = 16.1 or the 1.2
  // rule of thumb's 17).
  static int normalLineHeight(float px) {
    return juce::roundToInt(px * 0.9052f) + juce::roundToInt(px * 0.2119f) + juce::roundToInt(px * 0.0327f);
  }

  // Where Blink puts the baseline inside a line box of `lineHeightPx`:
  // ascent and descent rounded to whole pixels, the half-leading that
  // centres them floored (Arial 14px at 1.4 → 14; 18px → 18; 110px at 1 →
  // 93). Centring the exact metrics instead lands text ~1px low.
  static float cssBaseline(const juce::Font& font, float lineHeightPx) {
    const float ascent = std::round(font.getAscent());
    const float descent = std::round(font.getDescent());
    return std::floor((lineHeightPx - (ascent + descent)) * 0.5f) + ascent;
  }

private:
  static juce::Typeface::Ptr monoTypeface(bool bold);
};

}  // namespace t3k::ui
