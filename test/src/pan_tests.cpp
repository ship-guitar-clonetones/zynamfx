// Per-slot pan resolution tests (see docs/pan-in-a-chain.md).
//
// The shipped control bound every populated slot of a lane to the *same*
// `chainPanLeft`/`chainPanRight` parameter, so all slots moved together (and
// the rail's link pill mirrored one lane onto the other). The design gives
// every populated slot its own stored placement (`panNormalized` + `panSet`)
// and resolves each lane's effective placement as the last placed enabled
// block along that lane's signal path, else the rail fallback.
//
// These tests drive that resolution through the PUBLIC surface only:
//   set  -> ProcessorChain::setBlockParam(id, "pan", v)   (0..1; v < 0 clears)
//   read -> ProcessorChain::getChainState()["panLeft" / "panRight"]
// Nothing here reaches into resolveLanePan, ChainBlock or the internal lanes.
//
// The rig mirrors branch_tests.cpp: chains are seeded through
// setStateInformation with the IR bytes embedded (cache-first, no network),
// and no audio device is opened. A branched/rejoined chain only exercises the
// real resolution path once the RT routing state is live, so each case pumps
// a few callbacks (processStereo) before reading the resolved placement.

#include "Processor.h"
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace {

constexpr int kBlock = 512;

// The resolved lane placement out of getChainState's top-level payload.
double lanePan(const juce::var& state, const char* key) {
  return static_cast<double>(state[key]);
}

// Pump a few callbacks so the processor's RT routing state (rtStereoChains,
// rtBranchTapIndex/rtBranchMergeIndex) is set before the resolved placement
// is read. No assertion on the audio: this only advances the live state.
void pumpAudio(TONE3000Processor& proc, int blocks = 16) {
  (void)processStereo(proc, makeNoise(blocks * kBlock, 4242, 0.05f), kBlock);
}

// Seed a rig, prepare it and wait until every tone block has loaded (the same
// hermetic shape the branch suite uses; no audio device is involved).
void seedPrepared(ChainTestProcessor& proc, const std::vector<juce::String>& leftIds,
                  const std::vector<juce::String>& rightIds) {
  // Play config first, like the branch suite: the restored blocks' engine
  // builds need the live host rate/block size.
  proc.setPlayConfigDetails(2, 2, kFs, kBlock);
  proc.prepareToPlay(kFs, kBlock);
  seedStereoChains(proc, leftIds, rightIds);
  ASSERT_TRUE(waitForChainLoaded(proc)) << "tone blocks never finished loading from cache";
}

// 1. Two pan-set blocks in the SAME lane: the later (downstream) one wins —
//    a serial lane holds one placement.
// 2. Clearing the later one (pan = -1) makes the earlier one win again.
TEST(PanResolve, LastSetSlotInALaneWins) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a", "blk-b"}, {"blk-r"});

  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.20));
  ASSERT_TRUE(proc.setBlockParam("blk-b", "pan", 0.80));
  pumpAudio(proc);
  {
    const juce::var state = proc.getChainState(-1);
    ASSERT_TRUE(state.hasProperty("panLeft"));
    EXPECT_NEAR(lanePan(state, "panLeft"), 0.80, 1e-5)
        << "the downstream slot must place the lane";
  }

  // Clearing the later slot returns the lane to the earlier placement.
  ASSERT_TRUE(proc.setBlockParam("blk-b", "pan", -1.0));
  pumpAudio(proc);
  EXPECT_NEAR(lanePan(proc.getChainState(-1), "panLeft"), 0.20, 1e-5)
      << "clearing the later slot must fall back to the earlier one";
}

// 3. A lane with no pan set falls back to the rail value
//    (chainPanLeft / chainPanRight). The rail is moved off its defaults so
//    the fallback provably reads the rail, not a hardcoded centre.
TEST(PanResolve, UnsetLaneFallsBackToTheRail) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a"}, {"blk-r"});

  proc.parameters.getParameter("chainPanLeft")->setValueNotifyingHost(0.30f);
  proc.parameters.getParameter("chainPanRight")->setValueNotifyingHost(0.70f);
  pumpAudio(proc);  // primes the cached rail values from the atomics

  {
    const juce::var state = proc.getChainState(-1);
    ASSERT_TRUE(state.hasProperty("panLeft"));
    ASSERT_TRUE(state.hasProperty("panRight"));
    EXPECT_NEAR(lanePan(state, "panLeft"), 0.30, 1e-5);
    EXPECT_NEAR(lanePan(state, "panRight"), 0.70, 1e-5);
  }

  // A slot that sets a pan overrides the rail…
  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.90));
  pumpAudio(proc);
  EXPECT_NEAR(lanePan(proc.getChainState(-1), "panLeft"), 0.90, 1e-5);

  // …and clearing it (pan = -1) inherits the rail again.
  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", -1.0));
  pumpAudio(proc);
  EXPECT_NEAR(lanePan(proc.getChainState(-1), "panLeft"), 0.30, 1e-5)
      << "a cleared slot must inherit the rail placement";
}

// 4. Branched chain: a pan on the branch lane resolves INDEPENDENTLY of the
//    trunk lane — each lane keeps its own last-set block.
TEST(PanResolve, BranchLanePanIsIndependentOfTrunk) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a", "blk-b"}, {"blk-r"});

  letAudioGoIdle();
  ASSERT_TRUE(proc.setChainBranch("left", "blk-a"));

  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.40));  // trunk
  ASSERT_TRUE(proc.setBlockParam("blk-r", "pan", 0.80));  // branch lane
  pumpAudio(proc);

  const juce::var state = proc.getChainState(-1);
  EXPECT_NEAR(lanePan(state, "panLeft"), 0.40, 1e-5)
      << "the branch lane's pan must not move the trunk";
  EXPECT_NEAR(lanePan(state, "panRight"), 0.80, 1e-5)
      << "the branch lane resolves its own placement";
}

// 5. A pan-set block in the shared rejoin tail places BOTH lanes: the tail is
//    common after the merge, so it overrides the branch lane's own block.
TEST(PanResolve, SharedRejoinTailPlacesBothLanes) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a", "blk-b", "blk-c"}, {"blk-r"});

  letAudioGoIdle();
  ASSERT_TRUE(proc.setChainBranch("left", "blk-a"));
  ASSERT_TRUE(proc.setChainMerge("blk-c"));  // shared tail = trunk [blk-c]

  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.20));  // tap-side trunk
  ASSERT_TRUE(proc.setBlockParam("blk-r", "pan", 0.90));  // branch lane's own
  ASSERT_TRUE(proc.setBlockParam("blk-c", "pan", 0.40));  // shared tail
  pumpAudio(proc);

  const juce::var state = proc.getChainState(-1);
  EXPECT_NEAR(lanePan(state, "panLeft"), 0.40, 1e-5)
      << "the shared tail wins on the trunk side";
  EXPECT_NEAR(lanePan(state, "panRight"), 0.40, 1e-5)
      << "the shared tail overrides the branch lane's own placement";
}

}  // namespace
