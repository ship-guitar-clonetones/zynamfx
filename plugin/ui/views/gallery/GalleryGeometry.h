// Chain gallery layout constants (GalleryLane.tsx / GalleryBlock.tsx): tile
// edges, the connector gap (the visible run of each connector line), the
// plus ring geometry the lines meet, and the gutters the lanes fade under.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace t3k::ui::gallery {

inline constexpr int kTileSize = 128;
// Stereo shows two lanes, so its tiles shrink to fit the fixed height.
inline constexpr int kStereoTileSize = 112;
// Gap between tiles: the visible run of each connector line.
inline constexpr int kTileGap = 32;
// Vertical gap between the two stereo lanes.
inline constexpr int kLaneGap = 24;
// Gutter inside the scroll area; tiles fade out under it while scrolling.
inline constexpr int kEdgeFadeWidth = 32;
// Connector lines and the branch elbow.
inline constexpr float kLineWidth = 2.0f;
inline constexpr float kTileCorner = 8.0f;
// Add tile's dashed border, and the green one an OS file drag arms.
inline constexpr float kAddTileBorderWidth = 2.0f;
inline const juce::Colour kAddTileBorder = juce::Colour(141, 141, 147).withAlpha(0.65f);
inline const juce::Colour kFileDropBorder = juce::Colour(0, 209, 59).withAlpha(0.5f);
inline constexpr int kFileDropGlyphSize = 36;
// Design-px of travel before a drag engages, so tap/click stays a click.
inline constexpr int kDragDistance = 6;
// Opacity of the tile while it travels with the pointer.
inline constexpr float kDragGhostOpacity = 0.75f;

inline int tileSize(bool stereo) { return stereo ? kStereoTileSize : kTileSize; }

// Plus glyph: 48 on mono tiles (224), 40 on stereo (160). Half of that is
// the radius the routing lines run edge-to-circle against.
inline int plusIconSize(int tile) { return tile <= kStereoTileSize ? 40 : 48; }

// Lucide's circle-plus draws its circle at r=10 inside the 24-unit viewBox,
// so the visible ring sits 2/24 of the rendered size in from the icon's
// bounding box (measured to the stroke's centreline). Connector lines must
// overshoot the box by this much to actually meet the ring.
inline float plusCircleInset(int iconSize) { return iconSize * 2.0f / 24.0f; }

// X centre of the connector gap *before* the tile at `gapIndex` (gap g sits
// between tiles g-1 and g), in lane coordinates.
inline float gapCentreX(int gapIndex, int tile) {
  return gapIndex * static_cast<float>(tile + kTileGap) - kTileGap / 2.0f;
}

// Left edge of the tile at `index` in lane coordinates.
inline int tileX(int index, int tile) { return index * (tile + kTileGap); }
inline int laneWidth(int count, int tile) {
  return count <= 0 ? 0 : count * tile + (count - 1) * kTileGap;
}

// A rejoin ends the branch row: the trunk's merge gap, mapped into the branch
// lane's own coordinates (that lane starts `indent` past the trunk prefix).
// The branch slots sit on the trunk grid downstream of the tap, so this lands
// exactly on one of the branch lane's own connector gaps.
inline float joinCutX(float mergeGapX, int indent) {
  return mergeGapX - static_cast<float>(indent);
}

// Slots at or past the rejoin column are no longer offered (judged by the slot
// centre, so a slot straddling the join still sits before it).
inline bool slotAtOrPastJoin(int index, int tile, float cutX) {
  return static_cast<float>(tileX(index, tile)) + tile / 2.0f >= cutX;
}

}  // namespace t3k::ui::gallery
