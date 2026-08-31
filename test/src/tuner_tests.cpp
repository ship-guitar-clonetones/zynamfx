// TunerDetector: YIN pitch detection over the ring buffer, as the tuner
// screen uses it (audio thread pushes, message thread polls).
#include "TunerDetector.h"

#include <gtest/gtest.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace {

constexpr double kFs = 48000.0;

juce::var readingFor(TunerDetector& tuner, const std::vector<float>& samples) {
  tuner.pushSamples(samples.data(), static_cast<int>(samples.size()));
  // getReading() caches analyses for 25 ms; step past that so every call
  // here analyzes the samples just pushed.
  juce::Thread::sleep(30);
  return tuner.getReading();
}

std::vector<float> sine(double freq, double seconds, float amplitude, double fs = kFs) {
  std::vector<float> out(static_cast<size_t>(fs * seconds));
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = amplitude *
             static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * freq *
                                         static_cast<double>(i) / fs));
  return out;
}

// Bass-like spectrum: fundamental quieter than the 2nd harmonic, the way a
// passive pickup renders low E. The detector must still report the true
// fundamental, not the octave above.
std::vector<float> weakFundamentalTone(double freq, double seconds) {
  constexpr double kAmps[] = {0.08, 0.22, 0.12, 0.06};  // f, 2f, 3f, 4f
  std::vector<float> out(static_cast<size_t>(kFs * seconds), 0.0f);
  for (size_t i = 0; i < out.size(); ++i) {
    double s = 0.0;
    for (size_t h = 0; h < 4; ++h)
      s += kAmps[h] * std::sin(2.0 * juce::MathConstants<double>::pi * freq *
                               static_cast<double>(h + 1) * static_cast<double>(i) / kFs);
    out[i] = static_cast<float>(s);
  }
  return out;
}

TEST(TunerDetectorTest, DetectsGuitarStringPitches) {
  TunerDetector tuner;
  tuner.prepare(kFs);
  tuner.setEnabled(true);

  // Low E, A, and high E fundamentals; within a couple cents each
  // (1 cent at 110 Hz is ~0.06 Hz, so a 0.5% tolerance is generous but
  // catches octave and decimation errors outright).
  for (const double freq : {82.41, 110.0, 329.63}) {
    const auto reading = readingFor(tuner, sine(freq, 1.0, 0.25f));
    const double detected = reading.getProperty("frequency", 0.0);
    EXPECT_NEAR(detected, freq, freq * 0.005) << "at " << freq << " Hz";
    EXPECT_GT(static_cast<double>(reading.getProperty("confidence", 0.0)), 0.5);
  }
}

TEST(TunerDetectorTest, DetectsBassStringPitches) {
  TunerDetector tuner;
  tuner.prepare(kFs);
  tuner.setEnabled(true);

  // 5-string low B, 4-string low E, A, and G fundamentals. All sit below or
  // around the old 55 Hz search floor that made bass unusable.
  for (const double freq : {30.87, 41.2, 55.0, 98.0}) {
    const auto reading = readingFor(tuner, sine(freq, 1.0, 0.25f));
    const double detected = reading.getProperty("frequency", 0.0);
    EXPECT_NEAR(detected, freq, freq * 0.005) << "at " << freq << " Hz";
    EXPECT_GT(static_cast<double>(reading.getProperty("confidence", 0.0)), 0.5);
  }
}

TEST(TunerDetectorTest, BassWithWeakFundamentalAvoidsOctaveError) {
  TunerDetector tuner;
  tuner.prepare(kFs);
  tuner.setEnabled(true);

  const double freq = 41.2;  // bass low E
  const auto reading = readingFor(tuner, weakFundamentalTone(freq, 1.0));
  const double detected = reading.getProperty("frequency", 0.0);
  EXPECT_NEAR(detected, freq, freq * 0.005);
  EXPECT_GT(static_cast<double>(reading.getProperty("confidence", 0.0)), 0.5);
}

TEST(TunerDetectorTest, DetectsBassLowEAtHighSampleRates) {
  // At 192 kHz a fixed ÷4 decimation would clamp the YIN lag range and lose
  // everything below ~47 Hz; the adaptive factor keeps low E reachable.
  constexpr double kHighFs = 192000.0;
  TunerDetector tuner;
  tuner.prepare(kHighFs);
  tuner.setEnabled(true);

  const double freq = 41.2;
  const auto reading = readingFor(tuner, sine(freq, 1.0, 0.25f, kHighFs));
  const double detected = reading.getProperty("frequency", 0.0);
  EXPECT_NEAR(detected, freq, freq * 0.005);
  EXPECT_GT(static_cast<double>(reading.getProperty("confidence", 0.0)), 0.5);
}

TEST(TunerDetectorTest, SilenceAndNoiseReportNoPitch) {
  TunerDetector tuner;
  tuner.prepare(kFs);
  tuner.setEnabled(true);

  const auto silent = readingFor(tuner, std::vector<float>(static_cast<size_t>(kFs), 0.0f));
  EXPECT_EQ(static_cast<double>(silent.getProperty("frequency", -1.0)), 0.0);

  juce::Random rng(7);
  std::vector<float> noise(static_cast<size_t>(kFs));
  for (auto& s : noise)
    s = rng.nextFloat() * 0.4f - 0.2f;
  const auto noisy = readingFor(tuner, noise);
  EXPECT_EQ(static_cast<double>(noisy.getProperty("frequency", -1.0)), 0.0);
}

}  // namespace
