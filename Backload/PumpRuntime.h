#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Alle Zeiträume sind lokale Kalenderräume; die Laufzeit selbst basiert auf millis().
class PumpRuntime {
 public:
  static constexpr size_t DAYS = 32, MONTHS = 24, YEARS = 12;
  static constexpr size_t CHANNELS = 3; // Solar, Kombi -> Puffer, Puffer -> Kombi
  struct Period { uint32_t key; uint32_t seconds[CHANNELS]; };
  struct State {
    uint32_t magic, version;
    Period days[DAYS], months[MONTHS], years[YEARS];
    uint32_t undated[CHANNELS];
    uint32_t checksum;
  };

  PumpRuntime();
  bool restore(const void* bytes, size_t size);
  const State& state() const { return state_; }
  State snapshot() const;
  // Zeit bis zum Aufruf gehört zum zuletzt übergebenen Relaiszustand.
  void update(uint32_t nowMs, time_t nowEpoch, bool solar, bool charge, bool discharge);
  bool dirty() const { return dirty_; }
  void saved() { dirty_ = false; }
  bool clockValid() const { return lastEpoch_ >= MIN_EPOCH; }
  static constexpr time_t MIN_EPOCH = 1704067200; // 2024-01-01 UTC
 private:
  State state_{};
  uint32_t lastMs_ = 0, remainder_[CHANNELS]{};
  time_t lastEpoch_ = 0;
  bool initialized_ = false, active_[CHANNELS]{}, dirty_ = false;
  static uint32_t checksum(const State& state);
  static Period& slot(Period* periods, size_t count, uint32_t key);
  void add(time_t epoch, uint32_t ms);
};
