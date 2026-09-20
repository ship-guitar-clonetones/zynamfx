// Live spectrum of the audio leaving one block (useBlockSpectrum.ts).
// Constructing a feed enables the native analyser for that block (the audio
// thread does zero analyser work otherwise); destroying it disables it
// again. Polls at ~30 fps and notifies only when the bins moved, so an idle
// signal costs no repaints.
#pragma once

#include <juce_events/juce_events.h>

#include <functional>
#include <string>
#include <vector>

#include "backend/Backend.h"

namespace t3k::ui {

class SpectrumFeed : private juce::Timer {
public:
  // Mirrored from plugin/include/BlockSpectrum.h: kNumBins dB values
  // (kMinDb..0) log-spaced 20 Hz .. 20 kHz, the EQ graph's own axis.
  static constexpr int kNumBins = 64;
  static constexpr float kMinDb = -100;
  static constexpr int kPollMs = 33;

  SpectrumFeed(Backend& backend, std::string blockId);
  ~SpectrumFeed() override;

  const std::vector<float>& bins() const { return bins_; }
  std::function<void()> onChange;

private:
  void timerCallback() override;

  Backend& backend_;
  std::string blockId_;
  std::vector<float> bins_, scratch_;  // scratch_: reused poll buffer
};

}  // namespace t3k::ui
