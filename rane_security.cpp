#include "rane_security.h"

#include <cstdio>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

static void rane_security_debug_out(const char* s) {
  if (!s) return;
  OutputDebugStringA(s);
  OutputDebugStringA("\n");
}

static uint64_t fnv1a64(const char* s) {
  if (!s) return 0;
  uint64_t h = 1469598103934665603ull;
  while (*s) {
    h ^= (uint8_t)(*s++);
    h *= 1099511628211ull;
  }
  return h;
}

static int sha256_bytes(const void* data, size_t len, uint8_t out32[32]) {
  if (!out32) return 0;
  memset(out32, 0, 32);
  if (!data && len != 0) return 0;

  BCRYPT_ALG_HANDLE hAlg = NULL;
  BCRYPT_HASH_HANDLE hHash = NULL;
  uint8_t* hash_obj = NULL;
  DWORD hash_obj_len = 0;
  DWORD cb = 0;

  if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return 0;

  if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_obj_len, sizeof(hash_obj_len), &cb, 0) != 0) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  hash_obj = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, hash_obj_len ? hash_obj_len : 1);
  if (!hash_obj) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  if (BCryptCreateHash(hAlg, &hHash, (PUCHAR)hash_obj, hash_obj_len, NULL, 0, 0) != 0) {
    HeapFree(GetProcessHeap(), 0, hash_obj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  if (len != 0) {
    if (BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0) != 0) {
      BCryptDestroyHash(hHash);
      HeapFree(GetProcessHeap(), 0, hash_obj);
      BCryptCloseAlgorithmProvider(hAlg, 0);
      return 0;
    }
  }

  if (BCryptFinishHash(hHash, (PUCHAR)out32, 32, 0) != 0) {
    BCryptDestroyHash(hHash);
    HeapFree(GetProcessHeap(), 0, hash_obj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  BCryptDestroyHash(hHash);
  HeapFree(GetProcessHeap(), 0, hash_obj);
  BCryptCloseAlgorithmProvider(hAlg, 0);
  return 1;
}

int rane_security_check_integrity(const void* data, size_t len, const uint8_t* expected_hash) {
  // expected_hash must point to 32 bytes (SHA-256)
  if (!expected_hash) return 0;

  uint8_t got[32];
  if (!sha256_bytes(data, len, got)) return 0;

  return memcmp(got, expected_hash, 32) == 0;
}

void rane_security_sandbox_enter() {
  // Minimal "sandbox" in this bootstrap repo: reduce crash surface and log intent.
  // Full sandboxing on Windows would require tokens/job objects/AppContainer,
  // which is out of scope for this API surface without configuration inputs.
  rane_security_audit_log("sandbox_enter: no-op (bootstrap)");
}

void rane_security_audit_log(const char* event) {
  if (!event) event = "<null>";
  char buf[768];
  DWORD tid = GetCurrentThreadId();
  DWORD pid = GetCurrentProcessId();
  sprintf_s(buf, sizeof(buf), "rane_audit pid=%lu tid=%lu: %s",
            (unsigned long)pid, (unsigned long)tid, event);

  rane_security_debug_out(buf);
  fprintf(stderr, "%s\n", buf);
}

const char* rane_resolve_symbol(const char* name) {
  // Minimal resolver: return canonicalized names for known runtime symbols.
  // For now we only normalize a few expected symbols.
  if (!name) return NULL;
  if (strcmp(name, "rane_rt_print") == 0) return "rane_rt_print";
  if (strcmp(name, "printf") == 0) return "printf";
  return name;
}

uint64_t rane_resolve_address(const char* symbol) {
  // Resolve within current process module table.
  if (!symbol) return 0;

  HMODULE h = GetModuleHandleA(NULL);
  if (!h) return 0;

  FARPROC p = GetProcAddress(h, symbol);
  if (p) return (uint64_t)(uintptr_t)p;

  // Fallback: attempt CRT for common imports
  HMODULE crt = GetModuleHandleA("msvcrt.dll");
  if (!crt) crt = LoadLibraryA("msvcrt.dll");
  if (crt) {
    p = GetProcAddress(crt, symbol);
    if (p) return (uint64_t)(uintptr_t)p;
  }

  // Last resort: stable hash (so callers can still key off something deterministic)
  return fnv1a64(symbol);
}
