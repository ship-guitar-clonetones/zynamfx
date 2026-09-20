// The band between header and faceplate on the main screen (Plugin.tsx
// middle section): input meter | centre | output meter. The horizontal inset
// is on the band; the centre column owns its own vertical padding so the
// meters always centre in the full header-to-faceplate height and never
// shift when the tone browser opens. The centre hosts the chain gallery or
// the tone-browser takeover.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ChainStore.h"
#include "services/ParamBinding.h"
#include "services/Services.h"
#include "ChainScreen.h"
#include "browser/ToneBrowser.h"
#include "widgets/DbMeter.h"

namespace t3k::ui {

class MainScreen : public juce::Component, private ChainStore::Listener {
public:
  static constexpr int kPadX = 24;
  // 358 matches Figma's BLOCK column (title + gap + card).
  static constexpr int kMeterHeight = 358;

  explicit MainScreen(Services& services);
  ~MainScreen() override;

  ChainScreen& chainScreen() { return chain_; }

  // The tone browser takeover in place of the chain (Plugin.tsx
  // showToneBrowser); mounted only while open.
  void setBrowserShown(bool shown);
  bool browserShown() const { return browser_ != nullptr; }
  // Configure a freshly mounted browser (its CTAs) before it shows.
  std::function<void(ToneBrowser&)> onBrowserMounted;

  void resized() override;

private:
  void chainChanged(const ChainState& state) override;
  void syncMeters();

  Services& services_;
  ParamBinding spreadEnabled_;
  DbMeter inputMeter_, outputMeter_;
  juce::Component centre_;
  ChainScreen chain_;
  std::unique_ptr<ToneBrowser> browser_;
};

}  // namespace t3k::ui
