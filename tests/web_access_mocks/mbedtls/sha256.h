#pragma once
#include <cstddef>
#include <cstdint>
inline int mbedtls_sha256_ret(const uint8_t* p,size_t n,uint8_t* out,int){for(int i=0;i<32;i++)out[i]=i;for(size_t i=0;i<n;i++)out[i%32]=uint8_t(out[i%32]*33^p[i]);return 0;}
