#include "Knob.h"

#include <cmath>

#include "KnobFace.h"
#include "SecondaryPress.h"
#include "core/Fonts.h"
#include "core/Paint.h"

namespace t3k::ui {

namespace {

constexpr float kBaseSensitivity = 0.006f;  // value per design px
constexpr float kFineFactor = 8.0f;
// Label -> readout swap is debounced on press so a quick tap (half of a
// double-tap heading into the editor) never flashes the value; it also
// lingers after release instead of snapping back.
constexpr int kReadoutShowMs = 150;
constexpr int kReadoutHoldMs = 250;
// Touch double tap: the usual recogniser window, and a slop wide enough for
// two taps by the same finger without being a drag.
constexpr int kDoubleTapMs = 300;
constexpr float kDoubleTapSlop = 24.0f;
// Touch-and-hold: the system's own long-press delay; travel beyond the slop
// turns the hold into a drag.
constexpr int kHoldMs = 500;
constexpr float kHoldSlop = 8.0f;
// Bipolar centre detent window (coarse drag only).
constexpr float kDetent = 0.02f;

const juce::Colour kEditorBorder = juce::Colour(235, 235, 245).withAlpha(0.3f);

// Indicator angle in degrees from noon (-135..135) for a variant.
float angleFor(Knob::Variant variant, float v) {
  switch (variant) {
    case Knob::Variant::panLeft: return juce::jlimit(0.0f, 0.5f, v) / 0.5f * 135.0f - 135.0f;
    case Knob::Variant::panRight: return (juce::jlimit(0.5f, 1.0f, v) - 0.5f) / 0.5f * 135.0f;
    case Knob::Variant::full:
    case Knob::Variant::bipolar: break;
  }
  return juce::jlimit(0.0f, 1.0f, v) * 270.0f - 135.0f;
}

// Every centred variant reads zero at noon; a plain knob at the start of travel.
float arcFromFor(Knob::Variant variant) { return variant == Knob::Variant::full ? -135.0f : 0.0f; }

}  // namespace

Knob::Knob(Options options) : options_(std::move(options)) {
  setSize(options_.size, heightFor(options_.size));
  // Labels wider than the knob ("Crossover" under a 36px knob) overflow the
  // column, as the web's `overflow: visible` slot did.
  setPaintingIsUnclipped(true);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  setViewportIgnoreDragFlag(true);  // a touch drag turns the knob, not the page
  if (options_.help) setHelpText(help::text(*options_.help));
  live_ = emitted_ = value_ = juce::jlimit(options_.min, options_.max, options_.min);
}

Knob::~Knob() = default;

// The face is centred in whatever width the component was given; the label
// takes the full width, so a parent can widen a knob to let a long label
// overflow the face symmetrically (the web's nowrap label under a 60px knob).
juce::Rectangle<int> Knob::faceBounds() const {
  const int y = options_.labelOnTop ? kEditorOverflow + kLabelSlot + theme::kKnobLabelGap : 0;
  return {(getWidth() - options_.size) / 2, y, options_.size, options_.size};
}

juce::Rectangle<int> Knob::labelBounds() const {
  const int y = options_.labelOnTop ? kEditorOverflow : options_.size + theme::kKnobLabelGap;
  return {0, y, getWidth(), kLabelSlot};
}

void Knob::setValue(float normalised) {
  if (dragging_) return;
  const float v = juce::jlimit(options_.min, options_.max, normalised);
  live_ = emitted_ = v;
  if (juce::exactlyEqual(v, value_)) return;
  value_ = v;
  repaint();
}

void Knob::emit(float v) {
  emitted_ = v;
  value_ = v;
  repaint();
  if (onChange) onChange(v);
}

bool Knob::resetToDefault() {
  if (!options_.defaultValue) return false;
  const float fallback = *options_.defaultValue;
  live_ = fallback;
  emit(fallback);
  if (onReset) onReset();
  return true;
}

void Knob::applyLive(float next, bool fine) {
  // Accumulate raw: the detent is applied to the emitted value only, so drag
  // progress keeps counting while the readout rests on centre and the knob
  // glides out the far side of the window.
  const float raw = juce::jlimit(options_.min, options_.max, next);
  live_ = raw;
  const bool snap = options_.variant == Variant::bipolar && !fine && std::abs(raw - 0.5f) < kDetent;
  const float v = snap ? 0.5f : raw;
  if (juce::exactlyEqual(v, emitted_)) return;
  emit(v);
}

// Mouse
void Knob::mouseDown(const juce::MouseEvent& e) {
  const bool touch = e.source.isTouch();
  if (!touch && !e.mods.isLeftButtonDown()) {  // right-click belongs to the group
    forwardSecondaryPress(*this, e);
    return;
  }
  touchPress_ = touch;
  pressOrigin_ = e.position;
  pressTravelled_ = false;
  if (!faceBounds().contains(e.getPosition())) return;  // the label is a tap target only

  if (touch) {
    // Second tap of a double tap resets, and ends the gesture there: engaging
    // the drag as well would let the finger travel between the taps move the
    // value straight back off the default.
    const auto now = juce::Time::currentTimeMillis();
    const bool isDoubleTap = lastTapMs_ != 0 && now - lastTapMs_ < kDoubleTapMs &&
                             e.position.getDistanceFrom(lastTapPos_) < kDoubleTapSlop;
    lastTapMs_ = isDoubleTap ? 0 : now;  // a third tap starts a fresh pair
    lastTapPos_ = e.position;
    if (isDoubleTap && resetToDefault()) return;
    if (onLongPress)
      holdTimer_.start(kHoldMs, [this] {
        pressTravelled_ = true;  // a hold that fired is not a tap
        endDrag();
        onLongPress();
      });
  }

  // Alt/Option-click: reset to default. The drag still engages beneath,
  // which is harmless: releasing without moving stays at the default.
  if (!(e.mods.isAltDown() && resetToDefault())) live_ = emitted_ = value_;

  dragging_ = true;
  lastY_ = e.position.y;
  readoutTimer_.start(kReadoutShowMs, [this] { setReadoutVisible(true); });
  if (onDragStateChange) onDragStateChange(true);
}

void Knob::mouseDrag(const juce::MouseEvent& e) {
  if (!pressTravelled_ && e.position.getDistanceFrom(pressOrigin_) > kHoldSlop) {
    // A press that travels is a drag, not the first half of a double tap
    // (nor a hold, nor a label tap).
    pressTravelled_ = true;
    lastTapMs_ = 0;
    holdTimer_.cancel();
  }
  if (!dragging_) return;
  // Shift toggles fine mode live, including mid-drag. Coordinates are
  // already in design px (the root transform undoes the window scale), so
  // the same drag distance relative to the knob always covers the same range.
  const bool fine = e.mods.isShiftDown();
  const float sensitivity = fine ? kBaseSensitivity / kFineFactor : kBaseSensitivity;
  applyLive(live_ + (lastY_ - e.position.y) * sensitivity, fine);
  lastY_ = e.position.y;
}

void Knob::endDrag() {
  if (!dragging_) return;
  dragging_ = false;
  readoutTimer_.start(kReadoutHoldMs, [this] { setReadoutVisible(false); });
  if (onDragStateChange) onDragStateChange(false);
}

void Knob::mouseUp(const juce::MouseEvent& e) {
  holdTimer_.cancel();
  endDrag();
  // Touch route into the type-in editor: tap the label (double tap is taken
  // by the reset). Desktop keeps the double-click on the face and ignores
  // presses here.
  if (touchPress_ && !pressTravelled_ && editor_ == nullptr &&
      labelBounds().contains(pressOrigin_.toInt()) && labelBounds().contains(e.getPosition()))
    openEditor();
}

void Knob::mouseDoubleClick(const juce::MouseEvent& e) {
  // A touch double tap resets (see mouseDown); it must not also open the editor.
  if (e.source.isTouch() || !faceBounds().contains(e.getPosition())) return;
  endDrag();
  openEditor();
}

// Readout / editor
void Knob::setReadoutVisible(bool show) {
  if (readoutVisible_ == show) return;
  readoutVisible_ = show;
  repaint();
}

void Knob::openEditor() {
  if (editor_ != nullptr) return;
  editor_ = std::make_unique<TextField>();
  editor_->setFontSize(11.0f);
  editor_->setCornerRadius(4.0f);
  editor_->setPadding(0, 0, 0);
  editor_->setBackground(theme::kSurfaceRaised);
  editor_->setBorder(kEditorBorder);
  editor_->setJustification(juce::Justification::centred);
  editor_->setText(options_.scale->editText(value_));
  editor_->onEnter = [this] { commitEdit(); };
  editor_->onEscape = [this] { closeEditor(); };
  editor_->onBlur = [this] { commitEdit(); };
  addAndMakeVisible(*editor_);
  resized();
  editor_->focus();
  repaint();
}

void Knob::commitEdit() {
  if (editor_ == nullptr) return;
  const auto text = editor_->text().replace(",", ".").trim();
  if (text.isNotEmpty() && text.containsOnly("0123456789.-+eE")) {
    const double parsed = text.getDoubleValue();
    if (std::isfinite(parsed)) {
      const double norm = juce::jlimit<double>(options_.min, options_.max,
                                               options_.scale->fromDisplay(parsed));
      // Typed values keep the fine 1e-4 quantum; no detent.
      live_ = static_cast<float>(std::round(norm * 10000.0) / 10000.0);
      emit(live_);
    }
  }
  closeEditor();
}

void Knob::closeEditor() {
  if (editor_ == nullptr) return;
  // Hiding the editor drops its focus, which would re-enter through onBlur:
  // detach first, delete once the stack has unwound.
  auto editor = std::move(editor_);
  editor->onBlur = nullptr;
  editor->onEnter = nullptr;
  editor->onEscape = nullptr;
  removeChildComponent(editor.get());
  auto* raw = editor.release();
  juce::MessageManager::callAsync([raw] { delete raw; });
  repaint();
}

// Painting
void Knob::resized() {
  if (editor_ != nullptr)
    editor_->setBounds(labelBounds().expanded(0, kEditorOverflow));
}

void Knob::paint(juce::Graphics& g) {
  drawKnobFace(g, faceBounds().toFloat(), angleFor(options_.variant, value_),
               arcFromFor(options_.variant), options_.thumb == Thumb::primary
                                                  ? KnobTone::primary
                                                  : KnobTone::secondary);
  if (editor_ != nullptr) return;

  // Idle labels are muted by default; pan-rail labels pass labelBright to
  // read as section titles. The readout is always white.
  const bool readout = readoutVisible_;
  const auto colour = readout || options_.labelBright ? theme::kWhite : theme::kGray;
  const auto text = readout ? options_.scale->format(value_) : options_.label;
  // Wide labels centre on the knob and overflow the column symmetrically.
  g.setFont(Fonts::sans(kLabelSize));
  g.setColour(colour);
  g.drawText(text, labelBounds().expanded(60, 0), juce::Justification::centred, false);
}

}  // namespace t3k::ui
