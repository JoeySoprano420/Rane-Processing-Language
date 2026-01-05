#pragma once

#include "rane_common.h"
#include "rane_tir.h"

#ifdef __cplusplus
extern "C" {
#endif

// Minimal C backend: emit a single C translation unit from a TIR module.
// This is intended as a cross-platform bootstrap path to leverage the system C toolchain.
//
// Current coverage: enough for simple integer programs and the builtin `print`.

typedef struct rane_c_backend_options_s {
  const char* output_c_path;
} rane_c_backend_options_t;

rane_error_t rane_emit_c_from_tir(const rane_tir_module_t* mod, const rane_c_backend_options_t* opts);

#ifdef __cplusplus
} // extern "C"
#endif
