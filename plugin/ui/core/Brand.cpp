#include "Brand.h"

#include "Paint.h"
#include "Fonts.h"
#include "Theme.h"
#include "UiBinaryData.h"

namespace t3k::ui {

namespace {
constexpr const char* kA2MarkSvg=R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><rect x="1" y="1" width="22" height="22" rx="4" fill="#232730" stroke="#8e98ff"/><path d="m5 17 4-10 4 10M6 14h6M15 9c0-3 6-3 5 1l-5 6h6" fill="none" stroke="#ffffff" stroke-width="1.5"/></svg>)svg";
constexpr const char* kZyMarkSvg=R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 44 20"><rect x="0.75" y="0.75" width="18.5" height="18.5" rx="3" fill="#35262b" stroke="#f06470" stroke-width="1.5"/><path d="M4.5 5h11l-11 10h11" fill="none" stroke="#f06470" stroke-width="2.25" stroke-linecap="square" stroke-linejoin="miter"/><rect x="24.75" y="0.75" width="18.5" height="18.5" rx="3" fill="#19322f" stroke="#57d4c2" stroke-width="1.5"/><path d="m28.5 5 5.5 5 5.5-5M34 10v5" fill="none" stroke="#57d4c2" stroke-width="2.25" stroke-linecap="square" stroke-linejoin="miter"/></svg>)svg";
constexpr const char* kVerifiedBadgeSvg=R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20"><circle cx="10" cy="10" r="9" fill="#5cd8c6"/><path d="m5 10 3 3 7-7" fill="none" stroke="#101115" stroke-width="2"/></svg>)svg";

std::unique_ptr<juce::Drawable> parseString(const char* svg) {
  auto drawable = juce::Drawable::createFromSVGString(svg);
  jassert(drawable != nullptr);
  return drawable;
}

}  // namespace

const juce::Drawable& Brand::logo() {
  static const auto d = juce::Drawable::createFromImageData(UiBinaryData::t3k_svg, UiBinaryData::t3k_svgSize);
  return *d;
}

const juce::Drawable& Brand::zyMark() {
  static const auto d = parseString(kZyMarkSvg);
  return *d;
}

const juce::Drawable& Brand::mark() {
  static const auto d = juce::Drawable::createFromImageData(UiBinaryData::t3kmark_svg, UiBinaryData::t3kmark_svgSize);
  return *d;
}

const juce::Drawable& Brand::a2Mark() {
  static const auto d = parseString(kA2MarkSvg);
  return *d;
}

const juce::Drawable& Brand::verifiedBadge() {
  static const auto d = parseString(kVerifiedBadgeSvg);
  return *d;
}

void Brand::drawLogo(juce::Graphics& g, juce::Rectangle<float> box) {
  auto area=box.toNearestInt();
  g.setFont(Fonts::tracked(Fonts::sans(17,true),0.08f));
  g.setColour(theme::kWhite);
  g.drawText("zynamfx",area,juce::Justification::centredLeft,false);
}

void Brand::drawZyMark(juce::Graphics& g, juce::Rectangle<float> box) {
  paint::svg(g, zyMark(), box);
}

void Brand::drawTone3000Logo(juce::Graphics& g, juce::Rectangle<float> box) {
  paint::svg(g, logo(), box);
}

void Brand::drawMark(juce::Graphics& g, juce::Rectangle<float> box) {
  paint::svg(g, mark(), box);
}

void Brand::drawA2Mark(juce::Graphics& g, juce::Rectangle<float> box) {
  paint::svg(g, a2Mark(), box);
}

void Brand::drawVerifiedBadge(juce::Graphics& g, juce::Rectangle<float> box) {
  paint::svg(g, verifiedBadge(), box);
}

}  // namespace t3k::ui
