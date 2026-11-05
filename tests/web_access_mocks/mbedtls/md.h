#pragma once
struct mbedtls_md_context_t {};
struct mbedtls_md_info_t {};
#define MBEDTLS_MD_SHA256 1
inline void mbedtls_md_init(mbedtls_md_context_t*){}
inline const mbedtls_md_info_t* mbedtls_md_info_from_type(int){static mbedtls_md_info_t md;return &md;}
inline int mbedtls_md_setup(mbedtls_md_context_t*,const mbedtls_md_info_t*,int){return 0;}
inline void mbedtls_md_free(mbedtls_md_context_t*){}
