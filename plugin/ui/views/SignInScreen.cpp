#include "SignInScreen.h"

#include <algorithm>
#include <cmath>

#include "core/Design.h"
#include "core/Fonts.h"
#include "core/Help.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
using Phase = ToneSession::AuthFlow::Phase;
using DeviceState = ToneSession::AuthFlow::Device::State;

constexpr const char* kBrowserCopy = "Sign in on your browser then return here.";
constexpr const char* kBrowserDidntOpen = "Browser didn't open?";
// After AuthFlow::browserProblem's sentence.
constexpr const char* kProblemHelp = "Copy the link into a browser on this machine, or sign in with your phone.";
constexpr const char* kGettingCode = "Getting a code for your phone\xe2\x80\xa6";
constexpr const char* kOrBrowser = "Or finish signing in on your browser.";
// After AuthFlow::Device::error's sentence.
constexpr const char* kDeviceHelp = "Get a new one, or finish on your browser.";
constexpr const char* kReturning = "Finishing sign-in\xe2\x80\xa6";
constexpr const char* kCopyLink = "Copy link", *kCopied = "Copied";
constexpr const char* kLinkCopied = "Sign-in link copied";

constexpr float kCopyPx = 14, kMutedPx = 13, kCodePx = 24;
constexpr int kDotsRow = 32;  // the mockup's frame round the dots
constexpr int kGap = 16, kTightGap = 8, kSectionGap = 32, kButtonGap = 12;
constexpr int kCopyIcon = 14;
}  // namespace

SignInScreen::SignInScreen(Services& services)
    : services_(services),
      back_({}, help::Key::signInBack),
      copyLink_(kCopyLink, PillButton::Style::outline),
      phone_("Use your phone", PillButton::Style::outline),
      newCode_("New code", PillButton::Style::filled),
      retry_("Try again", PillButton::Style::filled),
      dismiss_("Dismiss", PillButton::Style::outline) {
  setOpaque(true);  // the page, not a scrim: nothing shows through
  setName("sign in");

  back_.onClick = [this] { back(); };
  addAndMakeVisible(back_);
  addChildComponent(dots_);
  addChildComponent(qr_);

  copyLink_.setLeadingIcon(Icon::Copy, kCopyIcon);
  copyLink_.setHelpText(help::text(help::Key::signInCopyLink));
  copyLink_.onClick = [this] { copyLink(); };
  phone_.setHelpText(help::text(help::Key::signInPhone));
  phone_.onClick = [this] { services_.session.startDeviceFlow(); };
  newCode_.setHelpText(help::text(help::Key::signInNewCode));
  newCode_.onClick = [this] { services_.session.startDeviceFlow(); };
  retry_.setHelpText(help::text(help::Key::signInRetry));
  retry_.onClick = [this] { services_.session.retryFlow(); };
  dismiss_.setHelpText(help::text(help::Key::signInDismiss));
  dismiss_.onClick = [this] { services_.session.clearAuthError(); };
  for (auto* b : {&copyLink_, &phone_, &newCode_, &retry_, &dismiss_}) addChildComponent(*b);

  services_.session.addListener(this);
  rebuild();
}

SignInScreen::~SignInScreen() { services_.session.removeListener(this); }

// Actions
void SignInScreen::back() {
  auto& session = services_.session;
  if (session.authFlow().phase == Phase::error) session.clearAuthError();
  else session.cancelFlow();
}

void SignInScreen::copyLink() {
  juce::SystemClipboard::copyTextToClipboard(services_.session.authFlow().authorizeUrl);
  copyLink_.setLabel(kCopied);
  layoutColumn();
  help::announce(kLinkCopied);
  copiedDelay_.start(kCopiedMs, [this] {
    copyLink_.setLabel(kCopyLink);
    layoutColumn();
  });
}

void SignInScreen::authFlowChanged() {
  juce::MessageManager::callAsync([self = juce::Component::SafePointer(this)] {
    if (self != nullptr) self->rebuild();
  });
}

// The column
SignInScreen::Row& SignInScreen::addComponent(juce::Component& c, int gapBefore) {
  c.setVisible(true);
  rows_.push_back({gapBefore, c.getHeight(), c.getWidth(), &c, -1, {}});
  return rows_.back();
}

SignInScreen::Row& SignInScreen::addText(const juce::String& text, const juce::Font& font, juce::Colour colour,
                                         int maxWidth, int gapBefore) {
  Text t;
  t.flow = std::make_unique<TextFlow>(font, static_cast<float>(Fonts::normalLineHeight(font)), text,
                                      static_cast<float>(maxWidth));
  t.colour = colour;
  t.maxWidth = maxWidth;
  const int h = static_cast<int>(std::ceil(t.flow->height()));
  texts_.push_back(std::move(t));
  rows_.push_back({gapBefore, h, maxWidth, nullptr, static_cast<int>(texts_.size()) - 1, {}});
  return rows_.back();
}

SignInScreen::Row& SignInScreen::addButtons(std::vector<PillButton*> buttons, int gapBefore) {
  int w = 0, h = 0;
  for (auto* b : buttons) {
    b->setVisible(true);
    w += (w > 0 ? kButtonGap : 0) + b->getWidth();
    h = std::max(h, b->getHeight());
  }
  rows_.push_back({gapBefore, h, w, nullptr, -1, std::move(buttons)});
  return rows_.back();
}

