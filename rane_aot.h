#pragma once

#include "rane_tir.h"

// AOT Compilation: Compile TIR to x64 ahead-of-time
rane_error_t rane_aot_compile(const rane_tir_module_t* tir_module, void** out_code, size_t* out_size);