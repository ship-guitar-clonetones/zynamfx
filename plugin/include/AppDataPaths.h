#pragma once

#include <juce_core/juce_core.h>

#include <mutex>
#include <vector>

#if !JUCE_WINDOWS
#include <cstdio>
#include <sys/stat.h>
#endif

// Fork distribution identity (cmake/ForkIdentity.cmake): app-data folder name.
// The CMake build always defines it; this fallback keeps any other build on
// upstream (same guard as Processor.h / PresetManager.h).
#ifndef T3K_APP_DATA_DIR_NAME
#define T3K_APP_DATA_DIR_NAME "zynamfx"
#endif

/**
 * The one resolver for the per-user app-data root.
 *
 * Everything that used to derive the folder on its own — the machine-wide
 * settings file and the native UI's preferences (ProcessorState.cpp), the
 * LocalModels drop stash (ProcessorModelLoader.cpp) and the user presets
 * (PresetManager.cpp) — now asks this header, so the four can never drift
 * apart, and a repaired/fallen-back root moves all of them together.
 *
 * Why this exists (github issue #76, user report "the config doesn't stick"):
 * the root was a SYMLINK to an unmounted volume. `juce::File::exists()` stats
 * *through* the link, so a dangling link reads as "nothing here"; the old heal
 * code therefore skipped its move-aside branch, called createDirectory() and
 * got "File exists" on every launch, silently, forever: no settings, no API
 * key, no LocalModels stash, while the log still claimed the folder was
 * writable. Two rules fix that class of failure:
 *
 *   1. a path node that EXISTS AS A NODE but is not a usable directory
 *      (dangling symlink, squatting file, foreign-owned folder) is moved
 *      aside and replaced — never deleted, never left to fail silently;
 *   2. "writable" is a PROBE (create + write + delete a temp file), never an
 *      access() guess; if the primary root cannot be made writable, a
 *      secondary local root keeps settings + ui-preferences + presets +
 *      LocalModels together instead of dropping them on the floor.
 */
namespace t3k::appdata {

struct ProbeResult {
  bool writable = false;
  juce::String reason;  // empty when writable
};

struct RootResolution {
  juce::File dir;             // the folder to use, primary whenever it works
  bool usedFallback = false;  // true when the secondary local root is in use
  ProbeResult probe;          // probe of `dir` at resolution time
  ProbeResult primaryProbe;   // probe of the primary root at resolution time
};

// Diagnostics buffered until a logger exists: PresetManager (a processor
// member) resolves the root before the constructor body has attached the
// FileLogger, so writing straight to the logger there would lose every
// repair line. Buffered lines are flushed once, by logResolutionOnce().
namespace detail {

struct State {
  std::mutex mutex;
  std::vector<juce::String> pending;
  bool flushed = false;
  bool resolved = false;
  RootResolution resolution;
};

inline State& state() {
  static State s;
  return s;
}

inline void note(const juce::String& line) {
  auto& s = state();
  const std::scoped_lock lock(s.mutex);
  // Before the FileLogger exists (the processor's members resolve the root
  // during member initialisation) keep the lines; logResolutionOnce() replays
  // them once a logger is installed, so a repair is never invisible.
  if (!s.flushed && juce::Logger::getCurrentLogger() == nullptr) {
    s.pending.push_back(line);
    return;
  }
  juce::Logger::writeToLog(line);
}

#if !JUCE_WINDOWS
// rename(2) moves the NODE ITSELF. That is the whole point for a dangling
// symlink: juce::File::moveFileTo() refuses a source whose exists() is false,
// which is exactly how a dangling link reads.
inline bool renameNode(const juce::File& from, const juce::File& to) {
  return ::rename(from.getFullPathName().toRawUTF8(), to.getFullPathName().toRawUTF8()) == 0;
}
#else
inline bool renameNode(const juce::File& from, const juce::File& to) {
  return from.moveFileTo(to);
}
#endif

}  // namespace detail

inline juce::File userDataBase() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
}

