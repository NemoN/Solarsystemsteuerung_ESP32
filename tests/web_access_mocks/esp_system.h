#pragma once
#include <cstdint>
#include <cstddef>
inline void esp_fill_random(void* out,size_t n){static uint8_t value=1;uint8_t* b=(uint8_t*)out;while(n--)*b++=value++;}
