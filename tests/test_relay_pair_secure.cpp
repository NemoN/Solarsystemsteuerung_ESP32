#include "../Backload/RelayPairSecure.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
  RelayPairKey mainKey, cardKey, otherKey;
  char mainPub[131], cardPub[131], otherPub[131];
  assert(mainKey.generate(mainPub) && cardKey.generate(cardPub) && otherKey.generate(otherPub));
  uint8_t a[32], b[32];
  assert(mainKey.shared(cardPub, mainPub, true, a));
  assert(cardKey.shared(mainPub, cardPub, false, b));
  assert(!memcmp(a,b,32));
  uint8_t wrong[32]; assert(otherKey.shared(mainPub, otherPub, false, wrong)); assert(memcmp(a,wrong,32));
  char sealed[441], out[193];
  assert(relaySecureSeal(a, "relay-wifi:12345678abcd", "PAIR1:U1NJRA==:cGFzc3dvcmQ=", sealed, sizeof(sealed)));
  assert(!strstr(sealed,"PAIR1") && relaySecureOpen(b,"relay-wifi:12345678abcd",sealed,out,sizeof(out)));
  assert(!strcmp(out,"PAIR1:U1NJRA==:cGFzc3dvcmQ="));
  assert(!relaySecureOpen(wrong,"relay-wifi:12345678abcd",sealed,out,sizeof(out)));
  assert(!relaySecureOpen(b,"relay-key:12345678abcd",sealed,out,sizeof(out)));
  sealed[30] = sealed[30]=='a'?'b':'a';
  assert(!relaySecureOpen(b,"relay-wifi:12345678abcd",sealed,out,sizeof(out)));
  assert(!mainKey.shared("bad",mainPub,true,a));
  mainKey.clear();
  puts("PASS: ECDH-Schlüsseltausch und authentifizierte Verschlüsselung; Manipulation erkannt.");
}
