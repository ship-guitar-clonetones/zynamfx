#include "OAuthOverlay.h"

namespace t3k::ui {

OAuthOverlay::OAuthOverlay(Backdrop backdrop, const ToneSession::AuthFlow& flow)
    : ModalLayer(std::move(backdrop)),
      retry_(body_.addButton("Try again", PillButton::Style::filled)),
      dismiss_(body_.addButton("Dismiss", PillButton::Style::outline)),
      cancel_(body_.addButton("Cancel", PillButton::Style::outline)) {
  setName("oauth overlay");
  setTitle("Signing in to TONE3000");
  retry_.onClick = [this] {
    if (onRetry) onRetry();
  };
  dismiss_.onClick = [this] {
    if (onDismiss) onDismiss();
  };
  cancel_.onClick = [this] {
    if (onCancel) onCancel();
  };
  setFlow(flow);
  setContent(body_);
}

void OAuthOverlay::setFlow(const ToneSession::AuthFlow& flow) {
  using Phase = ToneSession::AuthFlow::Phase;
  const bool error = flow.phase == Phase::error;
  body_.setBusy(!error);
  body_.setCopy(error ? (flow.error.isNotEmpty() ? flow.error : juce::String(kDefaultError)) : juce::String(),
                kBodyMaxW);
  retry_.setVisible(error);
  dismiss_.setVisible(error);
  cancel_.setVisible(false);
  cancelDelay_.cancel();
  if (flow.phase == Phase::leaving)
    cancelDelay_.start(kCancelDelayMs, [this] {
      cancel_.setVisible(true);
      body_.layout();
    });
  body_.layout();  // visibility drives the button row
}

}  // namespace t3k::ui
