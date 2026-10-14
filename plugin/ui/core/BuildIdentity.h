// Build identity: which repository this build came from and its channel.
// Substituted at configure time (plugin/CMakeLists.txt -> cmake/ForkIdentity.cmake):
//   development (private repository)  T3K_FORK_REPO_URL=https://github.com/zackyusof/zynamfx
//   public      (GitHub) T3K_FORK_REPO_URL=https://github.com/zackyusof/zynamfx
// The in-app logo link and the Settings footer both read these, so a private repository build
// points at private repository and a GitHub build points at GitHub without editing source.
// The fallbacks keep source-only compiles (the UI testbed, unit tests) working.
#pragma once

#include <juce_core/juce_core.h>

#ifndef T3K_REPO_URL
#define T3K_REPO_URL "https://github.com/zackyusof/zynamfx"
#endif
#ifndef T3K_BUILD_CHANNEL
#define T3K_BUILD_CHANNEL "development"
#endif
#ifndef T3K_VERSION
#define T3K_VERSION ""
#endif

namespace t3k::ui::build {

// "development" (private repository) or "public" (GitHub).
inline juce::String channel() { return T3K_BUILD_CHANNEL; }
// The repository this build was produced from.
inline juce::String repoUrl() { return T3K_REPO_URL; }
// The product version (from the repo-root VERSION file); empty in a
// source-only compile that does not define it.
inline juce::String version() { return T3K_VERSION; }
// One-line identity for the Settings footer, e.g.
// "development build (github.com/zackyusof/zynamfx)".
inline juce::String label() {
  return juce::String(T3K_BUILD_CHANNEL) + " build (" + T3K_REPO_URL + ")";
}

}  // namespace t3k::ui::build