// ~/Library/Application Support/<app> on macOS, %APPDATA%/<app> on Windows,
// ~/.config/<app> on Linux — the historical location, still the default.
inline juce::File primaryRoot() {
  juce::File base = userDataBase();
#if JUCE_MAC
  base = base.getChildFile("Application Support");
#endif
  return base.getChildFile(T3K_APP_DATA_DIR_NAME);
}

// <userApplicationDataDirectory>/<app>-local: a sibling that does not depend
// on the primary path (or anything mounted under /Volumes) being usable.
inline juce::File fallbackRoot() {
  return userDataBase().getChildFile(juce::String(T3K_APP_DATA_DIR_NAME) + "-local");
}

// The real test: create a small file, write to it, delete it. An access()
// check (juce::File::hasWriteAccess) says yes on a dangling/unmounted path's
// parent, on a full disk and on some ACL setups where every write then fails.
inline ProbeResult probeWritable(const juce::File& dir) {
  if (!dir.isDirectory())
    return {false, dir.getFullPathName() + " is not a directory"};

  const juce::File probe =
      dir.getChildFile(".write-probe-" + juce::Uuid().toString() + ".tmp");
  if (!probe.replaceWithText("zynamfx write probe"))
    return {false, "cannot create a file in " + dir.getFullPathName()};
  if (!probe.deleteFile())
    return {false, "cannot delete a file in " + dir.getFullPathName()};
  return {true, {}};
}

/**
 * Make `dir` a writable directory, or report honestly that it isn't.
 *
 * Never deletes: a node that must go is renamed to a "<name>.unwritable"
 * (or numbered) sibling and the log names it. Returns true only when the
 * write probe succeeds on exit.
 */
inline bool ensureWritableDir(const juce::File& dir) {
  const ProbeResult initial = probeWritable(dir);
  if (initial.writable)
    return true;

  if (dir.isDirectory() && dir.hasWriteAccess()) {
    // Structurally fine, but the probe still failed (full disk, read-only
    // mount, ACL). Nothing unprivileged can be fixed here, and moving a
    // healthy folder with the user's data in it aside would be destructive.
    detail::note("[AppData] " + dir.getFullPathName() +
                 " reports write access but the write probe failed (" + initial.reason +
                 "); leaving it in place");
    return false;
  }

  // A folder the user still owns with stripped write bits (restored backup)
  // is fixable in place with a chmod, which keeps its contents where they
  // are. chmod is owner-only, so this quietly does nothing to a root-owned
  // folder, which falls through to the rename below. The owner's write bit
  // only: juce::File::setReadOnly(false) would also grant group and world
  // (0555 -> 0777), and the folder holds the user's sign-in tokens.
  const auto restoreOwnerWrite = [&dir] {
#if JUCE_WINDOWS
    return dir.setReadOnly(false);
#else
    struct stat st {};
    const juce::String path = dir.getFullPathName();
    return ::stat(path.toRawUTF8(), &st) == 0 &&
           ::chmod(path.toRawUTF8(), (st.st_mode & 07777) | S_IWUSR) == 0;
#endif
  };
  if (dir.isDirectory() && restoreOwnerWrite() && probeWritable(dir).writable) {
    detail::note("[AppData] Restored write permission on " + dir.getFullPathName());
    return true;
  }

  // What chmod can't fix: a file squatting on the path, a directory owned by
  // someone else (root, after a sudo'd install script), or a DANGLING
  // SYMLINK — a link to a volume that isn't mounted. exists() is false for
  // the last one (it stats through the link), so it needs isSymbolicLink()
  // to be seen at all; without that check createDirectory() reports
  // "File exists" and nothing ever persists.
  const bool isLink = dir.isSymbolicLink();
  if (dir.exists() || isLink) {
    // Read the link target BEFORE the rename: afterwards readlink() on the old
    // path fails and the log would name the wrong thing.
    const juce::String linkTarget =
        isLink ? dir.getLinkedTarget().getFullPathName() : juce::String();
    const juce::File aside =
        dir.getSiblingFile(dir.getFileName() + ".unwritable").getNonexistentSibling();
    if (detail::renameNode(dir, aside)) {
      const juce::String targetNote =
          isLink ? " (was a symlink to " + linkTarget + ")" : juce::String();
      detail::note("[AppData] Moved unusable " + dir.getFullPathName() + " aside to " +
                   aside.getFullPathName() + targetNote);
    } else {
      detail::note("[AppData] " + dir.getFullPathName() +
                   " is not writable and couldn't be moved aside");
    }
  }

  const juce::Result created = dir.createDirectory();
  if (created.failed()) {
    detail::note("[AppData] Couldn't create " + dir.getFullPathName() + ": " +
                 created.getErrorMessage());
    return false;
  }

  const ProbeResult after = probeWritable(dir);
  if (after.writable)
    detail::note("[AppData] Created " + dir.getFullPathName());
  else
    detail::note("[AppData] Created " + dir.getFullPathName() +
                 " but the write probe failed (" + after.reason + ")");
  return after.writable;
}

