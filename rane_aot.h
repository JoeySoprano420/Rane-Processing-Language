#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "rane_tir.h"

// AOT Compilation: Compile TIR to x64 ahead-of-time
rane_error_t rane_aot_compile(const rane_tir_module_t* tir_module, void** out_code, size_t* out_size);

typedef struct rane_aot_call_fixup_s {
  char sym[64];
  uint32_t code_offset; // offset into code buffer where placeholder begins
} rane_aot_call_fixup_t;

typedef struct rane_aot_result_s {
  void* code;
  size_t code_size;
  rane_aot_call_fixup_t* call_fixups;
  uint32_t call_fixup_count;
} rane_aot_result_t;

rane_error_t rane_aot_compile_with_fixups(const rane_tir_module_t* mod, rane_aot_result_t* out);
