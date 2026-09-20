#include "UiPrefs.h"

namespace t3k::ui {

UiPrefs::UiPrefs(juce::PropertiesFile* file) : file_(file) {}

UiPrefs::~UiPrefs() {
  if (file_ != nullptr) file_->saveIfNeeded();
}

juce::String UiPrefs::get(const juce::String& key, const juce::String& fallback) const {
  if (file_ != nullptr)
    return file_->containsKey(key) ? file_->getValue(key) : fallback;
  auto it = memory_.find(key);
  return it == memory_.end() ? fallback : it->second;
}

bool UiPrefs::getBool(const juce::String& key, bool fallback) const {
  const auto v = get(key);
  if (v == "true") return true;
  if (v == "false") return false;
  return fallback;
}

juce::var UiPrefs::getJson(const juce::String& key) const {
  const auto raw = get(key);
  return raw.isEmpty() ? juce::var() : juce::JSON::parse(raw);
}

void UiPrefs::set(const juce::String& key, const juce::String& value) {
  if (get(key) == value && (file_ != nullptr ? file_->containsKey(key) : memory_.count(key) > 0))
    return;
  if (file_ != nullptr)
    file_->setValue(key, value);
  else
    memory_[key] = value;
  listeners.call([&](Listener& l) { l.prefChanged(key); });
}

void UiPrefs::setJson(const juce::String& key, const juce::var& value) {
  set(key, juce::JSON::toString(value, true));
}

void UiPrefs::remove(const juce::String& key) {
  if (file_ != nullptr) {
    if (!file_->containsKey(key)) return;
    file_->removeValue(key);
  } else {
    if (memory_.erase(key) == 0) return;
  }
  listeners.call([&](Listener& l) { l.prefChanged(key); });
}

}  // namespace t3k::ui
