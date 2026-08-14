// Auto-align probe tests
//
// The probe measurement (AutoOffset.h): the state machine must fade, sweep,
// capture and unmute on its fixed schedule, and the soft-PHAT estimator must
// recover integer and fractional inter-chain lags through gain/voicing
// differences and polarity inversion, rejecting a lag beyond what the
// Offset knob can express. The last tests close the loop: once through the
// real StereoOffset engine, and once through a real processor with
// different NAM/IR chains per lane, where the probe must produce the same
// answer run after run.
#include "AutoOffset.h"
#include "StereoOffset.h"
#include "chain_test_helpers.h"
#include "test_helpers.h"

#include <gtest/gtest.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace {

constexpr int kBlock = 512;
constexpr float kMsPerSample = 1000.0f / static_cast<float>(kFs);

// Unit impulse at `delay` samples, scaled by `gain`: a pure-delay "chain".
std::vector<float> impulseFir(int delay, float gain = 1.0f) {
  std::vector<float> h(static_cast<size_t>(delay) + 1, 0.0f);
  h.back() = gain;
  return h;
}

// Fractional delay: Blackman-windowed sinc centered at `delay`. The kernel
// is symmetric about `delay`, so its phase is exactly linear and the group
// delay exact at every frequency; only the magnitude ripples near Nyquist,
// which cannot move a correlation peak.
std::vector<float> fractionalDelayFir(double delay, int halfWidth = 32) {
  const int lo = static_cast<int>(std::floor(delay)) - halfWidth;
  const int hi = static_cast<int>(std::ceil(delay)) + halfWidth;
  EXPECT_GE(lo, 0) << "kernel would need negative taps";
  std::vector<float> h(static_cast<size_t>(hi) + 1, 0.0f);
  for (int k = lo; k <= hi; ++k) {
    const double x = k - delay;
    const double sinc = x == 0.0 ? 1.0 : std::sin(kPi * x) / (kPi * x);
    const double t = x / (halfWidth + 1);
    const double w = 0.42 + 0.5 * std::cos(kPi * t) + 0.08 * std::cos(2.0 * kPi * t);
    h[static_cast<size_t>(k)] = static_cast<float>(sinc * w);
  }
  return h;
}

// One-pole lowpass as a truncated FIR, a stand-in for "differently voiced
// chain" (decayed far below float noise by 256 taps at fc = 1 kHz).
std::vector<float> onePoleLpFir(double fc, int taps = 256) {
  const double a = std::exp(-2.0 * kPi * fc / kFs);
  std::vector<float> h(static_cast<size_t>(taps));
  double c = 1.0 - a;
  for (auto& tap : h) {
    tap = static_cast<float>(c);
    c *= a;
  }
  return h;
}

// h shifted right by `delay` samples (h convolved with a delayed impulse).
std::vector<float> delayedFir(const std::vector<float>& h, int delay) {
  std::vector<float> out(static_cast<size_t>(delay), 0.0f);
  out.insert(out.end(), h.begin(), h.end());
  return out;
}

std::vector<float> scaledFir(std::vector<float> h, float gain) {
  for (auto& tap : h)
    tap *= gain;
  return h;
}

// Runs the full probe schedule against a synthetic chain pair, each an FIR
// applied to the probe stream (renderProbeInput / captureChainOutputs /
// applyOutputGain per block, like processBlock does), and returns the
// analysis.
AutoOffset::Result measure(const std::vector<float>& firL, const std::vector<float>& firR) {
  AutoOffset ao;
  ao.prepare(kFs);
  ao.arm();

  std::vector<float> probe;
  std::vector<float> block(kBlock), outL(kBlock), outR(kBlock);
  juce::AudioBuffer<float> host(2, kBlock);

  for (int guard = 0; guard < 200 && ao.state() != AutoOffset::State::Captured; ++guard) {
    if (ao.renderProbeInput(block.data(), kBlock)) {
      const size_t base = probe.size();
      probe.insert(probe.end(), block.begin(), block.end());
      auto fir = [&](const std::vector<float>& h, size_t n) {
        double acc = 0.0;
        const size_t kMax = std::min(h.size() - 1, n);
        for (size_t k = 0; k <= kMax; ++k)
          acc += static_cast<double>(h[k]) * probe[n - k];
        return static_cast<float>(acc);
      };
      for (int i = 0; i < kBlock; ++i) {
        outL[static_cast<size_t>(i)] = fir(firL, base + static_cast<size_t>(i));
        outR[static_cast<size_t>(i)] = fir(firR, base + static_cast<size_t>(i));
      }
      ao.captureChainOutputs(outL.data(), outR.data(), kBlock);
    }
    host.clear();
    ao.applyOutputGain(host);  // advances FadeOut -> Probing and the mute
  }

  EXPECT_EQ(ao.state(), AutoOffset::State::Captured) << "schedule never completed";
  return ao.analyze();
}

