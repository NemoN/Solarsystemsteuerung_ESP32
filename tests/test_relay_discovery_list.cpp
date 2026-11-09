#include "../Backload/RelayDiscoveryList.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
  RelayDiscoveryList list;const uint8_t own[]={192,168,1,5}, mask[]={255,255,255,0};
  const uint8_t valid[]={192,168,1,42},valid2[]={192,168,1,43}, other[]={192,168,2,42}, network[]={192,168,1,0}, broadcast[]={192,168,1,255};
  assert(list.add("relais-123456789abc",valid,own,mask,80));
  assert(list.size()==1&&!strcmp(list[0].id,"123456789abc"));
  assert(!strcmp(list[0].name,"Relaiserweiterung-9ABC")&&!strcmp(list[0].address,"192.168.1.42"));
  assert(!list.add("relais-123456789abc",valid,own,mask,80));
  assert(list.add("relais-123456789abc",valid2,own,mask,80)); // Konflikte sichtbar lassen.
  assert(!list.add("relais-123456789abc",other,own,mask,80));
  assert(!list.add("relais-123456789abc",network,own,mask,80));
  assert(!list.add("relais-123456789abc",broadcast,own,mask,80));
  assert(!list.add("relais-123456789abc",own,own,mask,80));
  assert(!list.add("relais-123456789abc",valid,own,mask,81));
  assert(!list.add("relais-123456789abg",valid,own,mask,80));
  assert(!list.add("relais-123456789ab",valid,own,mask,80));
  std::puts("PASS: Relaiserweiterungen erkennen, deduplizieren und auf das Heimnetz begrenzen.");
}
