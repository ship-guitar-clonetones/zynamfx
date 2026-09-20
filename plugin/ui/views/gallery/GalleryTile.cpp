#include "GalleryTile.h"

#include "GalleryGeometry.h"
#include "core/Design.h"

namespace t3k::ui {

GalleryTile::GalleryTile(Services& services, std::string blockId, int size)
    : services_(services), blockId_(std::move(blockId)), size_(size) {
  setSize(size, size);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  // Sortable tiles are focusable (keyboard sorting) but draw no focus ring,
  // as the web didn't.
  setWantsKeyboardFocus(true);
  setMouseClickGrabsKeyboardFocus(false);
}

GalleryTile::~GalleryTile() {
  // The menu lives on the overlay layer; take it down with the tile.
  if (menu_ != nullptr) menu_->close();
}

TileDragHost* GalleryTile::host() { return findParentComponentOfClass<TileDragHost>(); }

void GalleryTile::setTravelling(bool travelling) {
  if (travelling_ == travelling) return;
  travelling_ = travelling;
  setAlpha(travelling ? gallery::kDragGhostOpacity : 1.0f);
  travellingChanged(travelling);
}

void GalleryTile::setDropArmed(bool armed) {
  if (dropArmed_ == armed) return;
  dropArmed_ = armed;
  dropArmedChanged(armed);
  repaint();
}

void GalleryTile::filesDropped(const juce::StringArray& files, int, int) {
  setDropArmed(false);
  services_.localFiles.drop(blockId_, files);
}

std::vector<ContextMenu::Item> GalleryTile::localLoadItems() {
  return {
      {"Load File", Icon::File, help::Key::loadFileTile,
       [this] { services_.localFiles.pick(blockId_, LocalFiles::Kind::file); }},
      {"Load Folder", Icon::FolderClosed, help::Key::loadFolderTile,
       [this] { services_.localFiles.pick(blockId_, LocalFiles::Kind::folder); }},
  };
}

// Menu
void GalleryTile::openMenu(juce::Point<int> at) {
  suppressClickUntilMs_ = juce::Time::currentTimeMillis() + kSuppressClickMs;
  if (auto* old = menu_.release()) {
    old->close();
    juce::MessageManager::callAsync([old] { delete old; });
  }
  menu_ = std::make_unique<ContextMenu>(menuItems());
  menu_->onDismiss = [this] { menuDismissedMs_ = juce::Time::currentTimeMillis(); };
  menu_->openAtPoint(*this, at);
}

void GalleryTile::closeMenu() {
  if (menu_ != nullptr) menu_->close();  // kept alive: we may be inside its row's click
}

// Pointer
void GalleryTile::mouseDown(const juce::MouseEvent& e) {
  const auto now = juce::Time::currentTimeMillis();
  dragging_ = false;
  pressAt_ = e.getPosition();

  // Right-click / ctrl-click: the action sheet, and the click that follows
  // (macOS ctrl-click fires both) is swallowed.
  if (e.mods.isPopupMenu() && !e.source.isTouch()) {
    openMenu(e.getPosition());
    return;
  }
  // A press that just dismissed the sheet (outside-press) closes it only.
  if (now - menuDismissedMs_ < 100) suppressClickUntilMs_ = now + kSuppressClickMs;

  // Coarse pointer: no contextmenu event, so a held touch opens the sheet
  // itself, on the system's long-press delay, while the finger is down.
  if (e.source.isTouch() && design::kCoarsePointer) {
    hold_.start(kLongPressMs, [this, at = e.getPosition()] {
      openMenu(at.translated(0, kLongPressMenuDrop));
    });
  }
}

void GalleryTile::mouseDrag(const juce::MouseEvent& e) {
  if (dragging_) {
    if (auto* h = host()) h->tileDragMove(e);
    return;
  }
  const auto travel = e.getPosition() - pressAt_;
  if (hold_.pending() &&
      (std::abs(travel.x) > kLongPressSlop || std::abs(travel.y) > kLongPressSlop))
    hold_.cancel();
  if (!e.mods.isLeftButtonDown() && !e.source.isTouch()) return;
  if (e.getDistanceFromDragStart() < gallery::kDragDistance) return;
  // Past the activation distance the press is a drag; a sheet the hold
  // already opened yields to it.
  closeMenu();
  hold_.cancel();
  dragging_ = true;
  if (auto* h = host()) h->tileDragStart(*this, e);
}

void GalleryTile::mouseUp(const juce::MouseEvent& e) {
  hold_.cancel();
  if (dragging_) {
    dragging_ = false;
    if (auto* h = host()) h->tileDragEnd(e);
    return;
  }
  if (e.mods.isPopupMenu() && !e.source.isTouch()) return;
  if (!e.mouseWasClicked()) return;
  const auto now = juce::Time::currentTimeMillis();
  if (now < suppressClickUntilMs_) {
    suppressClickUntilMs_ = 0;
    return;
  }
  if (e.mods.isCtrlDown() || e.mods.isCommandDown()) return;
  if (menuOpen()) {
    closeMenu();
    return;
  }
  open();
}

bool GalleryTile::keyPressed(const juce::KeyPress& key) {
  if (auto* h = host()) return h->tileKey(*this, key);
  return false;
}

}  // namespace t3k::ui
