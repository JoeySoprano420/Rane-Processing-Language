#include "rane_crypto.h"
#include <cstdlib>

rane_aes_key_t* rane_aes_key_new(const uint8_t* key, size_t len) {
  return NULL;
}

void rane_aes_encrypt(rane_aes_key_t* key, const uint8_t* in, uint8_t* out, size_t len) {
}

void rane_aes_decrypt(rane_aes_key_t* key, const uint8_t* in, uint8_t* out, size_t len) {
}

void rane_aes_key_free(rane_aes_key_t* key) {
}

void rane_sha256(const uint8_t* data, size_t len, uint8_t* hash) {
}

void rane_secure_random(uint8_t* buf, size_t len) {
}