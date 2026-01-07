#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <stdint.h>

#include "rane_tir.h"

// -----------------------------------------------------------------------------
// rane_x64.h
//
// Windows x64 backend (bootstrap).
// Emits machine code into a caller-provided buffer.
//
// Design goals (bootstrap):
// - Small, deterministic emission.
// - No dynamic allocations in the emitter itself.
// - Minimal dependencies; works in Visual Studio 2026.
// - Keep API stable; expand carefully.
// -----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Register encoding (x64, Win64 ABI)
// -----------------------------------------------------------------------------
typedef enum rane_x64_reg_e : uint8_t {
  RANE_X64_RAX = 0,
  RANE_X64_RCX = 1,
  RANE_X64_RDX = 2,
  RANE_X64_RBX = 3,
  RANE_X64_RSP = 4,
  RANE_X64_RBP = 5,
  RANE_X64_RSI = 6,
  RANE_X64_RDI = 7,

  RANE_X64_R8  = 8,
  RANE_X64_R9  = 9,
  RANE_X64_R10 = 10,
  RANE_X64_R11 = 11,
  RANE_X64_R12 = 12,
  RANE_X64_R13 = 13,
  RANE_X64_R14 = 14,
  RANE_X64_R15 = 15,
} rane_x64_reg_t;

// Win64 calling convention constants (bootstrap).
enum {
  RANE_WIN64_SHADOW_SPACE_BYTES = 32,
  RANE_WIN64_STACK_ALIGN        = 16,
};

// -----------------------------------------------------------------------------
// Condition codes (for SETcc / CMOVcc / Jcc helpers)
//
// NOTE: This mirrors `rane_tir_cc_t` values on purpose so lowering can pass
// the same immediate through without remapping.
// -----------------------------------------------------------------------------
typedef enum rane_x64_cc_e : uint8_t {
  RANE_X64_CC_NE = (uint8_t)TIR_CC_NE,
  RANE_X64_CC_E  = (uint8_t)TIR_CC_E,
  RANE_X64_CC_L  = (uint8_t)TIR_CC_L,
  RANE_X64_CC_LE = (uint8_t)TIR_CC_LE,
  RANE_X64_CC_G  = (uint8_t)TIR_CC_G,
  RANE_X64_CC_GE = (uint8_t)TIR_CC_GE,
} rane_x64_cc_t;

// -----------------------------------------------------------------------------
// Emitter state
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Low-level instruction emitters (bootstrap subset)
// -----------------------------------------------------------------------------
void rane_x64_emit_mov_reg_imm64(rane_x64_emitter_t* e, uint8_t reg, uint64_t imm);
void rane_x64_emit_mov_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_mov_reg_mem(rane_x64_emitter_t* e, uint8_t reg, uint8_t base_reg, int32_t disp);
void rane_x64_emit_mov_mem_reg(rane_x64_emitter_t* e, uint8_t base_reg, int32_t disp, uint8_t reg);

// LEA reg, [RIP + rel32] (for TIR_ADDR_OF)
void rane_x64_emit_lea_reg_rip_rel32(rane_x64_emitter_t* e, uint8_t reg, int32_t rel32);

// Win64 call ABI helpers
void rane_x64_emit_mov_rsp_disp_reg(rane_x64_emitter_t* e, int32_t disp, uint8_t reg);
void rane_x64_emit_sub_rsp_imm8(rane_x64_emitter_t* e, uint8_t imm);
void rane_x64_emit_add_rsp_imm8(rane_x64_emitter_t* e, uint8_t imm);
void rane_x64_emit_call_placeholder(rane_x64_emitter_t* e);

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

void rane_x64_emit_sub_rsp_imm32(rane_x64_emitter_t* e, uint32_t imm);
void rane_x64_emit_add_rsp_imm32(rane_x64_emitter_t* e, uint32_t imm);

void rane_x64_emit_push_reg(rane_x64_emitter_t* e, uint8_t reg);
void rane_x64_emit_pop_reg(rane_x64_emitter_t* e, uint8_t reg);

void rane_x64_emit_xor_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_and_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);
void rane_x64_emit_or_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg);

void rane_x64_emit_shl_reg_cl(rane_x64_emitter_t* e, uint8_t reg);
void rane_x64_emit_shr_reg_cl(rane_x64_emitter_t* e, uint8_t reg);
void rane_x64_emit_sar_reg_cl(rane_x64_emitter_t* e, uint8_t reg);

// -----------------------------------------------------------------------------
// New: unary / flag / booleanization emitters (needed for TIR_NEG/TIR_NOT/TIR_TEST/
//      TIR_SETCC/TIR_CMOVCC)
//
// These are intentionally narrow and bootstrap-friendly.
// -----------------------------------------------------------------------------