namespace detail {

inline RootResolution resolve() {
  RootResolution r;
  const juce::File primary = primaryRoot();

  // Repairable damage is fixed in place here; the returned flag is already a
  // successful write probe, so the healthy path costs exactly one probe.
  if (ensureWritableDir(primary)) {
    r.dir = primary;
    r.primaryProbe = {true, {}};
    r.probe = r.primaryProbe;
    note("[AppData] dataDir=" + r.dir.getFullPathName() + " writable (probe ok)");
    return r;
  }
  r.primaryProbe = probeWritable(primary);

  const juce::File fallback = fallbackRoot();
  if (ensureWritableDir(fallback)) {
    r.dir = fallback;
    r.usedFallback = true;
    r.probe = {true, {}};
    note("[AppData] dataDir=NOT writable at " + primary.getFullPathName() + " (" +
         r.primaryProbe.reason + ")");
    note("[AppData] Using fallback dataDir=" + fallback.getFullPathName() +
         " for settings, ui-preferences, presets and LocalModels; fix the primary folder "
         "and relaunch to move back");
    return r;
  }
  const ProbeResult fallbackProbe = probeWritable(fallback);

  // Both unusable: still hand back a path so the app runs (writes just fail
  // and say so), never crash and never fake writability.
  r.dir = primary;
  r.probe = r.primaryProbe;
  note("[AppData] dataDir=NOT writable at " + primary.getFullPathName() + " (" +
       r.primaryProbe.reason + ")");
  note("[AppData] Fallback dataDir=" + fallback.getFullPathName() +
       " is not writable either (" + fallbackProbe.reason +
       "); settings will not persist this session");
  return r;
}

}  // namespace detail

// Resolved once per process. All four consumers (settings, ui-preferences,
// presets, LocalModels) get the same answer.
inline const RootResolution& root() {
  static const RootResolution resolution = [] {
    RootResolution r = detail::resolve();
    auto& st = detail::state();
    const std::scoped_lock lock(st.mutex);
    st.resolution = r;
    st.resolved = true;
    return r;
  }();
  return resolution;
}

// Emit the buffered resolution/repair lines once, through the logger that is
// current now (the FileLogger exists by the time the processor constructor
// body asks for the settings file). A no-op while no logger is installed, so
// the lines are not lost to the default debug sink during member init.
inline void logResolutionOnce() {
  (void)root();  // resolve now if nobody has yet
  if (juce::Logger::getCurrentLogger() == nullptr)
    return;

  auto& s = detail::state();
  std::vector<juce::String> lines;
  {
    const std::scoped_lock lock(s.mutex);
    if (s.flushed || !s.resolved)
      return;
    s.flushed = true;
    lines.swap(s.pending);
  }
  for (const auto& line : lines)
    juce::Logger::writeToLog(line);
}

}  // namespace t3k::appdata
