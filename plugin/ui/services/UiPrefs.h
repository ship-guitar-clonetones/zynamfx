// Per-machine UI preferences and per-editor session values (port of
// uiPreferences.ts and the sessionStorage uses in helpText.ts /
// useToneSession.ts): `persistent` is a PropertiesFile in the plugin's
// app-data folder, `session` is plain memory that lives as long as the
// editor. Keys keep their web names so a reader of either UI finds them.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <map>

namespace t3k::ui {

class UiPrefs {
public:
  struct Listener {
    virtual ~Listener() = default;
    virtual void prefChanged(const juce::String& key) = 0;
  };

  // Persistent values live in `file`, which the caller owns and may share
  // between editors (nullptr = memory only, as the testbed uses).
  explicit UiPrefs(juce::PropertiesFile* file = nullptr);
  ~UiPrefs();

  juce::String get(const juce::String& key, const juce::String& fallback = {}) const;
  bool getBool(const juce::String& key, bool fallback) const;
  juce::var getJson(const juce::String& key) const;
  void set(const juce::String& key, const juce::String& value);
  void setBool(const juce::String& key, bool value) { set(key, value ? "true" : "false"); }
  void setJson(const juce::String& key, const juce::var& value);
  void remove(const juce::String& key);

  // Editor-lifetime values.
  std::map<juce::String, juce::String> session;

  void addListener(Listener* l) { listeners.add(l); }
  void removeListener(Listener* l) { listeners.remove(l); }

  // Keys shared by more than one component.
  static constexpr const char* kShowHints = "t3k.showHints";
  static constexpr const char* kShowBlockNormalizeControl = "t3k.showBlockNormalizeControl";
  static constexpr const char* kShowBlockSizeControl = "t3k.showBlockSizeControl";
  static constexpr const char* kShowPresetPcNumbers = "t3k.showPresetPcNumbers";
  static constexpr const char* kTokens = "t3k_tokens";
  static constexpr const char* kCachedUser = "t3k.cachedUser";
  static constexpr const char* kUpdateNotice = "t3k.updateNotice";
  static constexpr const char* kDismissedBanners = "t3k.dismissedBanners";
  // Session keys.
  static constexpr const char* kDetailBlockId = "t3k.detailBlockId";
  static constexpr const char* kChainScroll = "t3k.chainScroll";

private:
  juce::PropertiesFile* file_;
  std::map<juce::String, juce::String> memory_;
  juce::ListenerList<Listener> listeners;
};

}  // namespace t3k::ui
