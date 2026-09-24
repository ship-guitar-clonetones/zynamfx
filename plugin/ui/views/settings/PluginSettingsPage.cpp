#include "PluginSettingsPage.h"

#include "InlineChrome.h"
#include "core/Fonts.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/TextField.h"

namespace t3k::ui {

namespace {
// External docs: how to measure your rig's calibration levels.
const char* const kCalibrationDocsUrl =
    "https://neural-amp-modeler.readthedocs.io/en/latest/tutorials/calibration.html";

// Oversampling rate choices: the osFactor parameter's choice indices; the
// DSP maps index i to 2^(i+1).
const std::vector<SelectField::Option> kOsFactorOptions = {
    {"0", "2X - Default", {}}, {"1", "4X", {}}, {"2", "8X", {}}};

RichText copy(std::initializer_list<TextRun> runs) { return RichText(runs); }
}  // namespace

// DbuField
// The calibration level: an outlined number field with a bold grey "dBu"
// suffix. Edits are a draft while focused (committing every keystroke would
// reformat the value under the cursor); blur or Enter commits.
class PluginSettingsPage::DbuField : public FormItem {
public:
  static constexpr int kSuffixPad = 52;
  static constexpr float kSuffixPx = 14;

  explicit DbuField(ParamBinding& param) : param_(param) {
    field_.setBackground(juce::Colours::transparentBlack);
    field_.setBorder(form::kFieldBorder);
    field_.setCornerRadius(form::kFieldRadius);
    field_.setFontSize(form::kFieldPx);
    field_.setPadding(form::kFieldPadY, form::kFieldPadX, kSuffixPad);
    field_.setPlaceholder("Value");
    field_.setName("Calibration level");
    field_.onEnter = [this] { commit(); };
    field_.onBlur = [this] { commit(); };
    addAndMakeVisible(field_);
    sync();
  }

  // Reflect the parameter unless the user is mid-edit.
  void sync() {
    if (field_.hasFocus()) return;
    field_.setText(juce::String(value(), 1));
  }

  float heightFor(float) const override { return static_cast<float>(form::fieldHeight()); }

  void paint(juce::Graphics& g) override {
    const auto font = Fonts::sans(kSuffixPx, true);
    const float lineH = static_cast<float>(Fonts::normalLineHeight(kSuffixPx));
    const float w = Fonts::width(font, "dBu");
    paint::cssLine(g, "dBu", getWidth() - form::kFieldPadX - w, std::round((getHeight() - lineH) / 2), lineH,
                   w + 2, font, theme::kGray);
  }

  void resized() override { field_.setBounds(getLocalBounds()); }

private:
  float value() const { return param_.normalised() * (kDbuMax - kDbuMin) + kDbuMin; }

  void commit() {
    const auto text = field_.text().trim();
    if (text.isNotEmpty() && text.containsOnly("-+.0123456789")) {
      const float normalised = (text.getFloatValue() - kDbuMin) / (kDbuMax - kDbuMin);
      param_.set(juce::jlimit(0.0f, 1.0f, normalised));
    }
    field_.setText(juce::String(value(), 1));
  }

