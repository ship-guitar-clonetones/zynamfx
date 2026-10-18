#include "SlotPan.h"

#include <cmath>

#include "core/Fonts.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kInset = 6;      // room for the L / R letters
constexpr int kThumbW = 8;
constexpr int kTrackH = 4;
}  // namespace

SlotPan::SlotPan(Services& services, const juce::String& paramId) : binding_(services.backend, paramId) {
  value_ = binding_.normalised();
  binding_.onChange = [this] {
    value_ = binding_.normalised();
    repaint();
  };
  setWantsKeyboardFocus(true);
  setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

SlotPan::~SlotPan() = default;

void SlotPan::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat();
  const float cy = r.getCentreY();
  const float x0 = r.getX() + kInset, x1 = r.getRight() - kInset;
  const float inner = juce::jmax(1.0f, x1 - x0);

  // End letters, then the track between them.
  g.setFont(Fonts::mono(8.0f));
  g.setColour(theme::kMuted);
  g.drawText("L", r.withWidth(static_cast<float>(kInset)).withTrimmedLeft(1.0f).toNearestInt(),
             juce::Justification::centredLeft, false);
  g.drawText("R", r.withTrimmedLeft(r.getWidth() - static_cast<float>(kInset)).toNearestInt(),
             juce::Justification::centredRight, false);

  const juce::Rectangle<float> track(x0, cy - kTrackH / 2.0f, inner, static_cast<float>(kTrackH));
  g.setColour(theme::kSegmentedTrack);
  g.fillRoundedRectangle(track, kTrackH / 2.0f);
  // Centre notch: the "no pan" reference.
  g.setColour(theme::kSubtle);
  g.fillRect(juce::Rectangle<float>(x0 + inner / 2.0f - 0.5f, cy - 3.0f, 1.0f, 6.0f));

  // Bipolar progress: the bar grows out of the centre notch toward the
  // thumb, so "no pan" reads as an empty bar on both lanes.
  const float centreX = x0 + inner / 2.0f;
  const float thumbX = x0 + value_ * inner;
  if (std::abs(thumbX - centreX) > 1.0f) {
    const juce::Rectangle<float> fill(std::min(centreX, thumbX), cy - kTrackH / 2.0f,
                                      std::abs(thumbX - centreX), static_cast<float>(kTrackH));
    g.setColour(theme::kWhite.withAlpha(0.85f));
    g.fillRoundedRectangle(fill, kTrackH / 2.0f);
  }

  g.setColour(theme::kWhite);
  g.fillRoundedRectangle(juce::Rectangle<float>(thumbX - kThumbW / 2.0f, cy - 6.0f,
                                                static_cast<float>(kThumbW), 12.0f),
                         3.0f);
}

void SlotPan::mouseDown(const juce::MouseEvent& e) {
  grabKeyboardFocus();
  binding_.beginGesture();
  writeFromX(e.x);
}

void SlotPan::mouseDrag(const juce::MouseEvent& e) { writeFromX(e.x); }

void SlotPan::mouseUp(const juce::MouseEvent&) { binding_.endGesture(); }

void SlotPan::writeFromX(int x) {
  const float x0 = static_cast<float>(kInset);
  const float inner = juce::jmax(1.0f, static_cast<float>(getWidth()) - 2.0f * x0);
  value_ = juce::jlimit(0.0f, 1.0f, (static_cast<float>(x) - x0) / inner);
  binding_.dragTo(value_);
  repaint();
}

void SlotPan::nudge(float delta) {
  value_ = juce::jlimit(0.0f, 1.0f, value_ + delta);
  binding_.set(value_);
  repaint();
}

bool SlotPan::keyPressed(const juce::KeyPress& key) {
  const float step = key.getModifiers().isShiftDown() ? 0.005f : 0.02f;
  if (key == juce::KeyPress::leftKey) { nudge(-step); return true; }
  if (key == juce::KeyPress::rightKey) { nudge(step); return true; }
  if (key == juce::KeyPress::homeKey) { nudge(-1.0f); return true; }
  if (key == juce::KeyPress::endKey) { nudge(1.0f); return true; }
  return false;
}

}  // namespace t3k::ui
