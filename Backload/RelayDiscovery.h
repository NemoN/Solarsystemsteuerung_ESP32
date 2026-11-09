#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "RelayDiscoveryList.h"

struct RelayDiscoverySnapshot { bool available, searching, failed; RelayDiscoveryList results; };
class RelayDiscovery {
public:
  bool begin();
  bool search();
  RelayDiscoverySnapshot snapshot();
private:
  static void taskEntry(void* argument);
  void task();
  SemaphoreHandle_t lock_ = nullptr;
  bool searching_ = false, requested_ = false, failed_ = false;
  RelayDiscoveryList results_;
};
