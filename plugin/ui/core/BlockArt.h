#pragma once
#include "Theme.h"
#include "GearGlyphs.h"
#include "Icons.h"
namespace t3k::ui::blockart {
inline juce::Colour colour(const juce::String& gearId, bool effect=false) {
 if(effect) return theme::kEffect;
 if(gearId=="cab" || gearId=="ir") return theme::kCab;
 if(gearId=="pedal") return theme::kDrive;
 if(gearId=="space" || gearId=="outboard" || gearId=="experimental") return theme::kAccent;
 return theme::kAmp;
}
inline juce::String label(const juce::String& gearId, bool effect=false) {
 if(effect) return "HOSTED FX";
 if(gearId=="cab") return "CABINET";
 if(gearId=="ir") return "IMPULSE";
 if(gearId=="pedal") return "DRIVE";
 if(gearId=="amp-cab" || gearId=="full-rig") return "AMP + CAB";
 if(gearId=="outboard") return "OUTBOARD";
 if(gearId=="space") return "SPACE";
 if(gearId=="experimental") return "MODEL";
 return "AMPLIFIER";
}
inline void draw(juce::Graphics& g,juce::Rectangle<float> box,const juce::String& gearId,bool effect=false) {
 Icons::draw(g,effect ? gear::kOutboard : gear::svgFor(gearId),box,colour(gearId,effect));
}
} // namespace t3k::ui::blockart
