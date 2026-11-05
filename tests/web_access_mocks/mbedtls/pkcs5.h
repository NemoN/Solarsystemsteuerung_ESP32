#pragma once
#include "md.h"
#include <cstddef>
inline int mbedtls_pkcs5_pbkdf2_hmac(mbedtls_md_context_t*,const unsigned char* p,size_t n,const unsigned char* salt,size_t sn,unsigned,int len,unsigned char* out){for(int i=0;i<len;i++){out[i]=salt[i%sn];for(size_t j=0;j<n;j++)out[i]=(out[i]*33)^p[j];}return 0;}
