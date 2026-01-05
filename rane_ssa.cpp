#include "rane_ssa.h"
#include <stdlib.h>
#include <string.h>

// SSA in this project cannot freely rename registers because low-numbered
// registers are used as ABI-fixed physical registers by the bootstrap x64 backend
// (e.g., r0=RAX, r1=RCX, r2=RDX, r8=R8, r9=R9).
//
// Instead of doing full SSA renaming, we implement a safe SSA-ish pass that:
//  - removes redundant self-moves
//  - performs local copy-propagation within straight-line regions
//  - keeps control-flow boundaries conservative (labels/jumps/calls)

static int inst_is_barrier(const rane_tir_inst_t* in) {
  if (!in) return 1;
  switch (in->opcode) {
    case TIR_LABEL:
    case TIR_JMP:
    case TIR_JCC:
    case TIR_JCC_EXT:
    case TIR_SWITCH:
    case TIR_CALL_LOCAL:
    case TIR_CALL_IMPORT:
    case TIR_RET:
    case TIR_RET_VAL:
    case TIR_TRAP:
    case TIR_HALT:
      return 1;
    default:
      return 0;
  }
}

static int inst_defines_reg0(const rane_tir_inst_t* in, uint32_t* out_r) {
  if (!in || in->operand_count == 0) return 0;
  // Convention in this IR: for most ops, operand 0 is the destination.
  switch (in->opcode) {
    case TIR_MOV:
    case TIR_ADD:
    case TIR_SUB:
    case TIR_MUL:
    case TIR_DIV:
    case TIR_AND:
    case TIR_OR:
    case TIR_XOR:
    case TIR_SHL:
    case TIR_SHR:
    case TIR_SAR:
    case TIR_LD:
    case TIR_LDV:
      if (in->operands[0].kind == TIR_OPERAND_R) {
        if (out_r) *out_r = in->operands[0].r;
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

static void compact_nops(rane_tir_function_t* f) {
  if (!f || !f->insts) return;
  uint32_t w = 0;
  for (uint32_t i = 0; i < f->inst_count; i++) {
    if (f->insts[i].opcode == TIR_NOP) continue;
    if (w != i) f->insts[w] = f->insts[i];
    w++;
  }
  f->inst_count = w;
}

static void local_copy_prop(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  const uint32_t max_regs = 256;
  uint32_t map[max_regs];
  uint8_t valid[max_regs];

  auto reset_map = [&]() {
    for (uint32_t i = 0; i < max_regs; i++) {
      map[i] = i;
      valid[i] = 0;
    }
  };

  reset_map();

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];

    if (inst_is_barrier(in)) {
      reset_map();
      continue;
    }

    // Rewrite uses (skip operand0 if it's a write)
    uint32_t defr = 0;
    int has_def = inst_defines_reg0(in, &defr);

    for (uint32_t oi = 0; oi < in->operand_count; oi++) {
      if (has_def && oi == 0) continue;
      if (in->operands[oi].kind != TIR_OPERAND_R) continue;

      uint32_t r = in->operands[oi].r;
      if (r < max_regs && valid[r]) {
        in->operands[oi].r = map[r];
      }
    }

    // Kill mapping on defs
    if (has_def && defr < max_regs) {
      // definition breaks any previous aliasing of defr
      valid[defr] = 0;
      map[defr] = defr;

      // MOV dst, src creates a copy we can propagate.
      if (in->opcode == TIR_MOV && in->operand_count == 2 &&
          in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_R) {
        uint32_t dst = in->operands[0].r;
        uint32_t src = in->operands[1].r;

        // Canonicalize through existing map.
        if (src < max_regs && valid[src]) src = map[src];

        if (dst == src) {
          in->opcode = TIR_NOP;
          in->operand_count = 0;
        } else {
          valid[dst] = 1;
          map[dst] = src;
        }
      }
    }
  }

  compact_nops(f);
}

rane_error_t rane_build_ssa(rane_tir_module_t* tir_module) {
  if (!tir_module) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < tir_module->function_count; fi++) {
    local_copy_prop(&tir_module->functions[fi]);
  }
  return RANE_OK;
}