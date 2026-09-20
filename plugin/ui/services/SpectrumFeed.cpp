#include "SpectrumFeed.h"

namespace t3k::ui {

SpectrumFeed::SpectrumFeed(Backend& backend, std::string blockId)
    : backend_(backend), blockId_(std::move(blockId)) {
  backend_.setBlockSpectrumEnabled(blockId_, true);
  timerCallback();
  startTimer(kPollMs);
}

SpectrumFeed::~SpectrumFeed() {
  stopTimer();
  backend_.setBlockSpectrumEnabled(blockId_, false);
}

void SpectrumFeed::timerCallback() {
  const auto res = backend_.getBlockSpectrum(blockId_);
  const auto* arr = res.getArray();
  if (arr == nullptr) return;
  scratch_.clear();
  for (const auto& v : *arr) scratch_.push_back(static_cast<float>(static_cast<double>(v)));
  if (scratch_ == bins_) return;
  std::swap(bins_, scratch_);
  if (onChange) onChange();
}

}  // namespace t3k::ui