juce::String SignInScreen::typedUri(const juce::String& uri) {
  auto typed = uri.fromFirstOccurrenceOf("://", false, false);
  if (typed.isEmpty()) typed = uri;
  if (typed.startsWith("www.")) typed = typed.substring(4);
  return typed.trimCharactersAtEnd("/");
}

void SignInScreen::rebuild() {
  const auto& flow = services_.session.authFlow();
  rows_.clear();
  texts_.clear();
  for (juce::Component* c : std::initializer_list<juce::Component*>{&dots_, &qr_, &copyLink_, &phone_, &newCode_,
                                                                     &retry_, &dismiss_})
    c->setVisible(false);
  const auto copy = Fonts::sans(kCopyPx), muted = Fonts::sans(kMutedPx), code = Fonts::mono(kCodePx, true);
  juce::String headline;  // what a screen reader hears of this state

  if (flow.phase == Phase::error) {
    headline = flow.error.isNotEmpty() ? flow.error : juce::String(kDefaultError);
    addText(headline, copy, theme::kWhite, kWideMaxWidth, 0);
    addButtons({&retry_, &dismiss_}, kGap);
  } else if (flow.phase == Phase::returning) {
    headline = juce::String::fromUTF8(kReturning);
    addComponent(dots_, 0).height = kDotsRow;
    addText(headline, copy, theme::kWhite, kCopyMaxWidth, kGap);
  } else if (flow.device && flow.device->state == DeviceState::requesting) {
    headline = juce::String::fromUTF8(kGettingCode);
    addComponent(dots_, 0).height = kDotsRow;
    addText(headline, copy, theme::kWhite, kCopyMaxWidth, kGap);
  } else if (flow.device && flow.device->state == DeviceState::waiting) {
    const auto& device = *flow.device;
    qr_.setText(device.verificationUriComplete);
    qr_.setSize(kQrSize, kQrSize);
    addComponent(qr_, 0);
    const auto scan = "Scan the code with your phone, or go to " + typedUri(device.verificationUri) + " and enter";
    headline = scan + " " + device.userCode;
    addText(scan, copy, theme::kWhite, kWideMaxWidth, kGap);
    addText(device.userCode, code, theme::kWhite, kWideMaxWidth, kTightGap);
    addComponent(dots_, kGap).height = LoadingDots::kDot;
    addText(kOrBrowser, muted, theme::kMuted, kWideMaxWidth, kGap);
  } else if (flow.device && flow.device->state == DeviceState::failed) {
    headline = flow.device->error + " " + kDeviceHelp;
    addText(headline, copy, theme::kWhite, kWideMaxWidth, 0);
    addButtons({&newCode_, &copyLink_}, kGap);
  } else {
    // Waiting on the browser (the mockup), the fallbacks under it.
    addComponent(dots_, 0).height = kDotsRow;
    if (flow.browserProblem.isNotEmpty()) {
      headline = flow.browserProblem + " " + kProblemHelp;
      addText(headline, copy, theme::kWhite, kWideMaxWidth, kGap);
      addButtons({&copyLink_, &phone_}, kGap);
    } else {
      headline = kBrowserCopy;
      addText(headline, copy, theme::kWhite, kCopyMaxWidth, kGap);
      addText(kBrowserDidntOpen, muted, theme::kMuted, kWideMaxWidth, kSectionGap);
      addButtons({&copyLink_, &phone_}, kButtonGap);
    }
  }

  columnHeight_ = 0;
  for (const auto& row : rows_) columnHeight_ += row.gapBefore + row.height;
  layoutColumn();
  if (headline != announced_) {
    announced_ = headline;
    help::announce(headline);
  }
}

// Layout
void SignInScreen::paint(juce::Graphics& g) {
  g.fillAll(theme::kBlack);
  for (const auto& t : texts_)
    t.flow->draw(g, t.box.getTopLeft(), t.colour, 0, false, juce::Justification::horizontallyCentred);
}

void SignInScreen::resized() {
  const int w = getWidth();
  const int colW = std::min(design::kWidth - 2 * kPadX, w);
  back_.setTopLeftPosition((w - colW) / 2, kPadTop);
  layoutColumn();
}

// The rows stack centred in the space under the ← row (the mockup centres
// the dots and copy on the page).
void SignInScreen::layoutColumn() {
  const int top = kPadTop + BackLink::kHeight;
  const int cx = getWidth() / 2;
  int y = std::max(top + kGap, top + (getHeight() - top - columnHeight_) / 2);
  for (auto& row : rows_) {
    y += row.gapBefore;
    if (row.component != nullptr) {
      row.component->setTopLeftPosition(cx - row.component->getWidth() / 2,
                                        y + (row.height - row.component->getHeight()) / 2);
    } else if (row.text >= 0) {
      auto& t = texts_[static_cast<size_t>(row.text)];
      t.box = {static_cast<float>(cx - t.maxWidth / 2), static_cast<float>(y), static_cast<float>(t.maxWidth),
               t.flow->height()};
    } else {
      // Widths may have moved (Copy link ↔ Copied): re-measure the row.
      int w = 0;
      for (auto* b : row.buttons) w += (w > 0 ? kButtonGap : 0) + b->getWidth();
      int x = cx - w / 2;
      for (auto* b : row.buttons) {
        b->setTopLeftPosition(x, y + (row.height - b->getHeight()) / 2);
        x += b->getWidth() + kButtonGap;
      }
    }
    y += row.height;
  }
  repaint();
}

}  // namespace t3k::ui
