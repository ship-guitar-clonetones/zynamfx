#include "ImageLoader.h"

#include <algorithm>

#include "core/Bitmap.h"

namespace t3k::ui {

namespace {
constexpr int kConnectTimeoutMs = 8000;
// Artwork is shown at most ~300 logical px wide (the detail card), so
// 1024 covers a 3x display; the cache doesn't hold camera-sized originals.
constexpr int kMaxSide = 1024;
}

ImageLoader::ImageLoader() : juce::Thread("t3k image loader") {}

ImageLoader::~ImageLoader() {
  signalThreadShouldExit();
  notify();
  stopThread(2000);
  masterReference.clear();
}

std::optional<juce::Image> ImageLoader::cached(const juce::String& url) const {
  const juce::ScopedLock sl(lock_);
  auto it = cache_.find(url);
  if (it == cache_.end()) return std::nullopt;
  return it->second;
}

void ImageLoader::Request::cancel() {
  if (loader_ == nullptr) return;
  const juce::ScopedLock sl(loader_->lock_);
  auto& pending = loader_->pending_;
  pending.erase(std::remove_if(pending.begin(), pending.end(),
                               [this](const Pending& p) { return p.id == id_; }),
                pending.end());
  loader_ = nullptr;
}

void ImageLoader::load(const juce::String& url, Request& request,
                       std::function<void(const juce::Image&)> onDone) {
  request.cancel();
  if (url.isEmpty()) {
    onDone({});
    return;
  }
  if (auto hit = cached(url)) {
    onDone(*hit);
    return;
  }
  {
    const juce::ScopedLock sl(lock_);
    request.loader_ = this;
    request.id_ = nextId_++;
    pending_.push_back({request.id_, url, std::move(onDone)});
    if (std::find(queue_.begin(), queue_.end(), url) == queue_.end()) queue_.push_back(url);
  }
  if (!isThreadRunning()) startThread();
  notify();
}

void ImageLoader::run() {
  while (!threadShouldExit()) {
    juce::String url;
    {
      const juce::ScopedLock sl(lock_);
      if (!queue_.empty()) {
        url = queue_.front();
        queue_.erase(queue_.begin());
      }
    }
    if (url.isEmpty()) {
      wait(-1);
      continue;
    }
    auto image = fetch(url);
    juce::WeakReference<ImageLoader> self(this);
    juce::MessageManager::callAsync([self, url, image] {
      if (self != nullptr) self->deliver(url, image);
    });
  }
}

juce::Image ImageLoader::fetch(const juce::String& url) {
  if (offline) return {};
  if (localOverride)
    if (auto image = localOverride(url)) return *image;

  juce::URL target(url);
  auto stream = target.createInputStream(
      juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
          .withConnectionTimeoutMs(kConnectTimeoutMs)
          .withProgressCallback([this](int, int) { return !threadShouldExit(); }));
  if (stream == nullptr) return {};
  juce::MemoryBlock bytes;
  stream->readIntoMemoryBlock(bytes);
  return bitmap::fitWithin(juce::ImageFileFormat::loadFrom(bytes.getData(), bytes.getSize()), kMaxSide);
}

// Message thread: cache the result and fire every waiter for this URL, one
// at a time so a callback that destroys another waiter's Request still
// cancels it.
void ImageLoader::deliver(const juce::String& url, juce::Image image) {
  {
    const juce::ScopedLock sl(lock_);
    if (cache_.emplace(url, image).second) {
      cacheOrder_.push_back(url);
      if (cacheOrder_.size() > kMaxCached) {
        cache_.erase(cacheOrder_.front());
        cacheOrder_.erase(cacheOrder_.begin());
      }
    }
  }
  for (;;) {
    std::function<void(const juce::Image&)> onDone;
    {
      const juce::ScopedLock sl(lock_);
      auto it = std::find_if(pending_.begin(), pending_.end(), [&](const Pending& p) { return p.url == url; });
      if (it == pending_.end()) return;
      onDone = std::move(it->onDone);
      pending_.erase(it);
    }
    onDone(image);
  }
}

}  // namespace t3k::ui
