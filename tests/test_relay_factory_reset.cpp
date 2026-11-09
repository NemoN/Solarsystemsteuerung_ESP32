#include "../Relaiskarte/RelayFactoryReset.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
struct Storage {
 bool config=true, writeOk=true, removeOk=true, maskStored=true;
 uint8_t mask=7;
 unsigned putUChar(const char*,uint8_t value){if(!writeOk)return 0;mask=value;return 1;}
 uint8_t getUChar(const char*,uint8_t fallback){return maskStored?mask:fallback;}
 bool isKey(const char*){return config;}
 bool remove(const char*){if(!removeOk)return false;config=false;return true;}
};
int main(){
 bool off=false;Storage success;
 assert(relayFactoryReset(success,[&](){off=true;}));assert(off&&success.mask==0&&!success.config);
 Storage failedWrite;failedWrite.writeOk=false;off=false;
 assert(!relayFactoryReset(failedWrite,[&](){off=true;}));assert(off&&failedWrite.config);
 Storage failedRemove;failedRemove.removeOk=false;off=false;
 assert(!relayFactoryReset(failedRemove,[&](){off=true;}));assert(off&&failedRemove.mask==0&&failedRemove.config);
 Storage failedRead;failedRead.maskStored=false;
 assert(!relayFactoryReset(failedRead,[](){}));assert(failedRead.config);
 std::puts("PASS: Werksreset schaltet aus und entfernt Zugangsdaten nur bei bestätigtem Speichern.");
}
