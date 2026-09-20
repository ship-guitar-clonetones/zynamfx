// One CSS `transition: <property> Nms ease` for a float: tween from the
// current value to a target over a duration, applying each frame through a
// callback. AlphaTween (opacity) and the banner slot (height) are built on
// it. Owned by the component it drives, so it can never outlive the target.
#pragma once

#include <juce_events/juce_events.h>

#include <cmath>
#include <functional>

namespace t3k::ui {

class Tween : private juce::Timer {
public:
  explicit Tween(std::function<void(float)> apply, float initial = 0) : apply_(std::move(apply)), value_(initial) {}
  ~Tween() override { stopTimer(); }

  float value() const { return value_; }
  bool running() const { return isTimerRunning(); }

  // Ease to `target`; `done` fires once it lands (not when interrupted).
  void animateTo(float target, int durationMs, std::function<void()> done = {}) {
    if (durationMs <= 0) {
      snap(target);
      if (done) done();
      return;
    }
    from_ = value_;
    to_ = target;
    durationMs_ = durationMs;
    startMs_ = juce::Time::currentTimeMillis();
    done_ = std::move(done);
    startTimer(kFrameMs);
  }

  void snap(float target) {
    stopTimer();
    done_ = nullptr;
    value_ = to_ = target;
    apply_(value_);
  }

  // CSS `ease` (cubic-bezier(0.25, 0.1, 0.25, 1), close enough as ease-in-out).
  static float ease(float t) { return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2.0f) / 2; }

private:
  static constexpr int kFrameMs = 16;

  void timerCallback() override {
    const float t = juce::jlimit(0.0f, 1.0f,
                                 static_cast<float>(juce::Time::currentTimeMillis() - startMs_) / durationMs_);
    value_ = from_ + (to_ - from_) * ease(t);
    apply_(value_);
    if (t >= 1.0f) {
      stopTimer();
      auto done = std::move(done_);
      done_ = nullptr;
      if (done) done();
    }
  }

  std::function<void(float)> apply_;
  float value_, from_ = 0, to_ = 0;
  int durationMs_ = 0;
  juce::int64 startMs_ = 0;
  std::function<void()> done_;
};

}  // namespace t3k::ui
