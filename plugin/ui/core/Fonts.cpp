#include "Fonts.h"

#include "UiBinaryData.h"

namespace t3k::ui {

juce::Font Fonts::sans(float px, bool bold, bool italic) {
  const int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
  return juce::Font(juce::FontOptions(sansFamily(), px, style).withPointHeight(px));
}

// Arial where it is installed (macOS, Windows, iOS). On Linux JUCE matches
// family names against the font files it finds, not fontconfig's aliases, so
// a machine without Arial gets no typeface at all: a Font with a null
// typeface has height 0 and NaN ascent/descent, and text laid out from those
// (TextFlow, cssBaseline) lands at NaN positions, which the software renderer
// turns into out-of-bounds writes (x86 converts NaN to INT_MIN; it segfaulted
// the Linux x64 self-tests painting the tone browser). Fall back to the
// metric-compatible clones, then whatever JUCE calls the system sans.
const juce::String& Fonts::sansFamily() {
  static const juce::String family = [] {
    for (const char* name : {"Arial", "Liberation Sans", "Arimo"})
      if (juce::Font(juce::FontOptions(name, 14.0f, juce::Font::plain)).getTypefacePtr() != nullptr)
        return juce::String(name);
    return juce::Font::getDefaultSansSerifFontName();
  }();
  return family;
}

juce::Font Fonts::mono(float px, bool bold) {
  return juce::Font(juce::FontOptions(monoTypeface(bold)).withPointHeight(px));
}

juce::Font Fonts::tracked(const juce::Font& font, float em) {
  // withExtraKerningFactor is relative to the JUCE height; CSS em is relative
  // to the point size.
  return font.withExtraKerningFactor(em * font.getHeightToPointsFactor());
}

juce::Typeface::Ptr Fonts::monoTypeface(bool bold) {
  // Typefaces are heavyweight and immutable: build each once per process.
  static const juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor(
      UiBinaryData::RobotoMonoRegular_ttf, UiBinaryData::RobotoMonoRegular_ttfSize);
  static const juce::Typeface::Ptr boldFace = juce::Typeface::createSystemTypefaceFor(
      UiBinaryData::RobotoMonoBold_ttf, UiBinaryData::RobotoMonoBold_ttfSize);
  return bold ? boldFace : regular;
}

}  // namespace t3k::ui
