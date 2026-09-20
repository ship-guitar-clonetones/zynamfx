// Native UI testbed. Renders PluginRoot over the fixture-driven MockBackend:
//
//   UiTestbed --capture <outDir> [--ref <refDir>] [filter...] PNGs (+ diff table)
//   UiTestbed --compare <ref.png> <candidate.png> [diff.png]
//   UiTestbed --selftest                                     unit tests (pure logic)
//   UiTestbed [--scenario <id>]                              interactive window
//
// Captures are 2x like the React suite (Playwright deviceScaleFactor 2), so a
// reference in ui/local/ui-states/img and a capture here diff pixel for pixel.

#include <juce_gui_extra/juce_gui_extra.h>

#include "Compare.h"
#include "MockBackend.h"
#include "MockSession.h"
#include "Scenarios.h"
#include "SelfTests.h"
#include "core/Design.h"
#include "views/PluginRoot.h"

namespace t3k::ui::testbed {

namespace {

juce::File fixturesDir() {
  // Baked at configure time so the binary finds the fixtures from any cwd.
  return juce::File(T3K_TESTBED_FIXTURES);
}

// What the React suite seeded into localStorage / sessionStorage before load:
// the scenario's own entries, the auth tokens + cached user when `auth`, and
// the hints toggle.
void seedPrefs(UiPrefs& prefs, const Scenario& scenario, const juce::var& fixtures) {
  auto seed = [](const juce::var& obj, auto&& put) {
    if (auto* props = obj.getDynamicObject())
      for (const auto& [name, value] : props->getProperties()) put(name.toString(), value.toString());
  };
  seed(scenario.data["localStorage"], [&](auto k, auto v) { prefs.set(k, v); });
  seed(scenario.data["sessionStorage"], [&](auto k, auto v) { prefs.session[k] = v; });
  if (static_cast<bool>(scenario.data.getProperty("auth", false))) {
    auto* tokens = new juce::DynamicObject();
    tokens->setProperty("access_token", "mock-access-token");
    tokens->setProperty("refresh_token", "mock-refresh-token");
    tokens->setProperty("expires_at", juce::Time::currentTimeMillis() + 12 * 3600 * 1000);
    prefs.setJson(UiPrefs::kTokens, juce::var(tokens));
    prefs.setJson(UiPrefs::kCachedUser, fixtures["user"]);
  }
  if (!scenario.hints()) prefs.setBool(UiPrefs::kShowHints, false);
}

// The suite's fixtureSvg for keys without a real photo: a deterministic
// two-stop gradient with a translucent disc, keyed by a hash of the name.
// (The SVG also drew the key as a label; at avatar size it is invisible and
// is left out.)
juce::Image placeholderImage(const juce::String& key) {
  juce::uint32 hash = 7;
  for (const auto c : key.toStdString()) hash = hash * 31u + static_cast<juce::uint32>(static_cast<unsigned char>(c));
  const float hue = static_cast<float>(hash % 360) / 360.0f;
  const float hue2 = static_cast<float>((hash % 360 + 40) % 360) / 360.0f;
  const bool avatar = key.startsWith("avatar");
  auto hsl = [](float h, float sat, float light) {
    return juce::Colour::fromHSL(h, sat, light, 1.0f);
  };
  constexpr int kSize = 400;
  juce::Image image(juce::Image::ARGB, kSize, kSize, true);
  juce::Graphics g(image);
  g.setGradientFill(juce::ColourGradient(hsl(hue, 0.45f, avatar ? 0.45f : 0.26f), 0, 0,
                                         hsl(hue2, 0.55f, avatar ? 0.30f : 0.12f), kSize, kSize, false));
  g.fillAll();
  g.setColour(hsl(hue2, 0.50f, 0.35f).withAlpha(0.35f));
  const float cx = 120.0f + static_cast<float>(hash % 160), cy = 90.0f + static_cast<float>(hash % 120);
  g.fillEllipse(cx - 90, cy - 90, 180, 180);
  // The key as a bold 34px label, baseline at y = 212.
  const auto label = key.replaceCharacters("-_", "  ").toUpperCase();
  g.setColour(juce::Colours::white.withAlpha(0.82f));
  g.setFont(juce::Font(juce::FontOptions("Helvetica", 34.0f, juce::Font::bold)));
  g.drawSingleLineText(label, kSize / 2, 212, juce::Justification::horizontallyCentred);
  return image;
}

// Fits the root into whatever bounds the shell has, exactly like NativeEditor.
class ScaledHost : public juce::Component, public Shell {
public:
  ScaledHost(Backend& backend, const Scenario& scenario, const juce::var& fixtures)
      : session(scenario.data, fixtures), services(backend, session, *this, prefs, /*updateNotice=*/true) {
    seedPrefs(prefs, scenario, fixtures);
    // The React suite served the fixture image host from its assets folder
    // (real gear photos, copied to fixtures/img) and aborted every fetch
    // under `imagesOffline`.
    services.images.offline = static_cast<bool>(scenario.data.getProperty("imagesOffline", false));
    services.images.localOverride =
        [host = fixtures["imgHost"].toString()](const juce::String& url) -> std::optional<juce::Image> {
      if (host.isEmpty() || !url.startsWith(host)) return std::nullopt;
      const auto key = url.fromLastOccurrenceOf("/", false, false);
      const auto file = fixturesDir().getChildFile("img").getChildFile(key + ".jpg");
      if (file.existsAsFile()) return juce::ImageFileFormat::loadFrom(file);
      return placeholderImage(key);
    };
    root = std::make_unique<PluginRoot>(services);
    addAndMakeVisible(*root);
    setVisible(true);  // offscreen captures have no window to make us visible
    setSize(design::kWidth, root->designHeight());
  }

