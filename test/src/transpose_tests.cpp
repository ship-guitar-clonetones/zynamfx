// Transpose tests
//
// The mechanical guarantees of the input-stage pitch shifter (Transpose.h)
// and of the processor wiring around it:
//
//   TransposeTest   the latency figure the processor reports from the
//                   parameters matches the engine at every window and rate;
//                   at a 1.0 ratio the output is a pure delay at the floor
//                   and unity gain; the shift lands on the expected frequency
//                   including the fine trim; the tonality limit passes the
//                   highs unshifted; a pick attack re-syncs the tap to the
//                   floor; stereo channels share one tap; a mono buffer
//                   against the stereo engine is safe; rate / block-size /
//                   window changes and ±12 extremes stay finite; a window
//                   change keeps the shift; power, window and tonality
//                   changes blend (no step, no hole) and a powered-off
//                   engine stops running once its fade-out lands.
//   ProcessorTest   powered off the plugin is bit-exact and zero-latency;
//                   powering on reports boundary + window latency from the
//                   message thread; the parameters round-trip through state
//                   and presets; a state or preset saved before Transpose
//                   existed lands it on the defaults (off).
//
// Splice quality (sidebands, warble, onset timing on real DIs) is the
// bench's job, see plugin/docs/transpose.md.
#include "Processor.h"
#include "Transpose.h"
#include "test_helpers.h"

#include <gtest/gtest.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <vector>

namespace {

constexpr int kBlock = 512;

// Streams mono `in` through a shifter in kBlock blocks (the tail as a short
// one), optionally switching to `p2` at sample `switchAt`.
std::vector<float> runTranspose(const std::vector<float>& in, const Transpose::Params& p,
                                double fs = kFs, const Transpose::Params* p2 = nullptr,
                                int switchAt = -1) {
  Transpose t;
  t.prepare(fs, kBlock);
  t.setEnabled(true);
  t.setParams(p);
  std::vector<float> out;
  out.reserve(in.size());
  for (size_t off = 0; off < in.size(); off += kBlock) {
    if (p2 != nullptr && static_cast<int>(off) == switchAt) t.setParams(*p2);
    const int n = static_cast<int>(std::min<size_t>(kBlock, in.size() - off));
    juce::AudioBuffer<float> buf(1, n);
    buf.copyFrom(0, 0, in.data() + off, n);
    t.process(buf);
    out.insert(out.end(), buf.getReadPointer(0), buf.getReadPointer(0) + n);
  }
  return out;
}

void expectFinite(const juce::AudioBuffer<float>& buf) {
  for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    for (int i = 0; i < buf.getNumSamples(); ++i) {
      ASSERT_TRUE(std::isfinite(buf.getReadPointer(ch)[i])) << "ch " << ch << " sample " << i;
      ASSERT_LT(std::abs(buf.getReadPointer(ch)[i]), 10.0f);
    }
}

// Energy at `expected` must dominate energy left at `original` in the
// settled tail.
void expectShiftedTo(const std::vector<float>& out, double original, double expected) {
  constexpr int kSettle = 48000, kWindow = 65536;
  ASSERT_GE(out.size(), static_cast<size_t>(kSettle + kWindow));
  const double atExpected = db(goertzelPower(out.data() + kSettle, kWindow, expected));
  const double atOriginal = db(goertzelPower(out.data() + kSettle, kWindow, original));
  EXPECT_GT(atExpected, atOriginal + 20.0)
      << "expected the energy at " << expected << " Hz (" << atExpected << " dB), not at the original "
      << original << " Hz (" << atOriginal << " dB)";
}

}  // namespace

