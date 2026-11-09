#pragma once
#include <cstdint>
#include <cstddef>
inline int mbedtls_base64_decode(uint8_t* out,size_t cap,size_t* n,const uint8_t* src,size_t len){
 auto val=[](uint8_t c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
 if(len%4)return -1;*n=0;
 for(size_t i=0;i<len;i+=4){int a=val(src[i]),b=val(src[i+1]);if(a<0||b<0)return -1;int c=src[i+2]=='='?0:val(src[i+2]),d=src[i+3]=='='?0:val(src[i+3]);if(c<0||d<0||*n>=cap)return -1;out[(*n)++]=uint8_t((a<<2)|(b>>4));if(src[i+2]!='='){if(*n>=cap)return -1;out[(*n)++]=uint8_t((b<<4)|(c>>2));}if(src[i+3]!='='){if(*n>=cap)return -1;out[(*n)++]=uint8_t((c<<6)|d);}}
 return 0;
}

inline int mbedtls_base64_encode(uint8_t* out,size_t cap,size_t* n,const uint8_t* src,size_t len){
 static const char* t="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 const size_t needed=((len+2)/3)*4;if(cap<=needed)return -1;*n=0;
 for(size_t i=0;i<len;i+=3){unsigned x=unsigned(src[i])<<16;if(i+1<len)x|=unsigned(src[i+1])<<8;if(i+2<len)x|=src[i+2];out[(*n)++]=t[(x>>18)&63];out[(*n)++]=t[(x>>12)&63];out[(*n)++]=i+1<len?t[(x>>6)&63]:'=';out[(*n)++]=i+2<len?t[x&63]:'=';}
 out[*n]=0;return 0;
}
