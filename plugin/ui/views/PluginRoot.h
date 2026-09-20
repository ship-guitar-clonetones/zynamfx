// Top-level native UI (port of Plugin.tsx): header, meters + chain (or a
// takeover: tuner, tone browser), faceplate, hint bar, and the overlay layer
// for popovers, the toast, banners and modals. Laid out in design space
// (1024 x 578 + chrome strips); the shell scales the whole thing.
//
// The chrome strips grow the window instead of squishing the 578px core, so
// the banner's arrival is choreographed against the window resize
// (useChromeChoreography) so existing content never jumps:
//
//   hidden --banner appears--> waiting: the window grows first (the new
//     space is at the bottom edge, black on black; content stays put)
//   waiting --viewport grew (or 400ms)--> entering: the banner slides down
//     into place over kBannerAnimMs, pushing the content column down into
//     the space the window already has
//   entering --> shown; shown --banner clears--> leaving: reverse slide with
//     the last spec kept rendered; leaving --> hidden: the window shrinks.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "AppBanner.h"
#include "Faceplate.h"
#include "HintBar.h"
#include "MainScreen.h"
#include "PluginHeader.h"
#include "ToastView.h"
#include "TunerView.h"
#include "core/DelayedCall.h"
#include "core/Tween.h"
#include "modals/ConnectionModal.h"
#include "modals/OAuthOverlay.h"
#include "modals/UpdateNotice.h"
#include "settings/SettingsScreen.h"
#include "services/Services.h"
#include "widgets/Popover.h"

namespace t3k::ui {

class PluginRoot : public juce::Component,
                   public OverlayHost,
                   private HintBus::Listener,
                   private BannerStore::Listener,
                   private ConnectionGate::Listener,
                   private UpdateCheck::Listener,
                   private ToneSession::Listener,
                   private juce::ComponentListener {
public:
  static constexpr int kBannerAnimMs = 180;
  // A host may refuse or delay the resize; slide anyway after a beat.
  static constexpr int kBannerWaitMs = 400;

  explicit PluginRoot(Services& services);
  ~PluginRoot() override;

  // Current design-space height: the core plus whatever chrome strips show.
  int designHeight() const { return getHeight(); }

  Services& services() { return services_; }
  juce::Component& overlayLayer() override { return overlay_; }
  // Everything under the overlay layer, for the modals' blurred scrims.
  juce::Image snapshotBeneathOverlay(float scale);

  // The tuner takeover replaces the meters + chain band; the pitch
  // detector runs only while it is up.
  void setTunerShown(bool shown);
  bool tunerShown() const { return tuner_ != nullptr; }
  // The tone browser takeover replaces the chain between the meters.
  void setBrowserShown(bool shown);
  bool browserShown() const { return main_.browserShown(); }

  // The Settings takeover covers the whole window (chrome strips included)
  // under the overlay layer; mounted only while open. Banner actions and the
  // account menu land on System (setup first); hosted builds have one page.
  void openSettings(SettingsScreen::Tab tab = SettingsScreen::Tab::system);
  void closeSettings();
  SettingsScreen* settings() { return settings_.get(); }

  void paint(juce::Graphics& g) override;
  void resized() override;
  void parentHierarchyChanged() override;

private:
  enum class BannerPhase { hidden, waiting, entering, shown, leaving };

  void hintChanged() override;
  void bannerChanged() override;
  void connectionProblemChanged() override;
  void updateNoticeChanged() override;
  void sessionChanged() override {}
  void authFlowChanged() override;
  void componentMovedOrResized(juce::Component& parent, bool moved, bool resized) override;
  // Modals stack in the overlay layer (Plugin.tsx z-order): update notice,
  // then the OAuth overlay, then the connection modal on top, all above
  // popovers and the toast.
  template <typename Modal, typename... Args>
  std::unique_ptr<Modal> openModal(Args&&... args);
  void restackModals();
  void updateChromeHeight();
  // Banner phase machine.
  bool viewportFits() const;
  void bannerEnter();
  void bannerLeave();
  void handleBannerAction(BannerAction action);
  // Top-bar actions whose effect lands on the main screen leave the tuner
  // first so the result is visible.
  void closeTunerThen(const std::function<void()>& fn);
  // Loading a preset / resetting replaces the chain: leave any takeover.
  void showChainThen(const std::function<void()>& fn);
  void logout();

  Services& services_;
  PluginHeader header_;
  HintBar hintBar_;
  MainScreen main_;  // meters + chain gallery / tone browser
  std::unique_ptr<TunerView> tuner_;
  std::unique_ptr<SettingsScreen> settings_;
  Faceplate faceplate_;
  AppBanner banner_;
  juce::Component overlay_;
  ToastView toast_;
  std::unique_ptr<UpdateNotice> updateNotice_;
  std::unique_ptr<OAuthOverlay> oauthOverlay_;
  std::unique_ptr<ConnectionModal> connectionModal_;
  HintTracker hintTracker_;
  bool hintsVisible_ = true;

  BannerPhase bannerPhase_ = BannerPhase::hidden;
  // The slide slot's height (0 → AppBanner::kHeight); the banner hangs from
  // its bottom edge so it slides down from behind the top.
  Tween bannerSlot_;
  DelayedCall bannerWait_;
  juce::Component* watchedParent_ = nullptr;
};

}  // namespace t3k::ui