TEST(TransposeTest, LatencyFigureMatchesTheEngineAtAnyRate) {
  // The processor reports latency from the parameters via the static
  // figure, before the audio thread has switched windows; the engine the
  // audio thread runs must agree with it, at every window and host rate.
  for (const double fs : {44100.0, 48000.0, 96000.0}) {
    Transpose t;
    t.prepare(fs, kBlock);
    t.setEnabled(true);
    for (int w = 0; w < static_cast<int>(Transpose::kWindowMs.size()); ++w) {
      const auto window = Transpose::windowFromIndex(w);
      Transpose::Params p;
      p.window = window;
      t.setParams(p);
      EXPECT_EQ(t.latencySamples(), Transpose::latencySamples(window, fs)) << fs << " Hz, window " << w;
      // The tap's mean delay: halfway between the floor and the buffer.
      const int floor = static_cast<int>(fs * Transpose::kMinDelayMs / 1000);
      const int buffer = static_cast<int>(fs * Transpose::windowMs(window) / 1000);
      EXPECT_EQ(t.latencySamples(), (floor + buffer) / 2);
    }
  }
  EXPECT_EQ(Transpose::latencySamples(Transpose::Window::ms30, kFs), 768);  // (2 + 30) / 2 ms
  EXPECT_DOUBLE_EQ(Transpose::latencyMs(Transpose::Window::ms30), 16.0);
}

TEST(TransposeTest, UnityRatioIsAPureDelayAtTheFloor) {
  // Powered on at 0 st the tap does not drift, so the output is the input
  // delayed by the floor (where attacks are re-synced to) at unity gain.
  // Sweeping the knob through 0 therefore never jumps the timing.
  Transpose::Params p;
  const int floor = Transpose::minDelaySamples(kFs);
  const auto noise = makeNoise(2 * 48000, 3, 0.4f);
  const auto out = runTranspose(noise, p);
  EXPECT_EQ(bestCorrelationLag(out, noise, 48000, 8192, floor + 256), floor);
  for (int i = 48000; i < 48000 + 8192; ++i)
    ASSERT_NEAR(out[static_cast<size_t>(i)], noise[static_cast<size_t>(i - floor)], 1e-5f) << i;
  const auto tone = makeSine(3 * 48000, 440.0, 0.5f);
  const auto toneOut = runTranspose(tone, p);
  const double gain = db(goertzelPower(toneOut.data() + 96000, 16384, 440.0)) -
                      db(goertzelPower(tone.data() + 96000, 16384, 440.0));
  EXPECT_NEAR(gain, 0.0, 0.05);
}

TEST(TransposeTest, OctaveUpAndDownLandOnTheFrequency) {
  const auto in = makeSine(3 * 48000, 440.0, 0.5f);
  Transpose::Params up, down;
  up.semitones = 12;
  down.semitones = -12;
  expectShiftedTo(runTranspose(in, up), 440.0, 880.0);
  expectShiftedTo(runTranspose(in, down), 440.0, 220.0);
}

TEST(TransposeTest, FineTrimJoinsTheRatio) {
  // -2 st and +50 cents is a ratio of 2^(-1.5/12).
  const auto in = makeSine(3 * 48000, 440.0, 0.5f);
  Transpose::Params p;
  p.semitones = -2;
  p.cents = 50.0f;
  expectShiftedTo(runTranspose(in, p), 440.0, 440.0 * std::pow(2.0, -1.5 / 12.0));
}

TEST(TransposeTest, ShiftedToneKeepsUnityGainAtEveryWindow) {
  // Splices land on whole periods of a steady tone, so the crossfades add
  // nothing and take nothing away, at any buffer size.
  const auto in = makeSine(3 * 48000, 220.0, 0.5f);
  for (int w = 0; w < static_cast<int>(Transpose::kWindowMs.size()); ++w) {
    Transpose::Params p;
    p.semitones = -2;
    p.window = Transpose::windowFromIndex(w);
    const auto out = runTranspose(in, p);
    const double expected = 220.0 * std::pow(2.0, -2.0 / 12.0);
    const double gain = db(goertzelPower(out.data() + 48000, 65536, expected)) -
                        db(goertzelPower(in.data() + 48000, 65536, 220.0));
    EXPECT_NEAR(gain, 0.0, 1.0) << "window " << w;
  }
}

