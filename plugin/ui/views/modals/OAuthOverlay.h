// Busy scrim over the whole plugin while an OAuth redirect is in flight
// (OAuthOverlay.tsx): leaving for tone3000.com, or resolving the callback
// after landing back. The normal UI keeps rendering underneath (dimmed and
// blurred) instead of a blank takeover, so the user comes straight back to
// the view they'll interact with. Errors surface on the same scrim with a
// retry that restarts whichever flow actually failed.
//
// The webview navigated away wholesale, so "leaving" had no way out; the
// system browser can be closed without ever coming back, so after a beat
// the scrim offers Cancel (the only addition to the web's overlay).
#pragma once

#include <functional>

#include "core/DelayedCall.h"
#include "services/ToneSession.h"
#include "widgets/ModalLayer.h"
#include "widgets/ScrimMessage.h"

namespace t3k::ui {

class OAuthOverlay : public ModalLayer {
public:
  static constexpr int kBodyMaxW = 360;
  static constexpr const char* kDefaultError = "Something went wrong completing TONE3000 sign-in.";
  // How long the browser gets before Cancel appears.
  static constexpr int kCancelDelayMs = 4000;

  OAuthOverlay(Backdrop backdrop, const ToneSession::AuthFlow& flow);

  // Re-render for a new phase without tearing the scrim down.
  void setFlow(const ToneSession::AuthFlow& flow);

  std::function<void()> onRetry, onDismiss, onCancel;

private:
  ScrimMessage body_;
  PillButton& retry_;
  PillButton& dismiss_;
  PillButton& cancel_;
  DelayedCall cancelDelay_;
};

}  // namespace t3k::ui