TEST(AutoOffsetTest, MeasuresIntegerDelaysBothDirections) {
  // Left chain lags by 150 samples: delay the RIGHT chain, positive ms in
  // the StereoOffset convention. Sub-sample estimator, so the tolerance is
  // a twentieth of a sample.
  {
    const auto result = measure(impulseFir(150), impulseFir(0));
    EXPECT_NEAR(result.offsetMs, 150.0f * kMsPerSample, 0.05f * kMsPerSample);
    EXPECT_FALSE(result.inverted);
    EXPECT_GT(result.confidence, 0.9f);
    EXPECT_GT(result.peakSharpness, 3.0f);
  }
  // Right chain lags by 112: delay the LEFT chain, negative ms.
  {
    const auto result = measure(impulseFir(0), impulseFir(112));
    EXPECT_NEAR(result.offsetMs, -112.0f * kMsPerSample, 0.05f * kMsPerSample);
    EXPECT_FALSE(result.inverted);
    EXPECT_GT(result.confidence, 0.9f);
  }
}

TEST(AutoOffsetTest, RecoversFractionalDelayAndRepeatsExactly) {
  // A 37.36-sample lag: the band-limited sub-sample refinement must land
  // within a twentieth of a sample, and the deterministic schedule must
  // produce the identical answer on a second run.
  const auto firL = fractionalDelayFir(37.36);
  const auto firR = impulseFir(0);

  const auto first = measure(firL, firR);
  EXPECT_NEAR(first.offsetMs, 37.36f * kMsPerSample, 0.05f * kMsPerSample);
  EXPECT_GT(first.confidence, 0.9f);

  const auto second = measure(firL, firR);
  EXPECT_EQ(first.offsetMs, second.offsetMs);
}

TEST(AutoOffsetTest, GainAndVoicingMismatchDoNotMoveTheEstimate) {
  // One chain bright, the other 1 kHz-lowpassed and lagging 200 samples,
  // then the same pair with a 48 dB relative level mismatch. PHAT weighting
  // normalizes each bin's magnitude, so the level must not move the
  // estimate at all; the lowpass may contribute a couple of samples of its
  // own group delay (that is real, measurable delay).
  const auto voiced = delayedFir(onePoleLpFir(1000.0), 200);
  const auto flat = impulseFir(0);

  const auto even = measure(voiced, flat);
  EXPECT_NEAR(even.offsetMs, 200.0f * kMsPerSample, 0.2f);
  EXPECT_GT(even.peakSharpness, 3.0f);

  const auto mismatched = measure(scaledFir(voiced, 16.0f), scaledFir(flat, 1.0f / 16.0f));
  EXPECT_NEAR(mismatched.offsetMs, even.offsetMs, 1.0e-4f);
}

TEST(AutoOffsetTest, MeasuresInvertedChains) {
  // Captures don't share a polarity convention, so one chain can arrive
  // 180° out. The magnitude peak search must still land on the true lag and
  // report the inversion at full confidence.
  {
    const auto result = measure(impulseFir(0), impulseFir(150, -1.0f));
    EXPECT_NEAR(result.offsetMs, -150.0f * kMsPerSample, 0.05f * kMsPerSample);
    EXPECT_TRUE(result.inverted);
    EXPECT_GT(result.confidence, 0.9f);
  }
  // Perfectly aligned but inverted: zero offset, inversion reported.
  {
    const auto result = measure(impulseFir(0), impulseFir(0, -1.0f));
    EXPECT_NEAR(result.offsetMs, 0.0f, 0.05f * kMsPerSample);
    EXPECT_TRUE(result.inverted);
    EXPECT_GT(result.confidence, 0.9f);
  }
}

