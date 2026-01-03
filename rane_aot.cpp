#include "rane_aot.h"
#include "rane_x64.h"

rane_error_t rane_aot_compile(const rane_tir_module_t* tir_module, void** out_code, size_t* out_size) {
  // Use existing codegen
  rane_codegen_ctx_t ctx;
  ctx.code_buffer = malloc(1024 * 1024); // 1MB
  ctx.buffer_size = 1024 * 1024;
  rane_error_t err = rane_x64_codegen_tvm_to_machine(tir_module, &ctx);
  if (err == RANE_OK) {
    *out_code = ctx.code_buffer;
    *out_size = ctx.code_size;
  }
  return err;
}