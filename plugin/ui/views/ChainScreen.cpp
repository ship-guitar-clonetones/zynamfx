#include "ChainScreen.h"

#include <algorithm>
#include "core/Fonts.h"
#include "core/BlockArt.h"
#include "core/Paint.h"

namespace t3k::ui {

ChainScreen::ChainScreen(Services& services) : services_(services), gallery_(services) {
  gallery_.onOpenBlock = [this](const std::string& id) { openDetail(id); };
  gallery_.onSelectTone = [this](ChainSide side, const std::string& id) {
    if (onSelectTone) onSelectTone(side, id);
  };
  addAndMakeVisible(gallery_);
  services_.chain.addListener(this);

  // Reopen the card a swap / OAuth round trip left behind, unless its block
  // is gone (the chain changed underneath the round trip).
  const auto saved = services_.prefs.session.find(UiPrefs::kDetailBlockId);
  if (saved != services_.prefs.session.end() && saved->second.isNotEmpty()) {
    const auto id = saved->second.toStdString();
    if (services_.chain.state().findBlock(id) != nullptr)
      openDetail(id);
    else
      services_.prefs.session.erase(UiPrefs::kDetailBlockId);
  }
}

ChainScreen::~ChainScreen() { services_.chain.removeListener(this); }

void ChainScreen::openDetail(const std::string& blockId) {
  services_.prefs.session[UiPrefs::kDetailBlockId] = juce::String(blockId);
  detail_ = std::make_unique<BlockDetail>(services_, blockId);
  // The card may ask to close from inside a chain listener callback (its
  // block vanished); tear it down once that callback has unwound.
  detail_->onBack = [this] {
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChainScreen>(this)] {
      if (safe != nullptr) safe->closeDetail();
    });
  };
  detail_->onSwap = [this](const std::string& id) {
    // The swap keeps the block id; the detail id stays saved for the return.
    const auto& state = services_.chain.state();
    const bool right = state.chainRight && std::any_of(state.chainRight->begin(), state.chainRight->end(),
                                                       [&](const ChainItem& b) { return b.blockId == id; });
    if (onSelectTone) onSelectTone(right ? ChainSide::right : ChainSide::left, id);
  };
  addAndMakeVisible(*detail_);
  gallery_.setVisible(true);
  resized();
}

void ChainScreen::closeDetail() {
  services_.prefs.session.erase(UiPrefs::kDetailBlockId);
  if (!detail_) return;
  detail_.reset();
  gallery_.setVisible(true);
  resized();
}

void ChainScreen::returnToGallery() {
  closeDetail();
  gallery_.returnToGallery();
}

void ChainScreen::resized() {
 repaint();
 const bool stereo=services_.chain.state().stereoEnabled;
 const int signalHeight=stereo ? 296 : 184;
 gallery_.setBounds(getLocalBounds().withHeight(signalHeight));
 if(detail_) detail_->setBounds(getLocalBounds().withTrimmedTop(signalHeight));
}
void ChainScreen::paint(juce::Graphics& g) {
 const int top=services_.chain.state().stereoEnabled ? 296 : 184;
 const auto panel=getLocalBounds().withTrimmedTop(top).reduced(12,0);
 paint::fill(g,panel.toFloat(),6.0f,theme::kSurface);
 paint::border(g,panel.toFloat(),6.0f,theme::kBorder);
 if(detail_) return;
 g.setColour(theme::kAccent);g.fillRect(panel.getX(),panel.getY(),3,panel.getHeight());
 auto area=panel.reduced(28,20);
 paint::text(g,"BLOCK EDITOR",area.removeFromTop(18),Fonts::mono(10,true),theme::kAccent);
 area.removeFromTop(12);
 paint::text(g,"Your signal. Your sound.",area.removeFromTop(30),Fonts::sans(23,true),theme::kWhite);
 area.removeFromTop(8);
 paint::text(g,"Select a block above to edit its model, levels, mix and EQ.",area.removeFromTop(20),Fonts::sans(13),theme::kMuted);
 if(area.getHeight()>90) {
  const juce::String ids[]{"amp","cab","pedal","outboard"};
  const int w=area.getWidth()/4;
  for(int i=0;i<4;++i) {
   const auto slot=area.withWidth(w).translated(i*w,20);
   blockart::draw(g,slot.withHeight(40).toFloat().withSizeKeepingCentre(34,34),ids[i],i==3);
   paint::text(g,blockart::label(ids[i],i==3),slot.withTrimmedTop(48).withHeight(16),Fonts::mono(9),blockart::colour(ids[i],i==3),juce::Justification::centred);
  }
 }
}
} // namespace t3k::ui
