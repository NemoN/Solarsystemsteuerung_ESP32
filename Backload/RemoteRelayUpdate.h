#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "RemoteRelay.h"

struct RemoteRelayUpdateStatus {
  char state[16];
  char error[144];
  size_t size, received, sent;
};
class RemoteRelayUpdate {
public:
  enum class WatchdogAction : uint8_t { None, CancelRequested, RestartRequired };
  enum class StatusResult : uint8_t { Ok, NotFound, Busy };
  bool begin();
  bool prepare(size_t size, const RemoteRelaySnapshot& relay, char token[33]);
  bool prepareGithub(size_t size, const String& url, const String& sha256, const RemoteRelaySnapshot& relay, char token[33]);
  bool chunk(const String& token, size_t offset, const String& base64);
  bool start(const String& token);
  bool cancel(const String& token);
  bool status(const String& token, RemoteRelayUpdateStatus& result);
  StatusResult readStatus(const String& token, RemoteRelayUpdateStatus& result);
  bool active();
  void loop();
  WatchdogAction watchdog();
  bool idleForWatchdogRecovery();
  void watchdogRestartBlocked(const char* reason);
private:
  enum State { IDLE, RECEIVING, DOWNLOADING, SENDING, VERIFYING, DONE, FAILED, UNKNOWN, CANCELED };
  static void taskEntry(void* p);
  void task();
  bool downloadGithub();
  void releaseImage();
  void finishTask(State state, const char* message);
  void touch();
  bool canceled();
  SemaphoreHandle_t lock_ = nullptr;
  State state_ = IDLE;
  uint8_t* image_ = nullptr;
  size_t size_ = 0, received_ = 0, sent_ = 0, lastOffset_ = 0, lastLength_ = 0;
  uint32_t lastActivity_ = 0;
  uint32_t watchdogSince_ = 0;
  bool watchdogTriggered_ = false;
  uint32_t lockBlockedSince_ = 0;
  bool lockBlocked_ = false;
  uint8_t lastHash_[32] = {};
  bool cancelRequested_ = false;
  char token_[33] = {}, error_[144] = {};
  char githubUrl_[256] = {}, githubSha256_[65] = {};
  RemoteRelayConfig config_ = {};
};
