#pragma once
#include <cstdint>
#include <cstddef>
inline void esp_fill_random(void* p,size_t n){static uint32_t state=0x31415926;auto* x=static_cast<uint8_t*>(p);for(size_t i=0;i<n;i++){state^=state<<13;state^=state>>17;state^=state<<5;x[i]=static_cast<uint8_t>(state);}}