  PluginRoot& pluginRoot() { return *root; }

  void setExtraContentHeight(int total, int) override {
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>()) {
      const double scale = getWidth() / static_cast<double>(design::kWidth);
      window->setContentComponentSize(juce::roundToInt(design::kWidth * scale),
                                      juce::roundToInt((design::kHeight + total) * scale));
    } else {
      setSize(design::kWidth, design::kHeight + total);  // offscreen capture: 1:1
    }
  }

  void resized() override {
    if (root == nullptr) return;
    const double scale = juce::jmax(0.05, juce::jmin(getWidth() / double(design::kWidth),
                                                     getHeight() / double(root->designHeight())));
    root->setTransform(juce::AffineTransform::scale(static_cast<float>(scale)));
    root->setTopLeftPosition(juce::roundToInt((getWidth() - design::kWidth * scale) / 2), 0);
  }

  void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

private:
  UiPrefs prefs;
  MockSession session;
  Services services;
  std::unique_ptr<PluginRoot> root;
};

class TestbedWindow : public juce::DocumentWindow {
public:
  TestbedWindow(const Scenario& scenario, const juce::var& fixtures)
      : DocumentWindow("TONE3000 UI Testbed", juce::Colours::black, allButtons),
        backend(std::make_unique<MockBackend>(scenario.data)),
        host(*backend, scenario, fixtures) {
    setUsingNativeTitleBar(true);
    setContentNonOwned(&host, true);
    setResizable(true, false);
    getConstrainer()->setFixedAspectRatio(design::kWidth / double(host.getHeight()));
    centreWithSize(host.getWidth(), host.getHeight());
    setVisible(true);
  }
  void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
  std::unique_ptr<MockBackend> backend;
  ScaledHost host;
};

// Renders one scenario offscreen and returns the 2x snapshot.
juce::Image captureScenario(const Scenario& scenario, const juce::var& fixtures, bool& driveMissing) {
  MockBackend backend(scenario.data);
  ScaledHost host(backend, scenario, fixtures);
  auto& root = host.pluginRoot();

  auto* mm = juce::MessageManager::getInstance();
  mm->runDispatchLoopUntil(500);  // the React suite's post-load wait
  if (const auto* drive = driveFor(scenario.id)) {
    (*drive)(root, backend);
  } else if (scenario.hasDrive) {
    driveMissing = true;
  }
  mm->runDispatchLoopUntil(scenario.settleMs());
  return root.createComponentSnapshot(root.getLocalBounds(), true, 2.0f);
}

int runCapture(const juce::StringArray& args) {
  const juce::File outDir(juce::File::getCurrentWorkingDirectory().getChildFile(args[1]));
  outDir.createDirectory();
  juce::File refDir;
  juce::StringArray filters;  // substrings; a scenario runs if it matches any
  for (int i = 2; i < args.size(); ++i) {
    if (args[i] == "--ref" && i + 1 < args.size())
      refDir = juce::File::getCurrentWorkingDirectory().getChildFile(args[++i]);
    else
      filters.add(args[i]);
  }
  const auto selected = [&](const juce::String& id) {
    if (filters.isEmpty()) return true;
    for (const auto& f : filters)
      if (id.contains(f)) return true;
    return false;
  };

  const auto fixtures = Fixtures::load(fixturesDir().getChildFile("scenarios.json"));
  juce::PNGImageFormat png;
  int captured = 0;
  for (const auto& scenario : fixtures.scenarios) {
    if (!selected(scenario.id)) continue;
    if (webviewOnly(scenario.id)) {
      std::cout << scenario.id.paddedRight(' ', 34) << " (webview-only, skipped)" << std::endl;
      continue;
    }
    bool driveMissing = false;
    const auto image = captureScenario(scenario, fixtures.root, driveMissing);
    const auto file = outDir.getChildFile(scenario.id + ".png");
    file.deleteFile();
    if (juce::FileOutputStream out(file); out.openedOk())
      png.writeImageToStream(image, out);
    ++captured;

    juce::String line = scenario.id.paddedRight(' ', 34);
    if (driveMissing)
      line += " (drive not mirrored)";
    if (refDir != juce::File()) {
      const auto ref = refDir.getChildFile(scenario.id + ".png");
      const auto result = comparePngFiles(ref, file, outDir.getChildFile(scenario.id + ".diff.png"));
      line += result.ok ? juce::String::formatted(" %6.2f%% mismatch, worst tile %5.1f%% at %d,%d",
                                                  result.mismatchPercent(), result.worstTilePercent,
                                                  result.worstTile.x, result.worstTile.y)
                        : " " + result.error;
    }
    std::cout << line << std::endl;
  }
  std::cout << captured << " scenario(s) captured to " << outDir.getFullPathName() << std::endl;
  return 0;
}

int runCompare(const juce::StringArray& args) {
  const auto cwd = juce::File::getCurrentWorkingDirectory();
  const auto result = comparePngFiles(cwd.getChildFile(args[1]), cwd.getChildFile(args[2]),
                                      args.size() > 3 ? cwd.getChildFile(args[3]) : juce::File());
  if (!result.ok) {
    std::cerr << result.error << std::endl;
    return 2;
  }
  std::cout << juce::String::formatted("%lld px mismatched of %dx%d (%.3f%%), worst tile %.1f%% at %d,%d",
                                       static_cast<long long>(result.mismatched), result.width,
                                       result.height, result.mismatchPercent(), result.worstTilePercent,
                                       result.worstTile.x, result.worstTile.y)
            << std::endl;
  return 0;
}

}  // namespace