TEST(TransposeTest, TonalityLimitPassesTheHighsUnshifted) {
  // Above the limit the input bypasses the shifter: an octave up moves a
  // 6 kHz partial to 12 kHz with the limit off, and leaves it at 6 kHz with
  // a 2 kHz limit.
  const auto in = makeSine(3 * 48000, 6000.0, 0.5f);
  Transpose::Params p;
  p.semitones = 12;
  expectShiftedTo(runTranspose(in, p), 6000.0, 12000.0);
  p.tonalityHz = 2000.0f;
  expectShiftedTo(runTranspose(in, p), 12000.0, 6000.0);
  // ... while a partial below the limit still shifts.
  const auto low = makeSine(3 * 48000, 440.0, 0.5f);
  expectShiftedTo(runTranspose(low, p), 440.0, 880.0);
}

TEST(TransposeTest, PickAttackReSyncsTheTapToTheFloor) {
  // On a sustained note the tap drifts across the buffer; a pick attack
  // must not wait for it. The burst has to appear in the output within the
  // floor plus the re-sync span, wherever the tap was.
  const int floor = Transpose::minDelaySamples(kFs);
  const int span = static_cast<int>(kFs * 0.004);
  const int fade = static_cast<int>(kFs * 0.002);
  // 1 ms RMS of `x` ending at `end`.
  const auto rms = [](const std::vector<float>& x, int end) {
    double acc = 0.0;
    for (int i = end - 48; i < end; ++i) acc += static_cast<double>(x[static_cast<size_t>(i)]) * x[static_cast<size_t>(i)];
    return std::sqrt(acc / 48.0);
  };
  for (const int holdMs : {700, 1150, 1600, 2050}) {
    const int hold = holdMs * 48;
    // A quiet low note (little energy above the detector's 600 Hz), then a
    // loud broadband burst: a 14 dB step.
    auto in = makeSine(hold + 24000, 110.0, 0.1f);
    const auto burst = makeNoise(24000, 11, 0.9f);
    for (int i = 0; i < 24000; ++i) in[static_cast<size_t>(hold + i)] = burst[static_cast<size_t>(i)];
    Transpose::Params p;
    p.semitones = -2;
    const auto out = runTranspose(in, p);
    // Arrival: the first 1 ms window past the step's midpoint level (the
    // tone's RMS is 0.07, the burst's 0.52).
    int arrival = -1;
    for (int end = hold + 48; end < hold + 24000 && arrival < 0; ++end)
      if (rms(out, end) > 0.3) arrival = end;
    ASSERT_GT(arrival, 0) << holdMs;
    const int lag = arrival - hold;
    EXPECT_GE(lag, floor) << holdMs;
    // Floor + re-sync span + the fade + the 1 ms window + 1 ms of detector.
    EXPECT_LE(lag, floor + span + fade + 96) << holdMs;
  }
}

TEST(TransposeTest, StereoChannelsShareOneTap) {
  // The lag search and the detector run on the channel mean and both
  // channels read the same tap, so a right channel that is half the left
  // stays exactly half through every splice: the image never smears.
  Transpose t;
  t.prepare(kFs, kBlock);
  t.setEnabled(true);
  Transpose::Params p;
  p.semitones = -3;
  t.setParams(p);
  juce::AudioBuffer<float> buf(2, kBlock);
  constexpr int kLength = 192 * kBlock;
  const auto noise = makeNoise(kLength, 5, 0.4f);
  for (int off = 0; off < kLength; off += kBlock) {
    buf.copyFrom(0, 0, noise.data() + off, kBlock);
    buf.copyFrom(1, 0, noise.data() + off, kBlock);
    buf.applyGain(1, 0, kBlock, 0.5f);
    t.process(buf);
    for (int i = 0; i < kBlock; ++i)
      ASSERT_NEAR(buf.getReadPointer(1)[i], 0.5f * buf.getReadPointer(0)[i], 1e-6f) << off + i;
  }
}

