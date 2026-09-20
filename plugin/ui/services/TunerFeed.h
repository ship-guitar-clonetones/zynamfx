// The tuner's pitch readout (TunerView.tsx's polling half). Constructing a
// feed switches the native pitch detector on (it costs audio-thread work, so
// it only runs while the tuner screen is up); destroying it switches it off.
// Polls at 20 Hz, smooths the cents lightly so the display doesn't jitter,
// and holds the last note on screen briefly after the signal decays.
#pragma once

#include <juce_events/juce_events.h>

#include <functional>

#include "backend/Backend.h"

namespace t3k::ui {

class TunerFeed : private juce::Timer {
public:
  static constexpr int kPollMs = 50;
  static constexpr int kHoldMs = 900;
  static constexpr float kMinConfidence = 0.5f;

  struct State {
    bool hasSignal = false;
    juce::String note;  // last detected, kept while the readout fades
    float cents = 0;    // smoothed
    float frequency = 0;
    bool operator==(const State& o) const {
      return hasSignal == o.hasSignal && note == o.note && juce::exactlyEqual(cents, o.cents) &&
             juce::exactlyEqual(frequency, o.frequency);
    }
  };

  explicit TunerFeed(Backend& backend);
  ~TunerFeed() override;

  const State& state() const { return state_; }
  std::function<void()> onChange;

private:
  void timerCallback() override;

  Backend& backend_;
  State state_;
  juce::int64 holdUntilMs_ = 0;
};

}  // namespace t3k::ui
