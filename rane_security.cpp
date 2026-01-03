#include "rane_security.h"
#include <cstdlib>

int rane_security_check_integrity(const void* data, size_t len, const uint8_t* expected_hash) {
  return 0;
}

void rane_security_sandbox_enter() {
}

void rane_security_audit_log(const char* event) {
}

const char* rane_resolve_symbol(const char* name) {
  return NULL;
}

uint64_t rane_resolve_address(const char* symbol) {
  return 0;
}