// Public build identity is supplied by CMake for normal builds. The in-app logo
// link and Settings footer point to the public project. The fallbacks keep
// source-only compiles (the UI testbed, unit tests) working.
#pragma once

#include <juce_core/juce_core.h>

#ifndef T3K_REPO_URL
#define T3K_REPO_URL "https://github.com/zackyusof/zynamfx"
#endif
#ifndef T3K_BUILD_CHANNEL
#define T3K_BUILD_CHANNEL "public"
#endif
#ifndef T3K_VERSION
#define T3K_VERSION ""
#endif
#ifndef T3K_PRERELEASE
#define T3K_PRERELEASE ""
#endif

namespace t3k::ui::build {

// "public" for this public distribution.
inline juce::String channel() { return T3K_BUILD_CHANNEL; }
// The repository this build was produced from.
inline juce::String repoUrl() { return T3K_REPO_URL; }
// The product version (from the repo-root VERSION file); empty in a
// source-only compile that does not define it.
inline juce::String version() { return T3K_VERSION; }
// The full label shown to users and reported to support, e.g. "2.0.2-beta.1". The prerelease part is
// empty in a stable build, so this equals version() there.
inline juce::String displayVersion() {
  const juce::String pre(T3K_PRERELEASE);
  return pre.isEmpty() ? version() : version() + "-" + pre;
}
// One-line identity for the Settings footer, e.g.
// "public build (github.com/zackyusof/zynamfx)".
inline juce::String label() {
  return juce::String(T3K_BUILD_CHANNEL) + " build (" + T3K_REPO_URL + ")";
}

}  // namespace t3k::ui::build
