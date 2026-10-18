// One lane of tiles over its ghost rail (GalleryLane.tsx): plus circles
// joined by connector lines sit behind the (opaque) tiles and show through
// a slot vacated mid-drag; only the line runs inside the gaps are visible
// otherwise. In stereo, hover-revealed branch dots live on the connector
// gaps (set / re-point the branch; the active tap gap's dot clears it), and
// on the trunk lane the gaps after the tap carry rejoin rings (merge the
// branch back in; the active merge gap's ring clears it). A rejoin ends the
// branch lane at the merge column: no add slot is drawn past it (real tiles
// there stay — they still process in the parallel section).
// Tiles are keyed by block id and reused across resyncs.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "AddTile.h"
#include "ToneTile.h"
#include "model/ChainState.h"
#include "services/Services.h"

namespace t3k::ui {

class GalleryLane : public juce::Component {
public:
  GalleryLane(Services& services, ChainSide side);
  ~GalleryLane() override;

  ChainSide side() const { return side_; }
  int tileSize() const { return tile_; }
  const std::vector<ChainItem>& items() const { return items_; }

  // Rebuild from a lane snapshot (native's, or the optimistic mirror).
  void setItems(const std::vector<ChainItem>& items, int tileSize);
  // Stereo: branch affordances on the gaps. `interactive` is false while a
  // drag is in flight (gap hit targets would fight drops).
  void setBranch(bool stereo, const std::optional<ChainBranch>& branch, bool interactive);
  void setCanPaste(bool canPaste);
  // The lane balance rides the last real (non-insert) slot; hidden when the
  // lane has none or the caller says balance does not apply.
  void setSlotPan(bool show);
  // The travelling tile's slot: hidden so the rail shows through.
  void setPlaceholder(const std::string& blockId);
  // Rejoin: end this (branch) lane at `localX` in the lane's own coordinates —
  // no add tile or ghost plus slot is drawn at or past the merge column. Real
  // tiles there are untouched: they still process before the sum.
  void setMergeCutoff(std::optional<float> localX);

  // Hand a tile between lanes: the leaving lane releases it (still parented
  // there until adopted), the entering lane reparents it and reuses it on
  // its next setItems. Mid-drag the dragged tile is the very component the
  // pointer's events are addressed to, so it has to survive the crossing.
  std::unique_ptr<GalleryTile> releaseTile(const std::string& blockId);
  void adoptTile(std::unique_ptr<GalleryTile> tile);

  GalleryTile* tileFor(const std::string& blockId) const;
  int indexOf(const std::string& blockId) const;

  std::function<void(const std::string& blockId)> onOpen, onSwap, onAdd;
  std::function<void(int index)> onPaste;
  std::function<void(const std::string& afterBlockId)> onSetBranch;
  std::function<void()> onClearBranch;
  // Rejoin the branch lane into the trunk after `afterBlockId` (trunk lane).
  std::function<void(const std::string& afterBlockId)> onSetMerge;
  std::function<void()> onClearMerge;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  class BranchGap;

  void rebuildBranchGaps();
  // Past a rejoin the branch lane offers no more slots: the item at `index` is
  // an insert anchor that fell at or beyond the merge column.
  bool slotHidden(int index) const;
  // Show every tile that is neither the travelling tile's slot nor a hidden
  // add slot.
  void applyVisibility();
  // Point a tile's callbacks at this lane (re-done on adoption).
  void wire(GalleryTile& tile);

  Services& services_;
  ChainSide side_;
  int tile_ = 0;
  std::vector<ChainItem> items_;
  std::map<std::string, std::unique_ptr<GalleryTile>> tiles_;
  std::string placeholder_;

  bool stereo_ = false;
  std::optional<ChainBranch> branch_;
  bool branchInteractive_ = false;
  std::vector<std::unique_ptr<BranchGap>> gaps_;
  // Rejoin column in this lane's own coordinates; empty while the lanes run
  // separately to the output.
  std::optional<float> mergeCutoff_;
  bool canPaste_ = false;
};

}  // namespace t3k::ui
