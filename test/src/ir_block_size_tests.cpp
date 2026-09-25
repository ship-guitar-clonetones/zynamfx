// IR CPU vs the host's *promised* maximum block size (github issue #146).
//
// juce::dsp::Convolution sizes its FFT partition from the maximumBlockSize
// it is prepared with and runs a full FFT pair of that size on every
// process() call. Ardour promises 8192 to every LV2 plugin while running
// 64-sample cycles, so convolvers prepared from the host's promise made one
// IR block cost ~85% of a core. The processor now prepares every convolver
// at kIrConvolverBlockSize and chunks larger blocks (ChainBlock.h), so what
// the host promises must not change what a callback costs.
//
// The partition size is invisible from outside the processor; CPU is its
// only symptom, so that is what this pins. The pinned gap is ~45x (promise
// 8192 vs 64 before the fix) against a ~1x ratio after it, so a 3x bound
// leaves wide margin both ways, and taking the fastest of several runs
// keeps a busy machine from inflating either side.
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>

namespace {

constexpr int kRealBlock = 64;

// Wall time to push `seconds` of audio through a one-IR-block chain in
// kRealBlock callbacks, after the host prepared it with `promisedBlock`.
double secondsToProcess(const char* irFile, const juce::String& gear, int promisedBlock,
                        double seconds) {
  ChainTestProcessor proc;
  proc.setPlayConfigDetails(2, 2, kFs, promisedBlock);
  proc.prepareToPlay(kFs, promisedBlock);

  juce::ValueTree state("ChainSnapshot");
  juce::ValueTree lane("ChainBlocks");
  lane.appendChild(makeIrBlockTree("blk-ir", 1, 100, irFile, gear), nullptr);
  state.appendChild(lane, nullptr);
  proc.restoreFromTree(state);
  EXPECT_TRUE(waitForChainLoaded(proc)) << irFile << " never finished loading";

  const auto noise = makeNoise(kRealBlock, 99, 0.25f);
  juce::AudioBuffer<float> buffer(2, kRealBlock);
  juce::MidiBuffer midi;
  auto pump = [&](int calls) {
    for (int i = 0; i < calls; ++i) {
      buffer.copyFrom(0, 0, noise.data(), kRealBlock);
      buffer.copyFrom(1, 0, noise.data(), kRealBlock);
      proc.processBlock(buffer, midi);
    }
  };

  pump(static_cast<int>(0.25 * kFs / kRealBlock));  // warm-up
  const auto start = std::chrono::steady_clock::now();
  pump(static_cast<int>(seconds * kFs / kRealBlock));
  const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

  float peak = 0.0f;
  for (int ch = 0; ch < 2; ++ch)
    peak = std::max(peak, buffer.getMagnitude(ch, 0, kRealBlock));
  EXPECT_GT(peak, 1e-4f) << irFile << ": chain output is silent";
  return elapsed.count();
}

double fastestOf(int runs, const char* irFile, const juce::String& gear, int promisedBlock) {
  double best = 1e9;
  for (int i = 0; i < runs; ++i)
    best = std::min(best, secondsToProcess(irFile, gear, promisedBlock, 1.0));
  return best;
}

}  // namespace

TEST(IrBlockSizeTest, IrCpuDoesNotScaleWithPromisedMaxBlockSize) {
  struct Ir {
    const char* file;
    const char* gear;
  };
  // Both engines the loader builds: uniform (cab) and NonUniform (reverb).
  for (const Ir ir : {Ir{"cab-ir-test.wav", "cab"}, Ir{"reverb-ir-mono-test.wav", "space"}}) {
    const double honest = fastestOf(3, ir.file, ir.gear, kRealBlock);
    const double ardour = fastestOf(3, ir.file, ir.gear, 8192);
    EXPECT_LT(ardour, honest * 3.0)
        << ir.file << ": 64-sample callbacks took " << ardour << " s when the host promised 8192 vs "
        << honest << " s when it promised 64; convolvers are being sized from the promise again";
  }
}
