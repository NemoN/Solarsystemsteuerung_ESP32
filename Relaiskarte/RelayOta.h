#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>

// Dieses Kennzeichen steht auch in der Firmwaredatei und identifiziert das Zielgerät.
constexpr char RELAY_FIRMWARE_MARKER[] = "RBS16467-RELAY-FIRMWARE-OTA1";

class RelayOta {
public:
  void prepare(WebServer& server);
  void chunk(WebServer& server);
  void finish(WebServer& server);
  void abort(WebServer& server);
  void info(WebServer& server, bool bootPending);
  void loop();
  bool active() const { return active_ || restartPending_; }
private:
  void cancel();
  bool matches(WebServer& server) const;
  bool active_ = false, restartPending_ = false, hashReady_ = false, markerSeen_ = false;
  uint32_t startedAt_ = 0, restartAt_ = 0;
  size_t size_ = 0, received_ = 0, lastOffset_ = 0, lastLength_ = 0, markerMatch_ = 0;
  uint32_t targetAddress_ = 0;
  char token_[33] = {}, expected_[65] = {};
  uint8_t lastHash_[32] = {};
  mbedtls_sha256_context hash_;
};
