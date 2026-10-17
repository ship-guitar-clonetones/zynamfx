#include "AddTile.h"

#include <utility>

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "core/Fonts.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <map>

namespace t3k::ui {

namespace {
// The chip row along the tile's top edge: the same 4px inset the tone tiles'
// chrome uses, and the gap between chips (narrowed on the stereo tiles so all
// three fit a 112px slot).
constexpr int kChipPad = 4;
constexpr int kChipGap = 6;
constexpr int kChipY = 4;
constexpr int kChipMinGap = 2;
}  // namespace

AddTile::Routing AddTile::routingFor(int index, int count) {
  if (count <= 1) return Routing::none;
  if (index == 0) return Routing::right;
  if (index == count - 1) return Routing::left;
  return Routing::both;
}

AddTile::AddTile(Services& services, std::string blockId, int size)
    : GalleryTile(services, std::move(blockId), size),
      file_(Icon::File, ChromeIconButton::Tone::plain, help::Key::loadFileTile),
      folder_(Icon::FolderClosed, ChromeIconButton::Tone::plain, help::Key::loadFolderTile),
      paste_(Icon::ClipboardPaste, ChromeIconButton::Tone::plain, help::Key::pasteBlock) {
  setHelpText(help::text(help::Key::addTile));
  setTitle("Add Tone");

  // Local loading stays one click from the slot: the tone loader (the primary
  // click) is the catalog; these are the file pickers.
  file_.onClick = [this] { loadLocalFile(); };
  folder_.onClick = [this] { loadLocalFolder(); };
  paste_.onClick = [this] {
    if (onPaste) onPaste(this->blockId());
  };
  for (auto* chip : {&file_, &folder_, &paste_}) addAndMakeVisible(*chip);
  updateChips();  // nothing to paste yet
}

void AddTile::setRouting(Routing routing) {
  if (routing_ == routing) return;
  routing_ = routing;
  repaint();
}

void AddTile::setCanPaste(bool canPaste) {
  if (canPaste_ == canPaste) return;
  canPaste_ = canPaste;
  updateChips();
}

// A file drag is hovering: the tile is a drop zone, so the chips step aside.
void AddTile::dropArmedChanged(bool armed) {
  juce::ignoreUnused(armed);
  updateChips();
}

void AddTile::updateChips() {
  const bool shown = !dropArmed();
  file_.setVisible(shown);
  folder_.setVisible(shown);
  paste_.setVisible(shown && canPaste_);
  resized();
}

void AddTile::open() {
  if (onAdd) onAdd(blockId());
}

void AddTile::resized() {
  // Right-aligned along the top edge, clear of the plus circle.
  const int box = theme::kIconBoxSize;
  const int avail = getWidth() - 2 * kChipPad - box - kChipMinGap;
  const int n = (paste_.isVisible() ? 3 : 2);
  int gap = kChipGap;
  if (n * box + (n - 1) * gap > avail)
    gap = juce::jmax(0, (avail - n * box) / juce::jmax(1, n - 1));
  int x = getWidth() - kChipPad - box;
  for (auto* chip : {&paste_, &folder_, &file_}) {
    if (!chip->isVisible()) continue;
    chip->setBounds(x, kChipY, box, box);
    x -= box + gap;
  }
}

void AddTile::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  const bool armed = dropArmed();
  paint::fill(g, box, gallery::kTileCorner, theme::kSurfaceRaised);
  paint::dashedBorder(g, box, gallery::kTileCorner,
                      armed ? gallery::kFileDropBorder : gallery::kAddTileBorder,
                      gallery::kAddTileBorderWidth);

  if (armed) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, box.withSizeKeepingCentre(s, s), theme::kGray);
    return;
  }

  const int size = tileSize();
  const int icon = gallery::plusIconSize(size);
  // Routing lines are anchored inside the border (absolute children position
  // against the padding box), so the run to the plus ring is a border-width
  // shorter than measured from the tile's outer edge.
  if (!travelling() && routing_ != Routing::none) {
    const float run = size / 2.0f - icon / 2.0f + gallery::plusCircleInset(icon) -
                      gallery::kAddTileBorderWidth;
    const float y = box.getCentreY() - gallery::kLineWidth / 2;
    g.setColour(theme::kWhite);
    if (routing_ == Routing::left || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(gallery::kAddTileBorderWidth, y, run, gallery::kLineWidth));
    if (routing_ == Routing::right || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(box.getRight() - gallery::kAddTileBorderWidth - run, y, run,
                                        gallery::kLineWidth));
  }
  Icons::draw(g, Icon::PlusCircle, box.withSizeKeepingCentre(icon, icon), theme::kEffect,
              /*strokeWidth=*/1.0f);
  g.setFont(Fonts::mono(9));g.setColour(theme::kMuted);
  g.drawText("ADD BLOCK",8,getHeight()-29,getWidth()-16,16,juce::Justification::centred,false);
}

}  // namespace t3k::ui