TEST(AutoOffsetTest, RejectsWhenTheLagIsBeyondTheKnobRange) {
  // 40 ms of true lag sits outside the ±24 ms search window, so the best
  // in-window peak is junk; the sharpness gate the processor checks must
  // reject it.
  const int lag = static_cast<int>(std::round(0.040 * kFs));
  const auto result = measure(impulseFir(lag), impulseFir(0));

  EXPECT_LT(result.peakSharpness, 2.0f) << "confidence " << result.confidence;
}

TEST(AutoOffsetTest, OutputMutesOnScheduleAndRampsBackAfterResume) {
  AutoOffset ao;
  ao.prepare(kFs);

  juce::AudioBuffer<float> ones(2, kBlock);
  auto refill = [&] {
    for (int ch = 0; ch < 2; ++ch)
      juce::FloatVectorOperations::fill(ones.getWritePointer(ch), 1.0f, kBlock);
  };

  // Idle: untouched. No probe before the fade lands either.
  refill();
  ao.applyOutputGain(ones);
  EXPECT_EQ(ones.getSample(0, kBlock - 1), 1.0f);

  ao.arm();
  std::vector<float> probeBlock(kBlock);
  EXPECT_FALSE(ao.renderProbeInput(probeBlock.data(), kBlock)) << "probe must wait for the fade";

  // FadeOut: 5 ms at 48 kHz lands inside one 512-sample block; the block
  // ends silent and the probe owns the next one.
  refill();
  ao.applyOutputGain(ones);
  EXPECT_EQ(ones.getSample(0, kBlock - 1), 0.0f);
  EXPECT_EQ(ao.state(), AutoOffset::State::Probing);

  // Probing/Tail: hard mute, deterministic progress to Captured.
  while (ao.state() == AutoOffset::State::Probing || ao.state() == AutoOffset::State::Tail) {
    ASSERT_TRUE(ao.renderProbeInput(probeBlock.data(), kBlock));
    ao.captureChainOutputs(probeBlock.data(), probeBlock.data(), kBlock);
    refill();
    ao.applyOutputGain(ones);
    EXPECT_EQ(ones.getSample(1, kBlock - 1), 0.0f);
  }
  EXPECT_EQ(ao.state(), AutoOffset::State::Captured);
  EXPECT_EQ(ao.progress(), 1.0f);

  // Captured/Analyzing: live signal may run the chains again (probe is
  // done) but the output stays muted until resume().
  EXPECT_FALSE(ao.renderProbeInput(probeBlock.data(), kBlock));
  const auto result = ao.analyze();
  EXPECT_GT(result.confidence, 0.99f);  // captured itself on both channels
  refill();
  ao.applyOutputGain(ones);
  EXPECT_EQ(ones.getSample(0, kBlock - 1), 0.0f);
  EXPECT_EQ(ao.state(), AutoOffset::State::Analyzing);

  // RampBack: 10 ms lands inside one block; back to Idle, gain restored.
  ao.resume();
  refill();
  ao.applyOutputGain(ones);
  EXPECT_EQ(ones.getSample(0, kBlock - 1), 1.0f);
  EXPECT_EQ(ao.state(), AutoOffset::State::Idle);
}