TEST(TransposeTest, MonoBufferAgainstTheStereoEngineIsSafe) {
  // A genuinely mono host buffer (see
  // ProcessorTest.StereoChainsFoldToMonoWithoutAStereoOutput) feeds the
  // engine from its one channel; nothing may read or write past it.
  Transpose t;
  t.prepare(kFs, kBlock);
  t.setEnabled(true);
  Transpose::Params p;
  p.semitones = 7;
  t.setParams(p);
  juce::AudioBuffer<float> mono(1, kBlock);
  const auto noise = makeNoise(kBlock, 99, 0.3f);
  for (int i = 0; i < 50; ++i) {
    mono.copyFrom(0, 0, noise.data(), kBlock);
    t.process(mono);
  }
  expectFinite(mono);
}

TEST(TransposeTest, SurvivesRateBlockAndWindowChanges) {
  Transpose t;
  Transpose::Params p;
  p.semitones = 5;
  auto run = [&](int block, unsigned seed) {
    juce::AudioBuffer<float> buf(2, block);
    const auto noise = makeNoise(block, seed, 0.3f);
    buf.copyFrom(0, 0, noise.data(), block);
    buf.copyFrom(1, 0, noise.data(), block);
    t.process(buf);
    expectFinite(buf);
  };
  t.prepare(44100.0, 256);
  t.setEnabled(true);
  t.setParams(p);
  run(256, 1);
  // A device change is a fresh prepare(), like a real prepareToPlay.
  t.prepare(96000.0, 1024);
  run(1024, 2);
  // A block bigger than the prepared size (an offline bounce).
  run(4096, 3);
  // Every window, switched live, at both shift directions.
  for (int w = 0; w < static_cast<int>(Transpose::kWindowMs.size()); ++w) {
    p.window = Transpose::windowFromIndex(w);
    for (const int semis : {12, -12}) {
      p.semitones = semis;
      t.setParams(p);
      for (int i = 0; i < 8; ++i) run(1024, 10 + static_cast<unsigned>(w) + static_cast<unsigned>(i));
    }
  }
}

TEST(TransposeTest, ExtremeShiftsStayBounded) {
  for (const int semis : {-12, 12}) {
    Transpose::Params p;
    p.semitones = semis;
    const auto out = runTranspose(makeNoise(static_cast<int>(kFs), 555u + static_cast<unsigned>(semis), 0.5f), p);
    for (const float s : out) {
      ASSERT_TRUE(std::isfinite(s)) << semis;
      ASSERT_LT(std::abs(s), 10.0f) << semis;
    }
  }
}

TEST(TransposeTest, WindowChangeKeepsTheShift) {
  // Switching buffers mid-stream: the ratio must carry over and the tap
  // must find its way into the new range.
  const auto in = makeSine(4 * 48000, 440.0, 0.5f);
  Transpose::Params a, b;
  a.semitones = b.semitones = 12;
  a.window = Transpose::Window::ms20;
  b.window = Transpose::Window::ms60;
  const auto out = runTranspose(in, a, kFs, &b, 94 * kBlock);  // on a block edge, or never applied
  expectShiftedTo(out, 440.0, 880.0);
}

namespace {

// Largest sample-to-sample step in [from, to).
float maxStep(const std::vector<float>& x, int from, int to) {
  float m = 0.0f;
  for (int i = from + 1; i < to; ++i)
    m = std::max(m, std::abs(x[static_cast<size_t>(i)] - x[static_cast<size_t>(i - 1)]));
  return m;
}

}  // namespace