  ParamBinding& param_;
  TextField field_;
};

// PluginSettingsPage
PluginSettingsPage::PluginSettingsPage(Services& services)
    : FormStack(form::kSectionGap),
      services_(services),
      infoBar_("Info Bar", "Strip under the faceplate with hover tips and CPU load."),
      namSize_("NAM A2 Size",
               "Default size for new NAM blocks. Existing blocks keep their own, so presets load as saved."),
      lite_("A2-Lite", "Sounds great and uses less CPU"),
      full_("A2-Full", "Maximum accuracy model"),
      blockSize_("Per-Block NAM Size",
                 copy({TextRun::plain("Adds a "), TextRun::inlineBox(inline_chrome::liteFullChip()),
                       TextRun::plain(" toggle to every NAM block. When off, a block only shows its size if it "
                                      "differs from your default.")})),
      blockSizeTip_(copy({TextRun::plain("Look for "), TextRun::inlineBox(inline_chrome::liteFullChip()),
                          TextRun::plain(" next to each block's power button.")})),
      normalize_("Per-Block Normalization",
                 "Each block has normalization enabled, which levels output for consistent volume across signal "
                 "blocks. Turning this on reveals an optional control that lets you disable normalization per block."),
      normalizeTip_(copy({TextRun::plain("Normalization is now controlled per block. Look for the "),
                          TextRun::inlineBox(inline_chrome::icon(Icon::Equal)),
                          TextRun::plain(" icon on each block, enabled by default.")})),
      calibrateParam_(services.backend, "calibrateInput"),
      dbuParam_(services.backend, "inputCalibrationLevel"),
      osEnabledParam_(services.backend, "osEnabled"),
      osFactorParam_(services.backend, "osFactor"),
      calibration_("Calibration",
                   "Matches your input level to the capture's original recording level, for accurate gain staging."),
      dbu_(std::make_unique<DbuField>(dbuParam_)),
      calibrationHelp_(copy({TextRun::plain("Set the dBu level that matches your DAW's max digital level. Typical "
                                            "values: +12 dBu (professional gear), +4 dBu (semi-pro). "),
                             TextRun::link("Learn More", kCalibrationDocsUrl, theme::kLinkBlue)})),
      calibrationTip_(copy({TextRun::plain("Captures that include calibration data show a "),
                            TextRun::inlineBox(inline_chrome::icon(Icon::Gauge)),
                            TextRun::plain(juce::String::fromUTF8(
                                " icon on their block and it\xe2\x80\x99s enabled by default."))})),
      calibrationHandoff_("When one calibrated NAM feeds another, output calibration data also sets the handoff "
                          "level between them."),
      oversampling_("Oversampling", "Reduces aliasing. Higher rates improve quality but use more CPU."),
      rateLabel_("Rate", form::kBodyPx, false, theme::kMuted),
      multiCore_("Multi-Core Processing",
                 "Spreads the work across CPU cores for more headroom: stereo chains process in parallel, and "
                 "oversampled NAM models split across cores. Doesn't change the sound."),
      midi_("MIDI Mapping",
            "Control the plugin from pedals and knobs. Mappings are saved with the plugin and work in your DAW too."),
      midiSection_(services),
      diagnostics_("Diagnostics", "Copy recent diagnostic logs to the clipboard and paste them into a bug report."),
      copyLogs_("Copy Logs", FormButton::cta()),
      revealLogs_("Reveal log file on disk", FormButton::text(form::kSmallPx, false, theme::kSubtle)),
      copyLogsBox_(copyLogs_, static_cast<float>(copyLogs_.preferredHeight())),
      revealLogsBox_(revealLogs_, static_cast<float>(revealLogs_.preferredHeight())),
      logStatus_(juce::String(), form::kSmallPx),
      update_({}, FormButton::cta()),
      updateBox_(update_, static_cast<float>(update_.preferredHeight())),
      version_(juce::String(), form::kSmallPx, theme::kSubtle) {
  // Info Bar.
  infoBar_.onChange = [this](bool on) { services_.hints.setEnabled(on); };
  add(infoBar_);

  // NAM A2 Size.
  namSize_.setInlineLabel();
  namSize_.content().setGap(form::kControlGap);
  namSize_.content().add(lite_);
  namSize_.content().add(full_);
  lite_.onSelect = [this] { services_.chain.setNamSlimSizeDefault(kSlimSizeLite); };
  full_.onSelect = [this] { services_.chain.setNamSlimSizeDefault(kSlimSizeFull); };
  add(namSize_);

  // Per-block controls.
  blockSize_.content().add(blockSizeTip_);
  blockSize_.onChange = [this](bool on) { services_.prefs.setBool(UiPrefs::kShowBlockSizeControl, on); };
  add(blockSize_);
  normalize_.content().add(normalizeTip_);
  normalize_.onChange = [this](bool on) { services_.prefs.setBool(UiPrefs::kShowBlockNormalizeControl, on); };
  add(normalize_);

  // Calibration.
  calibration_.content().add(*dbu_);
  calibration_.content().add(calibrationHelp_);
  calibration_.content().add(calibrationTip_);
  calibration_.content().add(calibrationHandoff_);
  calibrationHelp_.onLink = [](const juce::String& href) { juce::URL(href).launchInDefaultBrowser(); };
  calibration_.onChange = [this](bool on) { calibrateParam_.set(on); };
  add(calibration_);

  // Oversampling.
  oversampling_.content().setGap(8);
  oversampling_.content().add(rateLabel_);
  oversampling_.content().add(osRate_);
  osRate_.setOptions(kOsFactorOptions);
  osRate_.onChange = [this](const juce::String& v) {
    if (auto* p = services_.backend.parameter("osFactor")) osFactorParam_.set(p->convertTo0to1(v.getFloatValue()));
  };
  oversampling_.onChange = [this](bool on) { osEnabledParam_.set(on); };
  add(oversampling_);

  // Multi-core.
  multiCore_.onChange = [this](bool on) { services_.chain.setMultiCore(on); };
  add(multiCore_);

  // MIDI Mapping.
  midi_.setInlineLabel();
  midi_.content().add(midiSection_);
  add(midi_);

  // Diagnostics.
  diagnostics_.setInlineLabel();
  diagnostics_.content().setGap(form::kControlGap);
  diagnostics_.content().add(copyLogsBox_);
  diagnostics_.content().add(revealLogsBox_);
  diagnostics_.content().add(logStatus_);
  logStatus_.setVisible(false);
  copyLogs_.onClick = [this] {
    showLogStatus(services_.backend.copyLogs() ? "Logs copied to clipboard" : "No log file found yet");
  };
  revealLogs_.onClick = [this] {
    showLogStatus(services_.backend.revealLogs().isNotEmpty() ? "Revealed log file" : "No log file found yet");
  };
  add(diagnostics_);

  // Version / update footer.
  footer_.add(updateBox_);
  footer_.add(version_, form::kControlGap);
  update_.onClick = [this] {
    if (const auto& u = services_.updates.update()) juce::URL(u->url).launchInDefaultBrowser();
  };
  add(footer_);

  for (auto* p : {&calibrateParam_, &dbuParam_, &osEnabledParam_, &osFactorParam_})
    p->onChange = [this] { syncParams(); };
  services_.prefs.addListener(this);
  services_.chain.addListener(this);
  services_.updates.addListener(this);
  syncPrefs();
  syncParams();
  syncFooter();
  chainChanged(services_.chain.state());
}

PluginSettingsPage::~PluginSettingsPage() {
  services_.updates.removeListener(this);
  services_.chain.removeListener(this);
  services_.prefs.removeListener(this);
}

void PluginSettingsPage::prefChanged(const juce::String& key) {
  if (key == UiPrefs::kShowHints || key == UiPrefs::kShowBlockSizeControl ||
      key == UiPrefs::kShowBlockNormalizeControl)
    syncPrefs();
}

void PluginSettingsPage::syncPrefs() {
  infoBar_.setValue(services_.hints.enabled());
  const bool size = services_.prefs.getBool(UiPrefs::kShowBlockSizeControl, false);
  blockSize_.setValue(size);
  blockSize_.setExpanded(size);
  const bool normalize = services_.prefs.getBool(UiPrefs::kShowBlockNormalizeControl, false);
  normalize_.setValue(normalize);
  normalize_.setExpanded(normalize);
}

void PluginSettingsPage::syncParams() {
  const bool calibrate = calibrateParam_.boolValue();
  calibration_.setValue(calibrate);
  calibration_.setExpanded(calibrate);
  dbu_->sync();
  const bool os = osEnabledParam_.boolValue();
  oversampling_.setValue(os);
  oversampling_.setExpanded(os);
  if (auto* p = services_.backend.parameter("osFactor"))
    osRate_.setValue(juce::String(juce::roundToInt(p->convertFrom0to1(osFactorParam_.normalised()))));
}

void PluginSettingsPage::chainChanged(const ChainState& state) {
  const bool full = isSlimSizeFull(state.namSlimSizeDefault);
  lite_.setSelected(!full);
  full_.setSelected(full);
  multiCore_.setValue(state.multiCore);
}

// Version / update sit last so diagnostics stay above the footer.
void PluginSettingsPage::syncFooter() {
  const auto& update = services_.updates.update();
  const auto& version = services_.updates.localVersion();
  if (update) update_.setLabel("Update to v" + update->version);
  footer_.setShown(updateBox_, update.has_value());
  version_.setText("TONE3000 v" + version);
  footer_.setShown(version_, version.isNotEmpty());
  const bool footer = update.has_value() || version.isNotEmpty();
  setShown(footer_, footer);
  // Every section carries SECTION_GAP below it; only the footer doesn't.
  setTrailing(footer ? 0 : form::kSectionGap);
}

void PluginSettingsPage::showLogStatus(const juce::String& status) {
  logStatus_.setText(status);
  diagnostics_.content().setShown(logStatus_, true);
  logStatusClear_.start(kLogStatusMs, [this] { diagnostics_.content().setShown(logStatus_, false); });
}

}  // namespace t3k::ui
