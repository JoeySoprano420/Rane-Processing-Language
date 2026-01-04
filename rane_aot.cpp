#include "rane_aot.h"
#include "rane_x64.h"

#include <string.h>

rane_error_t rane_aot_compile(const rane_tir_module_t* tir_module, void** out_code, size_t* out_size) {
  // Use existing codegen
  rane_codegen_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.code_buffer = (uint8_t*)malloc(1024 * 1024); // 1MB
  ctx.buffer_size = 1024 * 1024;
  rane_error_t err = rane_x64_codegen_tir_to_machine(tir_module, &ctx);
  if (err == RANE_OK) {
    *out_code = ctx.code_buffer;
    *out_size = (size_t)ctx.code_size;
  }
  return err;
}

rane_error_t rane_aot_compile_with_fixups(const rane_tir_module_t* tir_module, rane_aot_result_t* out) {
  if (!tir_module || !out) return RANE_E_INVALID_ARG;
  memset(out, 0, sizeof(*out));

  rane_codegen_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));

  ctx.code_buffer = (uint8_t*)malloc(1024 * 1024);
  if (!ctx.code_buffer) return RANE_E_OS_API_FAIL;
  ctx.buffer_size = 1024 * 1024;

  // Allocate fixup table for imported calls
  ctx.call_fixup_capacity = 128;
  rane_aot_call_fixup_t* fixups = (rane_aot_call_fixup_t*)calloc(ctx.call_fixup_capacity, sizeof(rane_aot_call_fixup_t));
  if (!fixups) {
    free(ctx.code_buffer);
    return RANE_E_OS_API_FAIL;
  }
  ctx.call_fixups = fixups;

  rane_error_t err = rane_x64_codegen_tir_to_machine(tir_module, &ctx);
  if (err != RANE_OK) {
    free(fixups);
    free(ctx.code_buffer);
    return err;
  }

  out->code = ctx.code_buffer;
  out->code_size = (size_t)ctx.code_size;
  out->call_fixups = fixups;
  out->call_fixup_count = ctx.call_fixup_count;
  return RANE_OK;
}

