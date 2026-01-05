#pragma once

#include "rane_common.h"

// Driver / CLI for compiling .rane to a Windows .exe (legacy bootstrap)

typedef struct rane_driver_options_s {
  const char* input_path;
  const char* output_path;
  int opt_level; // 0..3
} rane_driver_options_t;

rane_error_t rane_compile_file_to_exe(const rane_driver_options_t* opts);

// New: compile .rane to a portable C file.
// The produced .c expects to be built with a normal toolchain (cl/clang/gcc).
// This is the first step toward a stable runtime ABI and cross-platform builds.
rane_error_t rane_compile_file_to_c(const rane_driver_options_t* opts);
