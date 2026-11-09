#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#ifdef BACKLOAD_HOST_TEST
#include <AsyncWebServerMock.h>
#else
#include "src/AsyncTCP/src/AsyncTCP.h"
#include "src/ESPAsyncWebServer/src/ESPAsyncWebServer.h"
#endif
#include "RemoteRelay.h"
#include "RelayDiscovery.h"
#include "RelayPairSecure.h"

struct RelayProvisionSnapshot { char state[16]; char message[144]; unsigned board; };
class RelayProvisioning {
public:
  bool begin(RemoteRelay* boards, unsigned count, RelayDiscovery* discovery);
  bool start(unsigned board, const String& ssid, const char* homeSsid, const char* homePassword);
  void loop();
  bool cancel();
  bool active();
  bool apActive();
  RelayProvisionSnapshot snapshot();
private:
  enum Phase { IDLE, WAITING, CLAIMED, SEARCHING, VERIFYING, DONE, FAILED };
  void handleClaim(AsyncWebServerRequest* request);
  void handleHello(AsyncWebServerRequest* request);
  void stopPairAp();
  void finish(Phase phase, const char* message);
  static void verifyTask(void* arg);
  AsyncWebServer server_{8088};
  SemaphoreHandle_t lock_ = nullptr;
  RemoteRelay* boards_ = nullptr;
  RelayDiscovery* discovery_ = nullptr;
  unsigned count_ = 0, board_ = 0;
  Phase phase_ = IDLE;
  uint32_t startedAt_ = 0, claimedAt_ = 0, lastSearch_ = 0;
  RelayPairKey pairKey_;
  char pairPublic_[131] = {}, expectedSuffix_[5] = {}, deviceId_[13] = {}, deviceKey_[33] = {};
  char homeSsid_[33] = {}, homePassword_[64] = {}, message_[144] = {};
  char verifyAddress_[16] = {}, verifyKey_[33] = {}, verifyNonce_[33] = {};
  std::atomic<int> verifyState_{0};
};
