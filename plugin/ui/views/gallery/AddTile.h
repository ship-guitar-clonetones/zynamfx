// The insert slot as a dashed add tile (GalleryBlock.tsx AddTile), sortable
// so the insert point can be repositioned within its lane like any block.
// Routing lines continue the lane's connector line through to the plus
// circle. Also the drop zone for local .nam / IR .wav files.
//
// Click mapping: left-click the tile (or its plus circle) opens the tone
// loader for this slot — the NAM profile catalog; right-click / ctrl-click
// opens the hosted AU/VST3 effect chooser. The three chips in the top strip
// load a local .nam / IR file, a folder of them, or paste the copied block —
// one click each.
#pragma once

#include "GalleryTile.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

#include "widgets/ChromeIconButton.h"

namespace t3k::ui {

class AddTile : public GalleryTile {
public:
  // Which tile edges get a routing line into the plus circle (signal-flow
  // continuation of the lane's connector lines).
  enum class Routing { none, left, right, both };
  static Routing routingFor(int index, int count);

  AddTile(Services& services, std::string blockId, int size);

  void setRouting(Routing routing);
  // Paste the copied block into this slot; unset while there's nothing valid
  // to paste (the paste chip hides).
  void setCanPaste(bool canPaste);

  std::function<void(const std::string& insertBlockId)> onAdd;
  std::function<void(const std::string& insertBlockId)> onPaste;

  void paint(juce::Graphics& g) override;
  void resized() override;

protected:
  void open() override;
  void dropArmedChanged(bool armed) override;

private:
  void updateChips();
  // Load the local .nam / IR file · load a folder of them · paste.
  ChromeIconButton file_, folder_, paste_;
  Routing routing_ = Routing::none;
  bool canPaste_ = false;
};

}  // namespace t3k::ui
