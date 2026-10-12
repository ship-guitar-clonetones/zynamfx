// Original FX prototype outline artwork. Authored on a shared 24px grid.
#pragma once
#include <juce_core/juce_core.h>
namespace t3k::ui::gear {
inline constexpr const char* kAmp = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="5" width="20" height="14" rx="2"/><path d="M8 5V3h8v2M3 10h18M5 19v2m14-2v2"/><circle cx="6" cy="8" r=".6"/><circle cx="10" cy="8" r=".6"/><circle cx="14" cy="8" r=".6"/><path d="M5 13h14m-14 3h14"/></svg>)svg";
inline constexpr const char* kSpeakerCab = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><rect x="5" y="2" width="14" height="20" rx="2"/><circle cx="12" cy="8" r="3.2"/><circle cx="12" cy="16" r="3.2"/><circle cx="12" cy="8" r="1"/><circle cx="12" cy="16" r="1"/></svg>)svg";
inline constexpr const char* kPedal = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="2" width="12" height="20" rx="2"/><circle cx="9" cy="6" r="1"/><circle cx="15" cy="6" r="1"/><path d="M8 10h8v6H8z"/><circle cx="12" cy="19" r="1"/></svg>)svg";
inline constexpr const char* kOutboard = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><rect x="1" y="5" width="22" height="14" rx="2"/><circle cx="7" cy="12" r="3"/><path d="M13 9h6m-6 3h6m-6 3h6M3 8v8m18-8v8"/></svg>)svg";
inline constexpr const char* kFullRig = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="2" width="18" height="7" rx="1"/><rect x="4" y="11" width="16" height="11" rx="1"/><path d="M6 5h12"/><circle cx="8" cy="16.5" r="2.5"/><circle cx="16" cy="16.5" r="2.5"/></svg>)svg";
inline constexpr const char* kSpace = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="m12 2 9 5v10l-9 5-9-5V7zM3 7l9 5 9-5m-9 5v10"/></svg>)svg";
inline constexpr const char* kExperimental = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12h3l3-8 4 16 4-16 3 8h3"/></svg>)svg";
inline constexpr const char* kFileIcon = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M6 2h8l5 5v15H6zM14 2v6h5M9 12h7m-7 4h7"/></svg>)svg";
inline constexpr const char* kIr = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12h4l3-9 4 18 3-9h6"/></svg>)svg";
inline const char* svgFor(const juce::String& gearId) {
 const auto id=gearId.toLowerCase();
 if(id=="amp-cab" || id=="full-rig") return kFullRig;
 if(id=="pedal") return kPedal;
 if(id=="cab") return kSpeakerCab;
 if(id=="ir") return kIr;
 if(id=="outboard") return kOutboard;
 if(id=="space") return kSpace;
 if(id=="experimental") return kExperimental;
 return kAmp;
}
} // namespace t3k::ui::gear
