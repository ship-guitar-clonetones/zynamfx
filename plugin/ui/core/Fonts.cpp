#include "Fonts.h"

#include "UiBinaryData.h"

namespace t3k::ui {

namespace {

// Typefaces are heavyweight and immutable: build each once per process.
juce::Typeface::Ptr embedded(const char* data, int size) {
  return juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
}

// Arial is the web UI's body face and is installed on macOS, Windows and
// iOS. JUCE on Linux matches family names against the font files it scans,
// not fontconfig's aliases, so asking for Arial there gets a null typeface
// (height 0, NaN ascent: text laid out at NaN positions, which the software
// renderer turns into out-of-bounds writes). Where Arial is missing the
// embedded Arimo stands in: same metrics, so line boxes and wrapping match.
bool haveArial() {
  static const bool have =
      juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::plain)).getTypefacePtr() != nullptr;
  return have;
}

juce::Typeface::Ptr arimo(bool bold, bool italic) {
  static const juce::Typeface::Ptr faces[] = {
      embedded(UiBinaryData::ArimoRegular_ttf, UiBinaryData::ArimoRegular_ttfSize),
      embedded(UiBinaryData::ArimoBold_ttf, UiBinaryData::ArimoBold_ttfSize),
      embedded(UiBinaryData::ArimoItalic_ttf, UiBinaryData::ArimoItalic_ttfSize),
      embedded(UiBinaryData::ArimoBoldItalic_ttf, UiBinaryData::ArimoBoldItalic_ttfSize),
  };
  return faces[(bold ? 1 : 0) + (italic ? 2 : 0)];
}

juce::Typeface::Ptr robotoMono(bool bold) {
  static const juce::Typeface::Ptr regular =
      embedded(UiBinaryData::RobotoMonoRegular_ttf, UiBinaryData::RobotoMonoRegular_ttfSize);
  static const juce::Typeface::Ptr boldFace =
      embedded(UiBinaryData::RobotoMonoBold_ttf, UiBinaryData::RobotoMonoBold_ttfSize);
  return bold ? boldFace : regular;
}

}  // namespace

juce::Font Fonts::sans(float px, bool bold, bool italic) {
  const int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
  const auto options = haveArial() ? juce::FontOptions("Arial", px, style) : juce::FontOptions(arimo(bold, italic));
  return juce::Font(options.withPointHeight(px));
}

juce::Font Fonts::mono(float px, bool bold) {
  return juce::Font(juce::FontOptions(robotoMono(bold)).withPointHeight(px));
}

juce::Font Fonts::tracked(const juce::Font& font, float em) {
  // withExtraKerningFactor is relative to the JUCE height; CSS em is relative
  // to the point size.
  return font.withExtraKerningFactor(em * font.getHeightToPointsFactor());
}

}  // namespace t3k::ui