// NEG r64 (two's complement): dst = -dst
void rane_x64_emit_neg_reg(rane_x64_emitter_t* e, uint8_t reg);

// NOT r64 (bitwise): dst = ~dst
void rane_x64_emit_not_reg(rane_x64_emitter_t* e, uint8_t reg);

// TEST r/m64, r64  (sets flags; no result)
// Encoding typically: REX.W + 85 /r
void rane_x64_emit_test_reg_reg(rane_x64_emitter_t* e, uint8_t reg1, uint8_t reg2);

// TEST r/m64, imm32 (sets flags; no result)
// Encoding typically: REX.W + F7 /0 id
void rane_x64_emit_test_reg_imm32(rane_x64_emitter_t* e, uint8_t reg, uint32_t imm);

// SETcc r/m8  (writes 0/1 byte)
// Bootstrap convention: write to low 8-bit of the given reg (AL/CL/DL/BL/SPL/BPL/SIL/DIL/r8b..).
void rane_x64_emit_setcc_reg8(rane_x64_emitter_t* e, rane_x64_cc_t cc, uint8_t dst_reg);

// CMOVcc r64, r/m64
// Bootstrap: register-to-register form only.
void rane_x64_emit_cmovcc_reg_reg(rane_x64_emitter_t* e, rane_x64_cc_t cc, uint8_t dst_reg, uint8_t src_reg);

// -----------------------------------------------------------------------------
// New: Indexed addressing / SIB helpers
// Supports [base + index*scale + disp32] forms needed for full `TIR_OPERAND_M`.
// -----------------------------------------------------------------------------

// mov r64, [base + index*scale + disp32]
void rane_x64_emit_mov_reg_mem_sib(
  rane_x64_emitter_t* e,
  uint8_t dst_reg,
  uint8_t base_reg,
  uint8_t index_reg,
  uint8_t scale,
  int32_t disp
);

// mov [base + index*scale + disp32], r64
void rane_x64_emit_mov_mem_sib_reg(
  rane_x64_emitter_t* e,
  uint8_t base_reg,
  uint8_t index_reg,
  uint8_t scale,
  int32_t disp,
  uint8_t src_reg
);

// lea r64, [base + index*scale + disp32]
void rane_x64_emit_lea_reg_mem_sib(
  rane_x64_emitter_t* e,
  uint8_t dst_reg,
  uint8_t base_reg,
  uint8_t index_reg,
  uint8_t scale,
  int32_t disp
);

// -----------------------------------------------------------------------------
// High-level TIR emission
// -----------------------------------------------------------------------------
void rane_x64_emit_tir_inst(rane_x64_emitter_t* e, const rane_tir_inst_t* inst, uint64_t* label_offsets);

rane_error_t rane_x64_emit_module(rane_x64_emitter_t* e, const rane_tir_module_t* mod);
rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx);

// -----------------------------------------------------------------------------
// Fixups / labels (public data structures)
// -----------------------------------------------------------------------------
typedef enum rane_x64_fixup_kind_e : uint8_t {
  RANE_X64_FIXUP_JMP = 1,
  RANE_X64_FIXUP_JE  = 2,
  RANE_X64_FIXUP_JNE = 3,
  RANE_X64_FIXUP_JL  = 4,
  RANE_X64_FIXUP_JLE = 5,
  RANE_X64_FIXUP_JG  = 6,
  RANE_X64_FIXUP_JGE = 7,
  RANE_X64_FIXUP_CALL= 8,
} rane_x64_fixup_kind_t;

typedef struct rane_x64_fixup_s {
  char label[64];
  uint64_t patch_offset; // offset of rel32 immediate (not opcode)
  uint8_t kind;          // rane_x64_fixup_kind_t
} rane_x64_fixup_t;

typedef struct rane_x64_label_map_entry_s {
  char label[64];
  uint64_t offset;
} rane_x64_label_map_entry_t;

// -----------------------------------------------------------------------------
// Header-only convenience helpers (no .cpp changes required)
// -----------------------------------------------------------------------------
static inline uint64_t rane_x64_offset(const rane_x64_emitter_t* e) {
  return e ? e->offset : 0;
}

static inline uint64_t rane_x64_remaining(const rane_x64_emitter_t* e) {
  if (!e) return 0;
  if (e->offset >= e->buffer_size) return 0;
  return e->buffer_size - e->offset;
}

static inline int rane_x64_has_space(const rane_x64_emitter_t* e, uint64_t needed) {
  if (!e) return 0;
  return (e->offset + needed <= e->buffer_size) ? 1 : 0;
}

#ifdef __cplusplus
} // extern "C"
#endif

