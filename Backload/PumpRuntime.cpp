#include "PumpRuntime.h"
#include <string.h>
#include <limits.h>

namespace {
constexpr uint32_t MAGIC = 0x50525431, VERSION = 1;
void saturatingAdd(uint32_t& value, uint32_t amount) {
  value = amount > UINT32_MAX - value ? UINT32_MAX : value + amount;
}
}

PumpRuntime::PumpRuntime() { state_.magic = MAGIC; state_.version = VERSION; }
uint32_t PumpRuntime::checksum(const State& state) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&state);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(State, checksum); ++i) hash = (hash ^ data[i]) * 16777619UL;
  return hash;
}
PumpRuntime::State PumpRuntime::snapshot() const {
  State result = state_;
  result.checksum = checksum(result);
  return result;
}
bool PumpRuntime::restore(const void* bytes, size_t size) {
  if (!bytes || size != sizeof(State)) return false;
  State candidate;
  memcpy(&candidate, bytes, sizeof(candidate));
  if (candidate.magic != MAGIC || candidate.version != VERSION || candidate.checksum != checksum(candidate)) return false;
  state_ = candidate;
  dirty_ = false;
  return true;
}
PumpRuntime::Period& PumpRuntime::slot(Period* periods, size_t count, uint32_t key) {
  Period* oldest = &periods[0];
  for (size_t i = 0; i < count; ++i) {
    if (periods[i].key == key) return periods[i];
    if (periods[i].key < oldest->key) oldest = &periods[i];
  }
  // Nie ältere Daten über neuere Einträge schreiben (z. B. nach einer Uhrkorrektur).
  if (oldest->key && key < oldest->key) return *oldest;
  *oldest = {};
  oldest->key = key;
  return *oldest;
}
void PumpRuntime::add(time_t epoch, uint32_t ms) {
  uint32_t seconds[CHANNELS]{};
  bool any = false;
  for (size_t i = 0; i < CHANNELS; ++i) {
    if (!active_[i]) continue;
    uint64_t elapsed = uint64_t(remainder_[i]) + ms;
    seconds[i] = uint32_t(elapsed / 1000);
    remainder_[i] = uint32_t(elapsed % 1000);
    any |= seconds[i] != 0;
  }
  if (!any) return;
  if (epoch < MIN_EPOCH) {
    for (size_t i = 0; i < CHANNELS; ++i) saturatingAdd(state_.undated[i], seconds[i]);
  } else {
    tm local{};
    if (!localtime_r(&epoch, &local)) return;
    const uint32_t year = uint32_t(local.tm_year + 1900), month = year * 100 + uint32_t(local.tm_mon + 1);
    const uint32_t day = month * 100 + uint32_t(local.tm_mday);
    Period* entries[] = { &slot(state_.days, DAYS, day), &slot(state_.months, MONTHS, month), &slot(state_.years, YEARS, year) };
    const uint32_t keys[] = {day, month, year};
    for (size_t p = 0; p < 3; ++p) {
      if (entries[p]->key != keys[p]) continue;
      for (size_t i = 0; i < CHANNELS; ++i) saturatingAdd(entries[p]->seconds[i], seconds[i]);
    }
  }
  dirty_ = true;
}
void PumpRuntime::update(uint32_t nowMs, time_t nowEpoch, bool solar, bool charge, bool discharge) {
  if (initialized_) {
    const uint32_t elapsed = nowMs - lastMs_;
    // Zeitkorrekturen beeinflussen nie die gemessene Dauer. Bei normal laufender
    // Uhr einen Mitternachtswechsel auf die beiden lokalen Tage aufteilen.
    if (lastEpoch_ >= MIN_EPOCH && nowEpoch >= MIN_EPOCH && nowEpoch >= lastEpoch_ &&
        uint64_t(nowEpoch - lastEpoch_) * 1000 <= uint64_t(elapsed) + 2000 && elapsed <= 86400000UL) {
      tm local{};
      if (localtime_r(&lastEpoch_, &local)) {
        local.tm_hour = 0; local.tm_min = 0; local.tm_sec = 0; ++local.tm_mday; local.tm_isdst = -1;
        time_t midnight = mktime(&local);
        if (midnight > lastEpoch_ && midnight <= nowEpoch) {
          uint64_t after = uint64_t(nowEpoch - midnight) * 1000;
          if (after > elapsed) after = elapsed;
          add(lastEpoch_, elapsed - uint32_t(after));
          add(midnight, uint32_t(after));
        } else add(lastEpoch_, elapsed);
      } else add(0, elapsed);
    } else {
      // Eine Zeit ohne gültige Uhr wird nicht nachträglich einem Datum zugeschrieben.
      add(lastEpoch_, elapsed);
    }
  }
  lastMs_ = nowMs; lastEpoch_ = nowEpoch; initialized_ = true;
  active_[0] = solar; active_[1] = charge; active_[2] = discharge;
  // Auch ein erfasster Zeitraum ohne Pumpenlauf ist ein gültiger Nullwert.
  if (nowEpoch >= MIN_EPOCH) {
    tm local{};
    if (localtime_r(&nowEpoch, &local)) {
      const uint32_t year = uint32_t(local.tm_year + 1900), month = year * 100 + uint32_t(local.tm_mon + 1);
      const uint32_t day = month * 100 + uint32_t(local.tm_mday);
      Period* groups[] = {state_.days, state_.months, state_.years};
      const size_t sizes[] = {DAYS, MONTHS, YEARS};
      const uint32_t keys[] = {day, month, year};
      for (size_t p = 0; p < 3; ++p) {
        bool found = false;
        for (size_t i = 0; i < sizes[p]; ++i) found |= groups[p][i].key == keys[p];
        if (!found && slot(groups[p], sizes[p], keys[p]).key == keys[p]) dirty_ = true;
      }
    }
  }
}
