#include "Fonts.h"

#include "UiBinaryData.h"

namespace t3k::ui {

juce::Font Fonts::sans(float px, bool bold, bool italic) {
  const int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
  return juce::Font(juce::FontOptions("Arial", px, style).withPointHeight(px));
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
