#include "../Backload/RelayPairAuth.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 const char* key="0123456789abcdef0123456789abcdef";
 const char* nonce="fedcba9876543210fedcba9876543210";
 char proof[65]={};
 assert(relayPairProof(key,nonce,proof));
 assert(!strcmp(proof,"faf2b0c0c70f848e8c8fd0f5139bdfad89a1608a63359fd0e7677baa6e73eaa3"));
 assert(relayPairEqual(proof,proof,64));
 char changed[65];std::memcpy(changed,proof,65);changed[3]=changed[3]=='a'?'b':'a';
 assert(!relayPairEqual(proof,changed,64));
 assert(!relayPairProof("invalid",nonce,proof));
 assert(!relayPairProof(key,"FEDCBA9876543210fedcba9876543210",proof));
 assert(!relayPairEqual(nullptr,proof,64));
 std::puts("PASS: Kopplung prüft Schlüssel und frische HMAC-Challenge.");
}
