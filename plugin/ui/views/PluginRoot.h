// Top-level native UI (port of Plugin.tsx): header, meters + chain (or a
// takeover: tuner, tone browser), faceplate, hint bar, and the overlay layer
// for popovers, the toast, banners and modals. Laid out in design space
// (1024 x 578 + chrome strips); the shell scales the whole thing.
//
// The chrome strips grow the window instead of squishing the 578px core, so
// the banner's arrival is choreographed against the window resize
// (useChromeChoreography) so the content column moves once, not twice:
//
//   hidden --banner appears--> waiting: the window grows first (the new
//     space is at the bottom edge, black on black; content stays put)
//   waiting --viewport grew (or 400ms)--> shown: the banner takes the top
//     strip, moving the content column down into the space the window
//     already has
//   shown --banner clears--> hidden: the strip goes and the window shrinks.
//
// Keyboard focus works as a browser's does: nothing is focused by default.
// A text field takes focus from a click; every other control only from
// Tab / Shift+Tab or from code asking (a popover opening, a field's
// focus()), never from a click (Clickable). JUCE would otherwise hand focus
// to the first focusable component whenever the focused one goes away or
// the window activates, which put the caret in the tone browser's search
// box on some visits and not others. The root is the keyboard focus
// container with a traverser that names no default; Tab with nothing
// focused enters the tab order at either end; Escape or a press elsewhere
// drops the focus again. Keys nothing takes (Space and Enter with nothing
// focused, Space with a button or knob focused) fall through to the host
// as its transport keys (NativeEditor).
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
  // A host may refuse or delay the resize; show the banner anyway after a beat.
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
  // Escape drops a focused control's focus (a text field takes its own Escape).
  bool keyPressed(const juce::KeyPress& key) override;
  std::unique_ptr<juce::ComponentTraverser> createKeyboardFocusTraverser() override;

private:
  enum class BannerPhase { hidden, waiting, shown };

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
  void bannerShow();
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
  DelayedCall bannerWait_;
  juce::Component* watchedParent_ = nullptr;

  // The two focus rules a browser has and JUCE lacks. Keys with nothing
  // focused go to the window's component, which we are inside of, not
  // above, so Tab-from-nothing listens on the window. A press outside the
  // focused control drops its focus, so a Tab-focused button never keeps
  // Enter from the host once the user is back on the mouse.
  class FocusPolicy : public juce::KeyListener, public juce::MouseListener {
  public:
    explicit FocusPolicy(PluginRoot& root) : root_(root) {}
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;
    void mouseDown(const juce::MouseEvent& e) override;

  private:
    PluginRoot& root_;
  };
  FocusPolicy focusPolicy_{*this};
  juce::Component::SafePointer<juce::Component> keyWindow_;
};

}  // namespace t3k::ui
