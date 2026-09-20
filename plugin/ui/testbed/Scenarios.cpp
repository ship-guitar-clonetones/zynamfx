#include "Scenarios.h"


#include <map>

#include "Drive.h"
#include "views/PluginRoot.h"
#include "widgets/DbMeter.h"

namespace t3k::ui::testbed {

Fixtures Fixtures::load(const juce::File& scenariosJson) {
  Fixtures f;
  f.root = juce::JSON::parse(scenariosJson);
  if (const auto* arr = f.root["scenarios"].getArray()) {
    for (const auto& entry : *arr) {
      Scenario s;
      s.id = entry["id"].toString();
      s.data = entry;
      s.hasDrive = static_cast<bool>(entry.getProperty("hasDrive", false));
      f.scenarios.push_back(std::move(s));
    }
  }
  return f;
}

const Scenario* Fixtures::find(const juce::String& id) const {
  for (const auto& s : scenarios)
    if (s.id == id)
      return &s;
  return nullptr;
}

namespace {

// Mirrors of the Playwright `drive` steps in scenarios.mjs, keyed by id.
// Filled in phase by phase as the screens they touch are ported.
const std::map<juce::String, Drive>& drives() {
  using namespace drive;
  static const std::map<juce::String, Drive> table = {
      {"chrome-account-signed-out",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Account:");
         wait(200);
       }},
      {"chrome-account-signed-in",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Account:");
         wait(200);
       }},
      {"chrome-preset-save",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Save Preset");
         fill(root, "Name", "Stadium Lead");
         wait(200);
       }},
      {"chrome-preset-browse",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Presets:");
         wait(300);
       }},
      {"chrome-auto-balance",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Auto Balance");
         wait(100);
       }},
      {"chrome-input-mode",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Input Mode");
         wait(200);
       }},
      {"main-meters-hot",
       [](PluginRoot& root, MockBackend&) {
         wait(150);  // a meter tick, so the latch is set
         auto* meter = dynamic_cast<DbMeter*>(
             find(root, [](juce::Component& c) { return c.getName() == "input meter"; }));
         if (meter != nullptr) hoverAt(root, *meter, meter->clipDotCentre(0));
         wait(100);
       }},
      {"chrome-spread-deck",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Offset:", /*right=*/true);
         wait(200);
       }},
      {"chrome-align-deck",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Align:", /*right=*/true);
         wait(200);
       }},
      {"chrome-tile-menu",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "'02 Vox AC30/6 Top Boost.", /*right=*/true);
         wait(200);
       }},
      // The detail card opens from the seeded t3k.detailBlockId; the suite's
      // tile click (the web dropped the seed before the chain arrived) only
      // re-opens it, leaving no hover behind once the gallery is gone.
      {"main-detail", [](PluginRoot&, MockBackend&) { wait(300); }},
      {"load-detail-loading", [](PluginRoot&, MockBackend&) { wait(300); }},
      {"load-detail-failed", [](PluginRoot&, MockBackend&) { wait(300); }},
      {"main-detail-info",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Info:");
         wait(400);
       }},
      {"main-detail-info-signed-out",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Info:");
         wait(400);
       }},
      {"main-detail-eq-sliders",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "EQ:");
         wait(300);
       }},
      {"main-detail-eq-curve",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "EQ:");
         wait(200);
         clickByHelp(root, "Curve:");
         wait(400);
       }},
      {"main-detail-model-select",
       [](PluginRoot& root, MockBackend&) {
         wait(200);  // the catalog list lands
         if (auto* trigger = find(root, [](juce::Component& c) { return c.getName() == "model select"; }))
           click(root, *trigger);
         wait(400);
       }},
      {"main-tuner-idle", [](PluginRoot& root, MockBackend&) { clickByHelp(root, "Tuner:"); wait(200); }},
      {"main-tuner-intune", [](PluginRoot& root, MockBackend&) { clickByHelp(root, "Tuner:"); wait(400); }},
      {"main-tuner-flat", [](PluginRoot& root, MockBackend&) { clickByHelp(root, "Tuner:"); wait(400); }},
      {"main-tuner-sharp", [](PluginRoot& root, MockBackend&) { clickByHelp(root, "Tuner:"); wait(400); }},
      {"main-tuner-accidental", [](PluginRoot& root, MockBackend&) { clickByHelp(root, "Tuner:"); wait(400); }},
      {"load-offline-modal",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Add Tone");
         wait(300);  // the modal is rebuilt on the next loop turn
         unhover(root);
       }},
      {"load-update-notice", [](PluginRoot&, MockBackend&) { wait(300); }},
      {"load-oauth-leaving",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Account:");
         wait(100);
         if (auto* login = buttonNamed(root, "Login")) click(root, *login);
         wait(300);
         unhover(root);
       }},
      {"browser-picking",
       [](PluginRoot& root, MockBackend&) {
         if (auto* card = buttonNamed(root, "'02 Vox AC30/6 Top Boost")) click(root, *card);
         wait(600);
         unhover(root);
       }},
      {"chrome-toast-saved",
       [](PluginRoot& root, MockBackend&) {
         clickByHelp(root, "Save Preset");
         fill(root, "Name", "Stadium Lead");
         if (auto* save = buttonNamed(root, "Save")) click(root, *save);
         wait(100);
       }},

      // Settings
      {"settings-plugin", [](PluginRoot& root, MockBackend&) { openSettings(root); }},
      {"settings-advanced",
       [](PluginRoot& root, MockBackend&) {
         openSettings(root);
         scrollSettingsTo(root, "Calibration");
       }},
      {"settings-midi-empty",
       [](PluginRoot& root, MockBackend&) {
         openSettings(root);
         scrollSettingsTo(root, "MIDI Mapping");
       }},
      {"settings-midi-learning",
       [](PluginRoot& root, MockBackend&) {
         openSettings(root);
         scrollSettingsTo(root, "MIDI Mapping");
       }},
      {"settings-midi-picker",
       [](PluginRoot& root, MockBackend&) {
         openSettings(root);
         scrollSettingsTo(root, "MIDI Mapping");
         if (auto* picker = find(root, [](juce::Component& c) { return c.getName() == "Control to map"; }))
           click(root, *picker);
         wait(400);
       }},
      {"settings-system", [](PluginRoot& root, MockBackend&) { openSystemSettings(root); }},
      {"settings-system-mic-denied", [](PluginRoot& root, MockBackend&) { openSystemSettings(root); }},
      {"settings-system-feedback-risk", [](PluginRoot& root, MockBackend&) { openSystemSettings(root); }},
      {"settings-system-input-muted", [](PluginRoot& root, MockBackend&) { openSystemSettings(root); }},
      {"settings-system-rate96",
       [](PluginRoot& root, MockBackend&) {
         openSystemSettings(root);
         if (auto* settings = root.settings()) settings->scrollToHeading("MIDI Inputs", /*centre=*/true);
         wait(300);
       }},
  };
  return table;
}

}  // namespace

bool webviewOnly(const juce::String& scenarioId) { return scenarioId == "load-crash"; }

const Drive* driveFor(const juce::String& scenarioId) {
  const auto& table = drives();
  const auto it = table.find(scenarioId);
  return it == table.end() ? nullptr : &it->second;
}

}  // namespace t3k::ui::testbed
