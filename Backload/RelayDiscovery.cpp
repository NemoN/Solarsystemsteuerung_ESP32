#include "RelayDiscovery.h"
#include <WiFi.h>
#include <mdns.h>

bool RelayDiscovery::begin() {
  lock_ = xSemaphoreCreateMutex();
  if (!lock_) return false;
  return xTaskCreate(taskEntry, "relay-discovery", 6144, this, 1, nullptr) == pdPASS;
}
bool RelayDiscovery::search() {
  if (!lock_ || WiFi.status() != WL_CONNECTED) return false;
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (searching_ || requested_) { xSemaphoreGive(lock_); return false; }
  requested_ = true; failed_ = false;
  xSemaphoreGive(lock_); return true;
}
RelayDiscoverySnapshot RelayDiscovery::snapshot() {
  RelayDiscoverySnapshot result = {};
  if (!lock_) return result;
  xSemaphoreTake(lock_, portMAX_DELAY);
  result.available = WiFi.status() == WL_CONNECTED;
  result.searching = searching_ || requested_; result.failed = failed_; result.results = results_;
  xSemaphoreGive(lock_); return result;
}
void RelayDiscovery::taskEntry(void* argument) { static_cast<RelayDiscovery*>(argument)->task(); }
void RelayDiscovery::task() {
  uint32_t previousScan = 0;
  bool wasConnected = false;
  for (;;) {
    const bool connected = WiFi.status() == WL_CONNECTED;
    const uint32_t now = millis();
    bool query = false;
    xSemaphoreTake(lock_, portMAX_DELAY);
    if (!connected) { results_ = RelayDiscoveryList(); requested_ = false; wasConnected = false; }
    else if (requested_ || !wasConnected || now - previousScan >= 30000) {
      requested_ = false; searching_ = true; wasConnected = true; query = true;
    }
    xSemaphoreGive(lock_);
    if (query) {
      RelayDiscoveryList next;
      const IPAddress ip = WiFi.localIP(), netmask = WiFi.subnetMask();
      uint8_t own[4], mask[4];
      for (int i = 0; i < 4; ++i) { own[i] = ip[i]; mask[i] = netmask[i]; }
      mdns_result_t* results = nullptr;
      const esp_err_t err = mdns_query_ptr("_backload-relay", "_tcp", 2500, 24, &results);
      if (err == ESP_OK) for (mdns_result_t* r = results; r; r = r->next) {
        if (r->tcpip_if != MDNS_IF_STA || r->port != 80) continue;
        for (mdns_ip_addr_t* a = r->addr; a; a = a->next) {
          if (a->addr.type != MDNS_IP_PROTOCOL_V4) continue;
          const IPAddress address(a->addr.u_addr.ip4.addr);
          uint8_t bytes[4]; for (int i = 0; i < 4; ++i) bytes[i] = address[i];
          next.add(r->hostname, bytes, own, mask, r->port);
        }
      }
      if (results) mdns_query_results_free(results);
      xSemaphoreTake(lock_, portMAX_DELAY);
      if (WiFi.status() == WL_CONNECTED && WiFi.localIP() == ip && WiFi.subnetMask() == netmask) {
        results_ = next; failed_ = err != ESP_OK;
      } else { results_ = RelayDiscoveryList(); failed_ = false; wasConnected = false; }
      searching_ = false; previousScan = millis();
      xSemaphoreGive(lock_);
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