TEST(AutoOffsetTest, EndToEndMeasureThenAlignThroughStereoOffset) {
  // The full loop, exactly as pollAutoOffset applies it: measure a
  // 300-sample left-chain lag, map the result onto the knob's normalized
  // value, run a misaligned noise pair through the real StereoOffset; the
  // residual lag between the outputs must be zero.
  constexpr int kLag = 300;
  const auto result = measure(impulseFir(kLag), impulseFir(0));
  ASSERT_GT(result.confidence, 0.9f);

  const float norm = juce::jlimit(
      0.0f, 1.0f, 0.5f + result.offsetMs / (2.0f * StereoOffsetParams::kMaxOffsetMs));

  const auto x = makeNoise(440 * kBlock, 47, 0.5f);
  StereoOffset offset;
  offset.prepare(kFs, kBlock);
  juce::AudioBuffer<float> buf(2, kBlock);
  std::vector<float> outL, outR;
  for (size_t off = 0; off + kBlock <= x.size(); off += kBlock) {
    for (int i = 0; i < kBlock; ++i) {
      const size_t n = off + static_cast<size_t>(i);
      buf.setSample(0, i, n >= kLag ? x[n - kLag] : 0.0f);
      buf.setSample(1, i, x[n]);
    }
    offset.setTarget(StereoOffsetParams::fromNormalized(norm), true);
    offset.process(buf);
    for (int i = 0; i < kBlock; ++i) {
      outL.push_back(buf.getSample(0, i));
      outR.push_back(buf.getSample(1, i));
    }
  }

  // Well past the 40 ms delay glide-in; residual lag must be zero in both
  // directions (bestCorrelationLag only searches one way).
  const int start = static_cast<int>(kFs);
  EXPECT_EQ(bestCorrelationLag(outL, outR, start, 8 * kBlock, kLag), 0);
  EXPECT_EQ(bestCorrelationLag(outR, outL, start, 8 * kBlock, kLag), 0);
}

// Drives startAutoOffset through the real processor (probe injection,
// chain rendering, capture tap, mute stage, message-thread poll) and
// returns the terminal poll payload.
juce::var runProbe(TONE3000Processor& proc) {
  proc.startAutoOffset();
  juce::AudioBuffer<float> buffer(2, kBlock);
  juce::MidiBuffer midi;
  for (int block = 0; block < 400; ++block) {
    buffer.clear();
    proc.processBlock(buffer, midi);
    const juce::var poll = proc.pollAutoOffset();
    const juce::String state = poll["state"].toString();
    if (state == "done" || state == "timeout")
      return poll;
  }
  return {};
}

TEST(AutoOffsetTest, ProbeAlignsRealNamAndIrChains) {
  // Two genuinely different rigs: Left a full amp+cab capture, Right a
  // different amp head into a separate cab IR. The probe must complete
  // silently on schedule, produce an in-range offset, and (the point of a
  // deterministic stimulus) repeat within a tenth of a sample.
  ChainTestProcessor proc;
  proc.setPlayConfigDetails(2, 2, kFs, kBlock);
  proc.prepareToPlay(kFs, kBlock);

  juce::ValueTree state("ChainSnapshot");
  state.setProperty("stereoEnabled", true, nullptr);
  juce::ValueTree left("ChainBlocks");
  left.appendChild(makeNamBlockTree("amp-l", 1, 100, "a2-amp-cab-test.nam"), nullptr);
  state.appendChild(left, nullptr);
  juce::ValueTree right("RightChainBlocks");
  right.appendChild(makeNamBlockTree("amp-r", 2, 101, "a2-am-test-2.nam"), nullptr);
  right.appendChild(makeIrBlockTree("cab-r", 3, 102, "cab-ir-test-2.wav"), nullptr);
  state.appendChild(right, nullptr);
  proc.restoreFromTree(state);
  ASSERT_TRUE(waitForChainLoaded(proc)) << "chains never finished loading from cache";

  const juce::var first = runProbe(proc);
  ASSERT_EQ(first["state"].toString(), "done")
      << "first probe did not complete: " << juce::JSON::toString(first).toStdString();
  const double firstMs = static_cast<double>(first["matchedMs"]);
  EXPECT_LT(std::abs(firstMs), 24.0);

  // Let the ramp-back finish and the chain tails from the first probe decay
  // before measuring again.
  {
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    for (int block = 0; block < 30; ++block) {
      buffer.clear();
      proc.processBlock(buffer, midi);
    }
  }

  const juce::var second = runProbe(proc);
  ASSERT_EQ(second["state"].toString(), "done") << "second probe did not complete";
  EXPECT_NEAR(static_cast<double>(second["matchedMs"]), firstMs, 0.1 * 1000.0 / kFs);
}

}  // namespace
