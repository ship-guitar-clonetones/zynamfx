#include "MainScreen.h"

namespace t3k::ui {

MainScreen::MainScreen(Services& services)
    : services_(services),
      spreadEnabled_(services.backend, "spreadEnabled"),
      inputMeter_(services.meters, /*input=*/true, kMeterHeight, DbMeter::Labels::left),
      outputMeter_(services.meters, /*input=*/false, kMeterHeight, DbMeter::Labels::right),
      chain_(services) {
  addAndMakeVisible(inputMeter_);
  addAndMakeVisible(outputMeter_);
  addAndMakeVisible(centre_);
  centre_.addAndMakeVisible(chain_);
  // The meters sit above the centre's content (a tone-browser header scrim),
  // so stereo columns overflowing into the centre aren't covered by it.
  inputMeter_.toFront(false);
  outputMeter_.toFront(false);

  spreadEnabled_.onChange = [this] { syncMeters(); };
  services_.chain.addListener(this);
  syncMeters();
}

MainScreen::~MainScreen() { services_.chain.removeListener(this); }

void MainScreen::chainChanged(const ChainState&) { syncMeters(); }

void MainScreen::syncMeters() {
  const auto& chain = services_.chain.state();
  inputMeter_.setStereo(chain.stereoInput && chain.inputMode == InputMode::stereo);
  // The output carries a real stereo image only when a stereo-image feature
  // is on (stereo mode, or mono-mode spread) AND the rig can reproduce it.
  outputMeter_.setStereo((chain.stereoEnabled || spreadEnabled_.boolValue()) && chain.stereoOutput);
}

void MainScreen::setBrowserShown(bool shown) {
  if (shown == browserShown()) return;
  if (shown) {
    browser_ = std::make_unique<ToneBrowser>(services_);
    if (onBrowserMounted) onBrowserMounted(*browser_);
    centre_.addAndMakeVisible(*browser_);
    browser_->setBounds(centre_.getLocalBounds());
  } else {
    browser_.reset();
  }
  chain_.setVisible(!shown);
}

void MainScreen::resized() {
  auto area = getLocalBounds().reduced(kPadX, 0);
  // Each meter slot is the mono footprint; the component is wider by kInset
  // on both sides to hold the stereo overflow.
  const int meterY = (getHeight() - inputMeter_.getHeight()) / 2;
  inputMeter_.setTopLeftPosition(area.getX() - DbMeter::kInset, meterY);
  outputMeter_.setTopLeftPosition(area.getRight() - DbMeter::kFootprint - DbMeter::kInset, meterY);
  area.removeFromLeft(DbMeter::kFootprint);
  area.removeFromRight(DbMeter::kFootprint);
  centre_.setBounds(area);
  chain_.setBounds(centre_.getLocalBounds());
  if (browser_) browser_->setBounds(centre_.getLocalBounds());
}

}  // namespace t3k::ui
