#include "Popover.h"

namespace t3k::ui {

Popover::Popover() {
  setWantsKeyboardFocus(true);
  setMouseClickGrabsKeyboardFocus(false);
}

Popover::~Popover() { juce::Desktop::getInstance().removeGlobalMouseListener(&watcher_); }

void Popover::open(juce::Component& anchor, Align align, int gap, int inset, Placement placement) {
  auto* host = anchor.findParentComponentOfClass<OverlayHost>();
  jassert(host != nullptr);
  if (host == nullptr) return;

  anchor_ = &anchor;
  align_ = align;
  placement_ = placement;
  gap_ = gap;
  inset_ = inset;
  host->overlayLayer().addAndMakeVisible(this);
  reposition();
  juce::Desktop::getInstance().addGlobalMouseListener(&watcher_);
  if (isShowing()) grabKeyboardFocus();  // offscreen (testbed capture) has no peer
}

void Popover::openAt(juce::Component& context, juce::Point<int> point) {
  auto* host = context.findParentComponentOfClass<OverlayHost>();
  jassert(host != nullptr);
  if (host == nullptr) return;

  anchor_ = nullptr;
  auto& overlay = host->overlayLayer();
  overlay.addAndMakeVisible(this);
  const auto p = overlay.getLocalPoint(&context, point);
  setTopLeftPosition(juce::jlimit(0, std::max(0, overlay.getWidth() - getWidth()), p.x),
                     juce::jlimit(0, std::max(0, overlay.getHeight() - getHeight()), p.y));
  juce::Desktop::getInstance().addGlobalMouseListener(&watcher_);
  if (isShowing()) grabKeyboardFocus();
}

void Popover::reposition() {
  auto* overlay = getParentComponent();
  if (overlay == nullptr || anchor_ == nullptr) return;
  const auto a = overlay->getLocalArea(anchor_.getComponent(), anchor_->getLocalBounds());
  const int x = align_ == Align::left ? a.getX() + inset_ : a.getRight() - inset_ - getWidth();
  const int y = placement_ == Placement::below ? a.getBottom() + gap_ : a.getY() - gap_ - getHeight();
  setTopLeftPosition(x, y);
}

void Popover::close() {
  if (!isOpen()) return;
  juce::Desktop::getInstance().removeGlobalMouseListener(&watcher_);
  if (auto* parent = getParentComponent()) parent->removeChildComponent(this);
}

void Popover::dismiss() {
  close();
  if (onDismiss) onDismiss();
}

bool Popover::keyPressed(const juce::KeyPress& key) {
  if (key == juce::KeyPress::escapeKey) {
    dismiss();
    return true;
  }
  return false;
}

void Popover::outsidePress(const juce::MouseEvent& e) {
  if (primaryOnly && !e.mods.isLeftButtonDown()) return;
  auto* c = e.eventComponent;
  // Other windows (another plugin instance) don't count as "outside".
  if (c == nullptr || c->getTopLevelComponent() != getTopLevelComponent()) return;
  if (isParentOf(c) || c == this) return;
  if (!dismissOnAnchorPress && anchor_ != nullptr && (c == anchor_ || anchor_->isParentOf(c))) return;
  dismiss();
}

}  // namespace t3k::ui
