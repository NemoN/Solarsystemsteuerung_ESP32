#pragma once
#include <cstdint>
struct esp_partition_t { uint32_t address; uint32_t size; };
inline const esp_partition_t* esp_ota_get_next_update_partition(void*) { static const esp_partition_t p={0x150000,0x140000}; return &p; }
extern uint32_t mockRunningAddress;
inline const esp_partition_t* esp_ota_get_running_partition() { static esp_partition_t p={0x10000,0x140000}; p.address=mockRunningAddress; return &p; }