class TestbedApp : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override { return "UiTestbed"; }
  const juce::String getApplicationVersion() override { return "1.0"; }
  bool moreThanOneInstanceAllowed() override { return true; }

  void initialise(const juce::String& commandLine) override {
    const auto args = juce::StringArray::fromTokens(commandLine, true);
    if (args.size() >= 2 && args[0] == "--capture") {
      setApplicationReturnValue(runCapture(args));
      quit();
      return;
    }
    if (args.size() >= 3 && args[0] == "--compare") {
      setApplicationReturnValue(runCompare(args));
      quit();
      return;
    }
    if (args.size() >= 1 && args[0] == "--selftest") {
      setApplicationReturnValue(runSelfTests());
      quit();
      return;
    }
    const auto fixtures = Fixtures::load(fixturesDir().getChildFile("scenarios.json"));
    juce::String id = "main-mono";
    for (int i = 0; i + 1 < args.size(); ++i)
      if (args[i] == "--scenario")
        id = args[i + 1];
    const auto* scenario = fixtures.find(id);
    if (scenario == nullptr) {
      std::cerr << "unknown scenario: " << id << std::endl;
      setApplicationReturnValue(1);
      quit();
      return;
    }
    window = std::make_unique<TestbedWindow>(*scenario, fixtures.root);
  }

  void shutdown() override { window.reset(); }

private:
  std::unique_ptr<TestbedWindow> window;
};

}  // namespace t3k::ui::testbed

START_JUCE_APPLICATION(t3k::ui::testbed::TestbedApp)
