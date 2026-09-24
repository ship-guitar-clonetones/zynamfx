#include "DimGroup.h"

#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kFadeMs = 200;
}  // namespace

DimGroup::DimGroup() { setInterceptsMouseClicks(false, true); }

void DimGroup::setOff(bool off, bool animate) {
  if (off_ == off) return;
  off_ = off;
  // Off: the group takes the pointer (and its own hint), children see nothing.
  setInterceptsMouseClicks(off, !off);
  fade_.animateTo(off ? theme::kDisabledOpacity : 1.0f, kFadeMs, animate);
}

}  // namespace t3k::ui
