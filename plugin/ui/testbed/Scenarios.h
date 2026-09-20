// The scenario table: fixtures/scenarios.json (exported from the React
// screenshot suite) plus the hand-mirrored drive steps that the suite ran
// through Playwright before each shot.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace t3k::ui {
class PluginRoot;
}

namespace t3k::ui::testbed {

class MockBackend;

struct Scenario {
  juce::String id;
  juce::var data;  // the exported scenario entry (chain, device, auth, api, …)
  bool hasDrive = false;

  bool hints() const { return static_cast<bool>(data.getProperty("hints", true)); }
  bool banner() const { return static_cast<bool>(data.getProperty("banner", false)); }
  int settleMs() const { return static_cast<int>(data.getProperty("settle", 400)); }
};

struct Fixtures {
  juce::var root;  // whole scenarios.json
  std::vector<Scenario> scenarios;

  static Fixtures load(const juce::File& scenariosJson);
  const Scenario* find(const juce::String& id) const;
};

// A drive step runs against the live root + mock after the scenario has
// settled, then the capture waits `settle` again. Returns false when the
// scenario has a drive in the suite that is not mirrored yet.
using Drive = std::function<void(PluginRoot&, MockBackend&)>;
const Drive* driveFor(const juce::String& scenarioId);

// Scenarios that only exist in the webview (the React error boundary): the
// native UI has no equivalent state to capture.
bool webviewOnly(const juce::String& scenarioId);

}  // namespace t3k::ui::testbed
