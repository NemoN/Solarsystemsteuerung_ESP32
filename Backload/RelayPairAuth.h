#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <mbedtls/md.h>

inline bool relayPairHex(const char* text, size_t size) {
  if (!text || strlen(text) != size) return false;
  for (size_t i = 0; i < size; ++i)
    if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
  return true;
}
inline bool relayPairProof(const char* key, const char* nonce, char out[65]) {
  if (!relayPairHex(key, 32) || !relayPairHex(nonce, 32)) return false;
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  uint8_t digest[32];
  if (!md || mbedtls_md_hmac(md, reinterpret_cast<const uint8_t*>(key), 32,
      reinterpret_cast<const uint8_t*>(nonce), 32, digest) != 0) return false;
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) { out[2*i] = hex[digest[i] >> 4]; out[2*i+1] = hex[digest[i] & 15]; }
  out[64] = 0; return true;
}
inline bool relayPairEqual(const char* a, const char* b, size_t size) {
  if (!a || !b || strlen(a) != size || strlen(b) != size) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < size; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}
