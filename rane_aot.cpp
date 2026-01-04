#include "rane_aot.h"
#include "rane_x64.h"
#include <string.h>

rane_error_t rane_aot_compile(const rane_tir_module_t* tir_module, void** out_code, size_t* out_size) {
  // Use existing codegen
  rane_codegen_ctx_t ctx;
  ctx.code_buffer = (uint8_t*)malloc(1024 * 1024); // 1MB
  ctx.buffer_size = 1024 * 1024;
  rane_error_t err = rane_x64_codegen_tir_to_machine(tir_module, &ctx);
  if (err == RANE_OK) {
    *out_code = ctx.code_buffer;
    *out_size = ctx.code_size;
  }
  return err;
}

rane_error_t rane_aot_compile_with_fixups(const rane_tir_module_t* tir_module, rane_aot_result_t* out) {
  if (!tir_module || !out) return RANE_E_INVALID_ARG;
  memset(out, 0, sizeof(*out));

  void* code = NULL;
  size_t code_size = 0;
  rane_error_t err = rane_aot_compile(tir_module, &code, &code_size);
  if (err != RANE_OK) return err;

  out->code = code;
  out->code_size = code_size;

  // TODO: plumb real fixups from codegen.
  out->call_fixups = NULL;
  out->call_fixup_count = 0;
  return RANE_OK;
}

