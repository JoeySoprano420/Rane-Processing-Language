#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <stdint.h>

#include "rane_tir.h"

// x64 Emitter for RANE TIR
// Emits machine code into a buffer based on TIR instructions.

typedef struct rane_x64_emitter_s {
  uint8_t* buffer;
  uint64_t buffer_size;
  uint64_t offset;
} rane_x64_emitter_t;

// Initialize emitter with buffer
void rane_x64_init_emitter(rane_x64_emitter_t* e, uint8_t* buf, uint64_t size);

// Ensure space and advance offset
int rane_x64_ensure_space(rane_x64_emitter_t* e, uint64_t needed);

// Emit bytes
void rane_x64_emit_bytes(rane_x64_emitter_t* e, const uint8_t* bytes, uint64_t count);

// Instruction emitters (subset for bootstrap)
void rane_x64_emit_mov_reg_imm64(rane_x64_emitter_t* e, uint8_t reg, uint64_t imm);
void rane_x64_emit_mov_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_mov_reg_mem(rane_x64_emitter_t* e, uint8_t reg, uint8_t base_reg, int32_t disp);
void rane_x64_emit_mov_mem_reg(rane_x64_emitter_t* e, uint8_t base_reg, int32_t disp, uint8_t reg);
void rane_x64_emit_add_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_sub_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_cmp_reg_reg(rane_x64_emitter_t* e, uint8_t reg1, uint8_t reg2);
void rane_x64_emit_jmp_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_je_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_jne_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_jl_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_jle_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_jg_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_jge_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_call_rel32(rane_x64_emitter_t* e, int32_t rel);
void rane_x64_emit_ret(rane_x64_emitter_t* e);
void rane_x64_emit_mul_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_div_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_cmp_reg_imm(rane_x64_emitter_t* e, uint8_t reg, uint64_t imm);

// High-level: emit TIR instruction
void rane_x64_emit_tir_inst(rane_x64_emitter_t* e, const rane_tir_inst_t* inst, uint64_t* label_offsets);

// Emit entire module (assumes single function for now)
rane_error_t rane_x64_emit_module(rane_x64_emitter_t* e, const rane_tir_module_t* mod);

// High-level: codegen TIR module into provided code buffer context
rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx);

typedef struct rane_x64_fixup_s {
  char label[64];
  uint64_t patch_offset; // offset of rel32 immediate
  uint8_t kind;          // 1=jmp, 2=je, 3=jne
} rane_x64_fixup_t;

typedef struct rane_x64_label_map_entry_s {
  char label[64];
  uint64_t offset;
} rane_x64_label_map_entry_t;

