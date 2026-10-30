#pragma once
#include <stddef.h>
#include <stdint.h>

// The caller owns the storage. Every slot is one UTC minute, five readings in
// tenths of a degree; an explicit sentinel represents missing/invalid data.
class TemperatureHistory {
 public:
  static constexpr size_t SENSOR_COUNT = 5;
  static constexpr int16_t MISSING = 0x7f7f;
  struct Sample { int16_t values[SENSOR_COUNT]; };

  void attach(Sample* storage, size_t capacity, void (*progress)() = nullptr);
  bool available() const { return buffer_ != nullptr && capacity_ != 0; }
  size_t size() const { return count_; }
  size_t capacity() const { return capacity_; }
  uint32_t firstMinute() const { return first_; }
  uint32_t lastMinute() const { return count_ ? first_ + uint32_t(count_ - 1) : 0; }
  const Sample* at(size_t chronologicalIndex) const;
  bool record(uint32_t minute, const Sample& sample);

 private:
  Sample* buffer_ = nullptr;
  size_t capacity_ = 0, count_ = 0, next_ = 0;
  uint32_t first_ = 0;
  void (*progress_)() = nullptr;
  void appendMissing(size_t count);
  void append(const Sample& sample);
};
static_assert(sizeof(TemperatureHistory::Sample) == 10, "History sample must be 10 bytes");
