// The plugin's editor when built with T3K_NATIVE_UI: owns the backend, the
// services and the PluginRoot, and manages the window box: the 1024 x
// 578(+chrome) design box times an aspect-locked user scale between 1x and
// kMaxScale, persisted on the processor, grown by the chrome strips' height
// on request.
//
// The root is laid out in design space and scaled with one AffineTransform:
// top-anchored and horizontally centred in whatever box the host actually
// gives us, so a refused resize letterboxes instead of squishing.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DarkLookAndFeel.h"
#include "backend/ProcessorBackend.h"
#include "core/Design.h"
#include "services/HttpClient.h"
#include "services/Services.h"
#include "services/Tone3000Session.h"
#include "views/PluginRoot.h"

namespace t3k::ui {

class NativeEditor : public juce::AudioProcessorEditor, public Shell, private juce::Timer {
public:
  explicit NativeEditor(TONE3000Processor& owner);
  ~NativeEditor() override;

  void setExtraContentHeight(int total, int persistent) override;

  void paint(juce::Graphics& g) override;
  void resized() override;
  void parentHierarchyChanged() override;
  bool keyPressed(const juce::KeyPress& key) override;

private:
  static constexpr int kMaxExtraHeight = 160;  // banner (~44) + hint bar (36) + headroom
  // How long a taller design box keeps the current scale while the host
  // resize is pending, before shrinking to fit.
  static constexpr int kShrinkGraceMs = 450;

  int designHeight() const { return design::kHeight + extraContentHeight_; }
  double currentScale() const { return getWidth() / static_cast<double>(design::kWidth); }
  double maxStartScale() const;
  void applyScaledSize(double scale);
  void updateResizeConstraints();
  void fitRoot();
  void timerCallback() override { fitRoot(); }

  // The per-machine preferences file. Every plugin instance in the process
  // reads and writes the same file, and PropertiesFile saves its whole
  // in-memory copy, so two instances each holding their own would clobber
  // each other's writes: one shared instance per process.
  struct PrefsFile {
    juce::PropertiesFile file{TONE3000Processor::uiPreferencesOptions()};
  };

  TONE3000Processor& processor_;
  // One shared dark theme for JUCE-drawn surfaces (standalone dialogs).
  juce::SharedResourcePointer<DarkLookAndFeel> darkLookAndFeel_;
  juce::SharedResourcePointer<PrefsFile> prefsFile_;
  // Declared before the root: PluginRoot reports its chrome height from its
  // constructor, which lands in setExtraContentHeight.
  int extraContentHeight_ = 0;
  // Guards resized() against persisting a size we didn't choose (see
  // parentHierarchyChanged).
  bool restoringSize_ = false;
  juce::int64 shrinkAllowedAtMs_ = 0;
  ProcessorBackend backend_;
  // Owned here rather than in Services: the session persists its tokens in
  // the prefs, and Services needs the session.
  UiPrefs prefs_;
  HttpClient http_;
  Tone3000Session session_;
  Services services_;
  PluginRoot root_;
};

}  // namespace t3k::ui
