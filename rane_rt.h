#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#define RANE_RT_ABI_VERSION 1

// Minimal runtime ABI for bootstrap portability.
// All strings are assumed to be UTF-8, null-terminated.

void rane_rt_print(const char* s);

#ifdef __cplusplus
} // extern "C"
#endif
