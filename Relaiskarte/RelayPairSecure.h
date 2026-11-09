#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <esp_system.h>
#if defined(RELAY_PAIR_MBEDTLS_PRIVATE)
#include <mbedtls/private/ecdh.h>
#include <mbedtls/private/gcm.h>
#else
#include <mbedtls/ecdh.h>
#include <mbedtls/gcm.h>
#endif
#include <mbedtls/md.h>

// PAIR2 protects credentials against passive observation. Without a pre-shared
// secret it cannot authenticate the peer against an active intermediary.
inline int relaySecureRandom(void*, unsigned char* out, size_t length) {
  esp_fill_random(out, length); return 0;
}
inline void relaySecureHex(const uint8_t* bytes, size_t length, char* out) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < length; ++i) { out[i*2] = digits[bytes[i] >> 4]; out[i*2+1] = digits[bytes[i] & 15]; }
  out[length*2] = 0;
}
inline bool relaySecureUnhex(const char* input, size_t length, uint8_t* out) {
  if (!input || strlen(input) != length * 2) return false;
  for (size_t i = 0; i < length; ++i) {
    const char a = input[i*2], b = input[i*2+1];
    const int x = a >= '0' && a <= '9' ? a-'0' : a >= 'a' && a <= 'f' ? a-'a'+10 : -1;
    const int y = b >= '0' && b <= '9' ? b-'0' : b >= 'a' && b <= 'f' ? b-'a'+10 : -1;
    if (x < 0 || y < 0) return false;
    out[i] = static_cast<uint8_t>((x << 4) | y);
  }
  return true;
}
class RelayPairKey {
 public:
  RelayPairKey() { mbedtls_ecp_group_init(&group_); mbedtls_mpi_init(&private_); mbedtls_ecp_point_init(&public_); }
  ~RelayPairKey() { mbedtls_ecp_point_free(&public_); mbedtls_mpi_free(&private_); mbedtls_ecp_group_free(&group_); }
  RelayPairKey(const RelayPairKey&) = delete;
  RelayPairKey& operator=(const RelayPairKey&) = delete;
  void clear() { mbedtls_ecp_point_free(&public_); mbedtls_mpi_free(&private_); mbedtls_ecp_group_free(&group_); mbedtls_ecp_group_init(&group_); mbedtls_mpi_init(&private_); mbedtls_ecp_point_init(&public_); }
  bool generate(char output[131]) {
    unsigned char point[65]; size_t length = 0;
    if (mbedtls_ecp_group_load(&group_, MBEDTLS_ECP_DP_SECP256R1) ||
        mbedtls_ecdh_gen_public(&group_, &private_, &public_, relaySecureRandom, nullptr) ||
        mbedtls_ecp_point_write_binary(&group_, &public_, MBEDTLS_ECP_PF_UNCOMPRESSED, &length, point, sizeof(point)) || length != sizeof(point)) return false;
    relaySecureHex(point, sizeof(point), output); return true;
  }
  bool shared(const char* otherHex, const char* ownHex, bool mainSide, uint8_t key[32]) {
    uint8_t other[65], own[65], material[9+32+65+65] = {};
    if (!relaySecureUnhex(otherHex, sizeof(other), other) || !relaySecureUnhex(ownHex, sizeof(own), own)) return false;
    mbedtls_ecp_point peer; mbedtls_ecp_point_init(&peer);
    mbedtls_mpi secret; mbedtls_mpi_init(&secret);
    const bool ok = mbedtls_ecp_point_read_binary(&group_, &peer, other, sizeof(other)) == 0 &&
      mbedtls_ecp_check_pubkey(&group_, &peer) == 0 &&
      mbedtls_ecdh_compute_shared(&group_, &secret, &peer, &private_, relaySecureRandom, nullptr) == 0 &&
      mbedtls_mpi_write_binary(&secret, material+9, 32) == 0;
    mbedtls_mpi_free(&secret); mbedtls_ecp_point_free(&peer);
    if (!ok) return false;
    memcpy(material, "RelayPair", 9); memcpy(material+41, mainSide ? own : other, 65); memcpy(material+106, mainSide ? other : own, 65);
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    const bool hashed = md && mbedtls_md(md, material, sizeof(material), key) == 0;
    memset(material, 0, sizeof(material)); return hashed;
  }
 private:
  mbedtls_ecp_group group_;
  mbedtls_mpi private_;
  mbedtls_ecp_point public_;
};
// Envelope: 12-byte nonce, ciphertext and 16-byte tag, all lowercase hex.
inline bool relaySecureSeal(const uint8_t key[32], const char* aad, const char* text, char* out, size_t capacity) {
  const size_t size = strlen(text);
  if (size > 192 || capacity < (size + 28)*2 + 1) return false;
  uint8_t packet[220]; esp_fill_random(packet, 12);
  mbedtls_gcm_context ctx; mbedtls_gcm_init(&ctx);
  const bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
    mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, size, packet, 12,
      reinterpret_cast<const uint8_t*>(aad), strlen(aad), reinterpret_cast<const uint8_t*>(text), packet+12, 16, packet+12+size) == 0;
  mbedtls_gcm_free(&ctx);
  if (ok) relaySecureHex(packet, size+28, out);
  memset(packet, 0, sizeof(packet)); return ok;
}
inline bool relaySecureOpen(const uint8_t key[32], const char* aad, const char* envelope, char* out, size_t capacity) {
  const size_t hexLength = strlen(envelope);
  if (hexLength < 56 || hexLength > 440 || hexLength % 2) return false;
  const size_t bytes = hexLength/2, size = bytes-28;
  if (capacity <= size) return false;
  uint8_t packet[220]; if (!relaySecureUnhex(envelope, bytes, packet)) return false;
  mbedtls_gcm_context ctx; mbedtls_gcm_init(&ctx);
  const bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
    mbedtls_gcm_auth_decrypt(&ctx, size, packet, 12, reinterpret_cast<const uint8_t*>(aad), strlen(aad),
      packet+12+size, 16, packet+12, reinterpret_cast<uint8_t*>(out)) == 0;
  mbedtls_gcm_free(&ctx); memset(packet, 0, sizeof(packet));
  if (!ok) { memset(out, 0, capacity); return false; }
  out[size] = 0; return strlen(out) == size;
}
