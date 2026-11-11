#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct RemoteRelayConfig {
  char address[16];
  char key[33];
  char labels[4][49];
};
struct RemoteRelaySnapshot {
  RemoteRelayConfig config;
  bool online;
  bool busy;
  uint8_t mask;
  uint8_t forgetState;
  uint8_t rebootState;
  char version[32];
};
class RemoteRelay {
public:
  enum ForgetState : uint8_t { FORGET_IDLE, FORGET_PENDING, FORGET_DONE, FORGET_FAILED, FORGET_UNCERTAIN, FORGET_SAVE_FAILED };
  enum RebootState : uint8_t { REBOOT_IDLE, REBOOT_PENDING, REBOOT_SENT, REBOOT_ONLINE, REBOOT_UNSUPPORTED, REBOOT_UNCERTAIN, REBOOT_FAILED };
  bool begin(const char* storageKey = "config1");
  RemoteRelaySnapshot snapshot();
  bool configure(const String& address, const String& key, const String labels[4]);
  bool command(unsigned id, bool on);
  bool forget();
  bool reboot();
  bool clearAssociation();
private:
  static void taskEntry(void* arg);
  void task();
  SemaphoreHandle_t lock_ = nullptr;
  Preferences preferences_;
  char storageKey_[16] = {};
  RemoteRelayConfig config_ = {};
  bool storageReady_ = false, online_ = false, busy_ = false;
  uint8_t mask_ = 0, pendingId_ = 0;
  uint32_t lastContact_ = 0;
  uint32_t lastVersionCheck_ = 0, rebootSentAt_ = 0;
  bool versionChecked_ = false;
  char version_[32] = {};
  bool pendingValue_ = false;
  ForgetState forgetState_ = FORGET_IDLE;
  RebootState rebootState_ = REBOOT_IDLE;
};