TEST(TransposeTest, WindowChangeIsSeamless) {
  // Shrinking the buffer from 60 to 20 ms while the tap sits deep in it:
  // the rings keep their audio, so there is no hole, and the tap splices
  // back into range through a crossfade, so there is no step. A 220 Hz
  // tone's own largest step is ~0.014 per sample at 0.5 amplitude.
  const auto in = makeSine(3 * 48000, 220.0, 0.5f);
  Transpose::Params a, b;
  a.semitones = b.semitones = -2;
  a.window = Transpose::Window::ms60;
  b.window = Transpose::Window::ms20;
  const int switchAt = 96 * kBlock;
  const auto out = runTranspose(in, a, kFs, &b, switchAt);
  EXPECT_LT(maxStep(out, switchAt - 4800, switchAt + 4800), 0.03f);
  for (int end = switchAt + 240; end <= switchAt + 4800; end += 240) {
    double acc = 0.0;
    for (int i = end - 240; i < end; ++i) acc += static_cast<double>(out[static_cast<size_t>(i)]) * out[static_cast<size_t>(i)];
    EXPECT_GT(std::sqrt(acc / 240), 0.25) << "hole at " << end;  // the tone's RMS is 0.35
  }
}

TEST(TransposeTest, PowerBlendsInsteadOfStepping) {
  // Power on and off mid-tone: the dry and the shifted signal crossfade
  // over 25 ms; neither edge may step, and after the fade-out lands the
  // engine reports itself stopped so the processor can bypass it. An
  // off/on tap inside the fade-out (one block apart) must turn the blend
  // around, not restart the engine at a nonzero mix.
  Transpose t;
  t.prepare(kFs, kBlock);
  Transpose::Params p;
  p.semitones = -5;
  constexpr int kLength = 288 * kBlock;  // ~3 s
  const auto in = makeSine(kLength, 220.0, 0.5f);
  juce::AudioBuffer<float> buf(1, kBlock);
  std::vector<float> out;
  int stoppedAt = -1;
  for (int off = 0; off < kLength; off += kBlock) {
    if (off == 48 * kBlock) t.setEnabled(true);
    if (off == 120 * kBlock) t.setEnabled(false);
    if (off == 121 * kBlock) t.setEnabled(true);
    if (off == 192 * kBlock) t.setEnabled(false);
    buf.copyFrom(0, 0, in.data() + off, kBlock);
    if (t.isRunning()) {
      t.setParams(p);
      t.process(buf);
    } else if (off > 192 * kBlock && stoppedAt < 0) {
      stoppedAt = off;
    }
    out.insert(out.end(), buf.getReadPointer(0), buf.getReadPointer(0) + kBlock);
  }
  EXPECT_LT(maxStep(out, 0, kLength), 0.03f);
  ASSERT_GT(stoppedAt, 0);
  EXPECT_LT(stoppedAt - 192 * kBlock, 48000 * 0.05) << "the fade-out should land within ~25 ms";
  // Off before the power-on and after the fade-out: the input untouched.
  for (int i = 0; i < 48 * kBlock; ++i) ASSERT_EQ(out[static_cast<size_t>(i)], in[static_cast<size_t>(i)]);
  for (int i = stoppedAt; i < kLength; ++i) ASSERT_EQ(out[static_cast<size_t>(i)], in[static_cast<size_t>(i)]);
  // And it did shift in between.
  const double shifted = 220.0 * std::pow(2.0, -5.0 / 12.0);
  EXPECT_GT(db(goertzelPower(out.data() + 60 * kBlock, 32768, shifted)),
            db(goertzelPower(out.data() + 60 * kBlock, 32768, 220.0)) + 20.0);
}

TEST(TransposeTest, TonalityBlendsInsteadOfStepping) {
  const auto in = makeSine(3 * 48000, 220.0, 0.5f);
  Transpose::Params a, b;
  a.semitones = b.semitones = -2;
  b.tonalityHz = 3000.0f;
  const int switchAt = 96 * kBlock;
  const auto out = runTranspose(in, a, kFs, &b, switchAt);
  EXPECT_LT(maxStep(out, switchAt - 4800, switchAt + 4800), 0.03f);
}

