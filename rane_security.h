#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>

// Security Library for RANE
// Access control, sandboxing, integrity checks

int rane_security_check_integrity(const void* data, size_t len, const uint8_t* expected_hash);
void rane_security_sandbox_enter();
void rane_security_audit_log(const char* event);

// Resolvers
const char* rane_resolve_symbol(const char* name);
uint64_t rane_resolve_address(const char* symbol);
