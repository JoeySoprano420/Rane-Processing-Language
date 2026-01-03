#pragma once

#include <cstdint>
#include <cstddef>

// Cryptography Library for RANE
// AES, SHA, RSA stubs

typedef struct rane_aes_key_s rane_aes_key_t;

rane_aes_key_t* rane_aes_key_new(const uint8_t* key, size_t len);
void rane_aes_encrypt(rane_aes_key_t* key, const uint8_t* in, uint8_t* out, size_t len);
void rane_aes_decrypt(rane_aes_key_t* key, const uint8_t* in, uint8_t* out, size_t len);
void rane_aes_key_free(rane_aes_key_t* key);

// SHA256
void rane_sha256(const uint8_t* data, size_t len, uint8_t* hash);

// Secure random
void rane_secure_random(uint8_t* buf, size_t len);