TEST(TransposeTest, UpshiftOnTheSmallestBufferStaysOnPitch) {
  // +12 on 20 ms is the tightest case: the tap covers the buffer in ~12 ms
  // and the guard, fade and search lead eat most of it. Splices must still
  // find whole-period jumps.
  const auto in = makeSine(3 * 48000, 440.0, 0.5f);
  Transpose::Params p;
  p.semitones = 12;
  p.window = Transpose::Window::ms20;
  const auto out = runTranspose(in, p);
  // Peak of a 1.36 s spectrum within 3 cents of 880.
  double best = 0.0, bestF = 0.0;
  for (double f = 870.0; f <= 890.0; f += 0.25) {
    const double pw = goertzelPower(out.data() + 48000, 65536, f);
    if (pw > best) best = pw, bestF = f;
  }
  EXPECT_NEAR(1200.0 * std::log2(bestF / 880.0), 0.0, 3.0) << bestF << " Hz";
}

// Processor-level contracts.

namespace {

float denormalised(TONE3000Processor& proc, const char* id) {
  auto* p = proc.parameters.getParameter(id);
  return p->convertFrom0to1(p->getValue());
}

void setDenormalised(TONE3000Processor& proc, const char* id, float value) {
  auto* p = proc.parameters.getParameter(id);
  p->setValueNotifyingHost(p->convertTo0to1(value));
}

// Latency changes are reported from the message thread (see
// TONE3000Processor::updateLatency); pump it.
void pumpMessages() { juce::MessageManager::getInstance()->runDispatchLoopUntil(50); }

std::vector<float> processThrough(TONE3000Processor& proc, const std::vector<float>& in) {
  std::vector<float> out(in.size(), 0.0f);
  juce::AudioBuffer<float> buffer(2, kBlock);
  juce::MidiBuffer midi;
  for (size_t off = 0; off < in.size(); off += kBlock) {
    buffer.copyFrom(0, 0, in.data() + off, kBlock);
    buffer.copyFrom(1, 0, in.data() + off, kBlock);
    proc.processBlock(buffer, midi);
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlock, out.begin() + static_cast<long>(off));
  }
  return out;
}

}  // namespace

TEST(ProcessorTest, TransposeDefaultsAreOffAndMatchTheDsp) {
  TONE3000Processor proc;
  const Transpose::Params p;
  EXPECT_FLOAT_EQ(proc.parameters.getRawParameterValue("transposeEnabled")->load(), 0.0f);
  EXPECT_FLOAT_EQ(denormalised(proc, "transposeSemitones"), static_cast<float>(p.semitones));
  EXPECT_NEAR(denormalised(proc, "transposeFine"), p.cents, 1e-4f);
  EXPECT_FLOAT_EQ(denormalised(proc, "transposeTonality"), Transpose::kTonalityOffHz);  // off
  EXPECT_FLOAT_EQ(denormalised(proc, "transposeWindow"), static_cast<float>(p.window));
  // The knob's ends and centre map to whole semitones.
  auto* semis = proc.parameters.getParameter("transposeSemitones");
  EXPECT_FLOAT_EQ(semis->convertFrom0to1(0.0f), -12.0f);
  EXPECT_FLOAT_EQ(semis->convertFrom0to1(0.5f), 0.0f);
  EXPECT_FLOAT_EQ(semis->convertFrom0to1(1.0f), 12.0f);
  // The tonality log map round-trips its ends.
  auto* tonality = proc.parameters.getParameter("transposeTonality");
  EXPECT_NEAR(tonality->convertFrom0to1(0.0f), Transpose::kTonalityMinHz, 0.5f);
  EXPECT_NEAR(tonality->convertFrom0to1(1.0f), Transpose::kTonalityOffHz, 0.5f);
}

