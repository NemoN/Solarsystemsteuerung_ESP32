#include "TemperatureHistory.h"
#include <string.h>

void TemperatureHistory::attach(Sample* storage, size_t capacity, void (*progress)()) {
  buffer_ = storage; capacity_ = storage ? capacity : 0;
  count_ = 0; next_ = 0; first_ = 0; progress_ = progress;
}

const TemperatureHistory::Sample* TemperatureHistory::at(size_t index) const {
  if (!available() || index >= count_) return nullptr;
  const size_t oldest = (next_ + capacity_ - count_) % capacity_;
  return &buffer_[(oldest + index) % capacity_];
}

void TemperatureHistory::append(const Sample& sample) {
  buffer_[next_] = sample;
  next_ = (next_ + 1) % capacity_;
  if (count_ == capacity_) ++first_; else ++count_;
}

void TemperatureHistory::appendMissing(size_t count) {
  // 0x7f7f is the invalid sentinel: fill whole runs without per-sample work.
  while (count) {
    size_t chunk = capacity_ - next_;
    if (chunk > count) chunk = count;
    if (chunk > 2048) chunk = 2048;
    memset(&buffer_[next_], 0x7f, chunk * sizeof(Sample));
    const size_t displaced = count_ + chunk > capacity_ ? count_ + chunk - capacity_ : 0;
    first_ += uint32_t(displaced);
    count_ = count_ + chunk > capacity_ ? capacity_ : count_ + chunk;
    next_ = (next_ + chunk) % capacity_;
    count -= chunk;
    if (count && progress_) progress_();
  }
}

bool TemperatureHistory::record(uint32_t minute, const Sample& sample) {
  if (!available()) return false;
  if (!count_) { first_ = minute; append(sample); return true; }
  const uint32_t last = lastMinute();
  if (minute <= last) return false; // Duplicate minute or clock moved backwards.
  const uint32_t delta = minute - last;
  if (delta >= capacity_) { count_ = 0; next_ = 0; first_ = minute; }
  else if (delta > 1) appendMissing(delta - 1);
  append(sample);
  return true;
}
