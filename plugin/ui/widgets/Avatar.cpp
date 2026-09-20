#include "Avatar.h"

#include "core/Bitmap.h"
#include "core/CustomIcons.h"
#include "core/Icons.h"
#include "core/Theme.h"

namespace t3k::ui {

void Avatar::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  if (!image_.isValid()) {
    Icons::draw(g, custom_icons::kAvatarFallback, box, theme::kGray);
    return;
  }
  const float scale = bitmap::pixelScale(g);
  if (!cover_.isValid() || !juce::approximatelyEqual(scale, coverScale_)) {
    cover_ = bitmap::cover(image_, getWidth(), getHeight(), scale);
    coverScale_ = scale;
  }
  // object-fit: cover inside a circular clip.
  juce::Path clip;
  clip.addEllipse(box);
  g.reduceClipRegion(clip);
  bitmap::draw(g, cover_, getLocalBounds());
}

}  // namespace t3k::ui