TEST(ProcessorTest, TransposeOffIsBitExactAndZeroLatencyEvenWithAShiftDialled) {
  // Off is the default, and a dialled-in shift with the power off must not
  // leak: the group's knob is a setting, the power is the effect.
  TONE3000Processor proc;
  proc.setPlayConfigDetails(2, 2, kFs, kBlock);
  proc.prepareToPlay(kFs, kBlock);
  setDenormalised(proc, "transposeSemitones", -4.0f);
  pumpMessages();
  EXPECT_EQ(proc.getLatencySamples(), 0);
  const auto in = makeNoise(64 * kBlock, 7, 0.4f);
  const auto out = processThrough(proc, in);
  // The DC blocker is the only thing in the path (see
  // EmptyChainAt48kIsTransparentWithZeroLatency), so the tail correlates at
  // lag 0 with no smearing.
  EXPECT_EQ(bestCorrelationLag(out, in, 16384, 4096, 64), 0);
}

TEST(ProcessorTest, TransposePowerReportsWindowLatencyFromTheMessageThread) {
  TONE3000Processor proc;
  proc.setPlayConfigDetails(2, 2, kFs, kBlock);
  proc.prepareToPlay(kFs, kBlock);
  ASSERT_EQ(proc.getLatencySamples(), 0);

  proc.parameters.getParameter("transposeEnabled")->setValueNotifyingHost(1.0f);
  pumpMessages();
  EXPECT_EQ(proc.getLatencySamples(), Transpose::latencySamples(Transpose::kDefaultWindow, kFs));

  setDenormalised(proc, "transposeWindow", static_cast<float>(Transpose::Window::ms60));
  pumpMessages();
  EXPECT_EQ(proc.getLatencySamples(), Transpose::latencySamples(Transpose::Window::ms60, kFs));

  // The knob itself never moves the latency: the engine runs at 0 st too.
  setDenormalised(proc, "transposeSemitones", 0.0f);
  setDenormalised(proc, "transposeSemitones", -12.0f);
  pumpMessages();
  EXPECT_EQ(proc.getLatencySamples(), Transpose::latencySamples(Transpose::Window::ms60, kFs));

  proc.parameters.getParameter("transposeEnabled")->setValueNotifyingHost(0.0f);
  pumpMessages();
  EXPECT_EQ(proc.getLatencySamples(), 0);
}

TEST(ProcessorTest, TransposeShiftsThePluginOutput) {
  // End to end at the default window: a powered -12 lands the octave below
  // in the output, delayed by the reported latency.
  TONE3000Processor proc;
  proc.setPlayConfigDetails(2, 2, kFs, kBlock);
  proc.prepareToPlay(kFs, kBlock);
  proc.parameters.getParameter("transposeEnabled")->setValueNotifyingHost(1.0f);
  setDenormalised(proc, "transposeSemitones", -12.0f);
  pumpMessages();
  const auto in = makeSine(4 * 48000, 440.0, 0.5f);
  const auto out = processThrough(proc, in);
  expectShiftedTo(out, 440.0, 220.0);
}

TEST(ProcessorTest, TransposeSurvivesStateRoundTrip) {
  juce::MemoryBlock state;
  {
    TONE3000Processor a;
    a.parameters.getParameter("transposeEnabled")->setValueNotifyingHost(1.0f);
    setDenormalised(a, "transposeSemitones", -3.0f);
    setDenormalised(a, "transposeFine", 25.0f);
    setDenormalised(a, "transposeTonality", 4000.0f);
    setDenormalised(a, "transposeWindow", 2.0f);
    a.getStateInformation(state);
  }
  TONE3000Processor b;
  b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  EXPECT_FLOAT_EQ(b.parameters.getRawParameterValue("transposeEnabled")->load(), 1.0f);
  EXPECT_FLOAT_EQ(denormalised(b, "transposeSemitones"), -3.0f);
  EXPECT_NEAR(denormalised(b, "transposeFine"), 25.0f, 0.01f);
  EXPECT_NEAR(denormalised(b, "transposeTonality"), 4000.0f, 1.0f);
  EXPECT_FLOAT_EQ(denormalised(b, "transposeWindow"), 2.0f);
}

