#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
struct mbedtls_sha256_context { uint8_t data[32]; size_t count; };
inline void mbedtls_sha256_init(mbedtls_sha256_context* x){std::memset(x,0,sizeof(*x));}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context* x,int){std::memset(x,0,sizeof(*x));return 0;}
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context* x,const uint8_t* d,size_t n){for(size_t i=0;i<n;i++){size_t k=x->count++%32;x->data[k]=uint8_t((x->data[k]*33u)^d[i]);}return 0;}
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context* x,uint8_t out[32]){std::memcpy(out,x->data,32);return 0;}
inline int mbedtls_sha256_ret(const uint8_t* d,size_t n,uint8_t out[32],int){mbedtls_sha256_context x;mbedtls_sha256_init(&x);mbedtls_sha256_update_ret(&x,d,n);return mbedtls_sha256_finish_ret(&x,out);}
