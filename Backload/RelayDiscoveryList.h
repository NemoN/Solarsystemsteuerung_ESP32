#pragma once
#include <stddef.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// mDNS-Einträge sind nur Vorschläge; sie enthalten keine Zugangsschlüssel.
struct RelayDiscoveryEntry { char id[13]; char name[32]; char address[16]; };
class RelayDiscoveryList {
public:
  static constexpr size_t CAPACITY = 12;
  size_t size() const { return size_; }
  const RelayDiscoveryEntry& operator[](size_t i) const { return entries_[i]; }
  bool add(const char* hostname, const uint8_t ip[4], const uint8_t local[4], const uint8_t subnet[4], uint16_t port) {
    if (!hostname || strlen(hostname) != 19 || memcmp(hostname, "relais-", 7) != 0 || port != 80) return false;
    for (size_t i = 7; i < 19; ++i) {
      const char c = hostname[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    bool same = true, network = true, broadcast = true;
    for (size_t i = 0; i < 4; ++i) {
      if ((ip[i] & subnet[i]) != (local[i] & subnet[i])) return false;
      if (ip[i] != local[i]) same = false;
      if ((ip[i] & uint8_t(~subnet[i])) != 0) network = false;
      if ((ip[i] & uint8_t(~subnet[i])) != uint8_t(~subnet[i])) broadcast = false;
    }
    if (same || network || broadcast || ip[0] == 0 || ip[0] == 127 || ip[0] >= 224) return false;
    char address[16]; snprintf(address, sizeof(address), "%u.%u.%u.%u", unsigned(ip[0]), unsigned(ip[1]), unsigned(ip[2]), unsigned(ip[3]));
    for (size_t i = 0; i < size_; ++i) if (!strcmp(entries_[i].id, hostname + 7) && !strcmp(entries_[i].address, address)) return false;
    if (size_ == CAPACITY) return false;
    RelayDiscoveryEntry& entry = entries_[size_++];
    memcpy(entry.id, hostname + 7, 12); entry.id[12] = 0;
    char suffix[5] = {};
    for (size_t i = 0; i < 4; ++i) suffix[i] = static_cast<char>(toupper(static_cast<unsigned char>(entry.id[i + 8])));
    snprintf(entry.name, sizeof(entry.name), "Relaiserweiterung-%s", suffix);
    memcpy(entry.address, address, strlen(address) + 1);
    return true;
  }
private:
  RelayDiscoveryEntry entries_[CAPACITY] = {};
  size_t size_ = 0;
};
