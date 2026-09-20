// Async artwork loader: URL → decoded juce::Image, fetched off the message
// thread and cached (the browser's image cache, minus the disk; the oldest
// entries go once kMaxCached is reached, and a consumer's own copy of an
// Image keeps it alive regardless). Consumers ask for a URL and get called
// back on the message thread when it resolves; a failed fetch resolves to a
// null image so the caller can fall back to a glyph (ToneImage.tsx onError).
#pragma once

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace t3k::ui {

class ImageLoader : private juce::Thread {
public:
  ImageLoader();
  ~ImageLoader() override;

  // Result for a URL, if already known (null Image = load failed).
  std::optional<juce::Image> cached(const juce::String& url) const;

  // Handle for a pending request; destroying it drops the callback.
  class Request {
  public:
    Request() = default;
    ~Request() { cancel(); }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    void cancel();

  private:
    friend class ImageLoader;
    ImageLoader* loader_ = nullptr;
    juce::uint64 id_ = 0;
  };

  // Resolve `url`; `onDone` runs on the message thread (synchronously when
  // the result is already cached).
  void load(const juce::String& url, Request& request,
            std::function<void(const juce::Image&)> onDone);

  // Testbed hooks: answer a URL locally instead of fetching (a fixture file,
  // or the suite's synthesized placeholder), and simulate the network being
  // down (fixtures' `imagesOffline`). Runs on the loader thread.
  std::function<std::optional<juce::Image>(const juce::String& url)> localOverride;
  bool offline = false;

private:
  struct Pending {
    juce::uint64 id;
    juce::String url;
    std::function<void(const juce::Image&)> onDone;
  };

  void run() override;
  juce::Image fetch(const juce::String& url);
  void deliver(const juce::String& url, juce::Image image);

  static constexpr size_t kMaxCached = 128;

  juce::CriticalSection lock_;
  std::map<juce::String, juce::Image> cache_;  // null Image = failed
  std::vector<juce::String> cacheOrder_;       // insertion order, for eviction
  std::vector<juce::String> queue_;
  std::vector<Pending> pending_;
  juce::uint64 nextId_ = 1;

  JUCE_DECLARE_WEAK_REFERENCEABLE(ImageLoader)
};

}  // namespace t3k::ui
