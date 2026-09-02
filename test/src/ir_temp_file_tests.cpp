// IR loader temp-file hygiene (github issue #25). Every IR engine build
// round-trips the model bytes through a "*_ir.wav" file in the OS temp dir;
// builds through v0.0.2 never deleted it, leaving hundreds of MB behind after
// a session of preset browsing. Pins that (a) loads clean up after
// themselves, on success and on failure, and (b) the startup sweep removes
// older builds' leftovers without touching in-flight or foreign files.

#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <set>
#include <vector>

namespace {

juce::File osTempDir() { return juce::File::getSpecialLocation(juce::File::tempDirectory); }

// Leaf names of every "*_ir.wav" in the OS temp dir: the loader's file
// pattern, old (uuid) and new (TemporaryFile) naming alike.
std::set<juce::String> irTempLeaves() {
  std::set<juce::String> names;
  for (const auto& f : osTempDir().findChildFiles(juce::File::findFiles, false, "*_ir.wav"))
    names.insert(f.getFileName());
  return names;
}

// True once no "*_ir.wav" beyond `before` remains in the OS temp dir. A
// leaked file sits there forever, so waiting out transients (another test
// process mid-load) can never mask a real leak; on timeout the stragglers
// are reported by name.
bool noNewIrTempFilesSettle(const std::set<juce::String>& before, int timeoutMs = 10000) {
  const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);
  for (;;) {
    std::vector<juce::String> added;
    for (const auto& name : irTempLeaves())
      if (before.count(name) == 0)
        added.push_back(name);
    if (added.empty())
      return true;
    if (juce::Time::getMillisecondCounter() >= deadline) {
      for (const auto& name : added)
        ADD_FAILURE() << "leaked IR temp file: " << name.toStdString();
      return false;
    }
    juce::Thread::sleep(50);
  }
}

// Restore a mono chain of the given blocks through the real state path.
void restoreMonoChain(ChainTestProcessor& proc, std::initializer_list<juce::ValueTree> blocks) {
  juce::ValueTree state("ChainSnapshot");
  state.setProperty("stereoEnabled", false, nullptr);
  juce::ValueTree lane("ChainBlocks");
  for (const auto& block : blocks)
    lane.appendChild(block, nullptr);
  state.appendChild(lane, nullptr);
  state.appendChild(juce::ValueTree("RightChainBlocks"), nullptr);
  proc.restoreFromTree(state);
}

// An IR block whose ModelCache holds unreadable bytes: the loader writes
// them to its temp file, fails to open a reader, and must still clean up.
juce::ValueTree makeJunkIrBlockTree(const juce::String& blockId, int toneId, int modelId) {
  juce::ValueTree block = makeIrBlockTree(blockId, toneId, modelId);
  juce::MemoryBlock junk(256);
  junk.fillWith(0x5a);  // no RIFF header, so no WAV reader will open it
  block.getChildWithName("ModelCache").getChild(0).setProperty("data", juce::var(junk), nullptr);
  return block;
}

// Wait until the block reports loadFailed through the UI-facing chain state.
bool waitForBlockLoadFailed(TONE3000Processor& proc, const juce::String& blockId,
                            int timeoutMs = 20000) {
  const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);
  while (juce::Time::getMillisecondCounter() < deadline) {
    const juce::var state = proc.getChainState(-1);
    if (const auto* lane = state["chain"].getArray())
      for (const auto& item : *lane)
        if (item["blockId"].toString() == blockId && static_cast<bool>(item["loadFailed"]))
          return true;
    juce::Thread::sleep(20);
  }
  return false;
}

}  // namespace

TEST(IrTempFileTest, SuccessfulIrLoadsLeaveNoTempFiles) {
  const auto before = irTempLeaves();

  ChainTestProcessor proc;
  // A short cab IR and a long stereo reverb IR: the reverb also builds the
  // true-stereo convolver, the second file-reading path in the loader.
  restoreMonoChain(proc, {makeIrBlockTree("blk-cab", 1, 100, "cab-ir-test.wav"),
                          makeIrBlockTree("blk-verb", 2, 101, "reverb-ir-stereo-test.wav")});
  ASSERT_TRUE(waitForChainLoaded(proc));

  EXPECT_TRUE(noNewIrTempFilesSettle(before));
}

TEST(IrTempFileTest, FailedIrLoadLeavesNoTempFile) {
  const auto before = irTempLeaves();

  ChainTestProcessor proc;
  restoreMonoChain(proc, {makeJunkIrBlockTree("blk-junk", 1, 100)});
  ASSERT_TRUE(waitForBlockLoadFailed(proc, "blk-junk"));

  EXPECT_TRUE(noNewIrTempFilesSettle(before));
}

TEST(IrTempFileTest, SweepRemovesOnlyStaleIrTempFiles) {
  // A private directory stands in for the OS temp dir, so the test owns
  // every file the sweep sees (the once-per-process startup sweep scans only
  // the temp root, never this subdirectory).
  const juce::File dir = osTempDir().getChildFile("t3k-sweep-test-" + juce::Uuid().toString());
  ASSERT_TRUE(dir.createDirectory());

  const auto makeFileAgedHours = [&dir](const juce::String& name, double hours) {
    const juce::File f = dir.getChildFile(name);
    EXPECT_TRUE(f.replaceWithText("x"));
    if (hours > 0)
      EXPECT_TRUE(f.setLastModificationTime(juce::Time::getCurrentTime() -
                                            juce::RelativeTime::hours(hours)));
    return f;
  };

  // The two leaked shapes: an old build's uuid-named file and a crashed
  // build's TemporaryFile, both well past the hour age guard.
  const juce::File leaked = makeFileAgedHours(juce::Uuid().toString() + "_ir.wav", 2.0);
  const juce::File crashLeftover = makeFileAgedHours("temp_ab12cd34_ir.wav", 2.0);
  // A concurrent instance's in-flight file (fresh) and a stale file that
  // merely lives in the same directory: both must survive.
  const juce::File inFlight = makeFileAgedHours(juce::Uuid().toString() + "_ir.wav", 0.0);
  const juce::File foreign = makeFileAgedHours("somebody-elses.wav", 2.0);

  EXPECT_EQ(TONE3000Processor::sweepLeakedIrTempFiles(dir), 2);
  EXPECT_FALSE(leaked.existsAsFile());
  EXPECT_FALSE(crashLeftover.existsAsFile());
  EXPECT_TRUE(inFlight.existsAsFile());
  EXPECT_TRUE(foreign.existsAsFile());

  EXPECT_TRUE(dir.deleteRecursively());
}