TEST(ProcessorTest, StateFromBeforeTransposeLandsOnItsDefaults) {
  // A session saved before Transpose existed carries none of its entries.
  // Restoring it must land the group off at its defaults, never leave a
  // live shift running (a silent latency and pitch change on project load).
  juce::MemoryBlock saved;
  {
    TONE3000Processor old;
    old.parameters.getParameter("gateThreshold")->setValueNotifyingHost(0.6f);
    old.getStateInformation(saved);
  }
  juce::ValueTree tree = juce::ValueTree::readFromData(
      static_cast<const char*>(saved.getData()) + 4, saved.getSize() - 4);
  ASSERT_TRUE(tree.isValid());
  juce::ValueTree params = tree.getChildWithName("PARAMETERS");
  ASSERT_TRUE(params.isValid());
  for (const auto* id : {"transposeEnabled", "transposeSemitones", "transposeFine", "transposeTonality",
                         "transposeWindow"}) {
    const auto child = params.getChildWithProperty("id", id);
    ASSERT_TRUE(child.isValid()) << id;
    params.removeChild(child, nullptr);
  }
  juce::MemoryBlock reframed;
  {
    juce::MemoryOutputStream out(reframed, false);
    out.write("T3KB", 4);
    tree.writeToStream(out);
  }

  TONE3000Processor proc;
  proc.parameters.getParameter("transposeEnabled")->setValueNotifyingHost(1.0f);
  setDenormalised(proc, "transposeSemitones", -5.0f);
  proc.setStateInformation(reframed.getData(), static_cast<int>(reframed.getSize()));
  EXPECT_NEAR(proc.parameters.getRawParameterValue("gateThreshold")->load(), -40.0f, 0.01f)
      << "the old state's own parameters must still restore";
  EXPECT_FLOAT_EQ(proc.parameters.getRawParameterValue("transposeEnabled")->load(), 0.0f);
  EXPECT_FLOAT_EQ(denormalised(proc, "transposeSemitones"), 0.0f);
}

TEST(ProcessorTest, PresetsCarryTranspose) {
  const juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("t3k-transpose-tests-" + juce::Uuid().toString());
  tmp.createDirectory();
  {
    TONE3000Processor proc;
    proc.setPresetStoreForTesting(tmp);
    // A stock preset loads with the group off (a preset from a pre-Transpose
    // build has no entries at all and takes the same default path, see
    // loadPreset's missing-id fallback).
    const juce::var stock = proc.savePreset("Stock");
    ASSERT_TRUE(stock.isObject());

    proc.parameters.getParameter("transposeEnabled")->setValueNotifyingHost(1.0f);
    setDenormalised(proc, "transposeSemitones", -2.0f);
    setDenormalised(proc, "transposeFine", -10.0f);
    const juce::var dropD = proc.savePreset("Drop D");
    ASSERT_TRUE(dropD.isObject());

    ASSERT_TRUE(proc.loadPreset(stock["id"].toString()));
    EXPECT_FLOAT_EQ(proc.parameters.getRawParameterValue("transposeEnabled")->load(), 0.0f);
    EXPECT_FLOAT_EQ(denormalised(proc, "transposeSemitones"), 0.0f);

    ASSERT_TRUE(proc.loadPreset(dropD["id"].toString()));
    EXPECT_FLOAT_EQ(proc.parameters.getRawParameterValue("transposeEnabled")->load(), 1.0f);
    EXPECT_FLOAT_EQ(denormalised(proc, "transposeSemitones"), -2.0f);
    EXPECT_NEAR(denormalised(proc, "transposeFine"), -10.0f, 0.01f);
  }
  tmp.deleteRecursively();
}
