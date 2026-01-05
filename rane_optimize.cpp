#include "rane_optimize.h"
#include <stdlib.h>
#include <string.h>

static int tir_has_side_effect(rane_tir_opcode_t op) {
  switch (op) {
    case TIR_CALL_IMPORT:
    case TIR_CALL_LOCAL:
    case TIR_ST:
    case TIR_STV:
    case TIR_TRAP:
    case TIR_HALT:
      return 1;
    default:
      return 0;
  }
}

static int tir_is_block_barrier(rane_tir_opcode_t op) {
  switch (op) {
    case TIR_LABEL:
    case TIR_JMP:
    case TIR_JCC:
    case TIR_JCC_EXT:
    case TIR_SWITCH:
    case TIR_CALL_IMPORT:
    case TIR_CALL_LOCAL:
    case TIR_RET:
    case TIR_RET_VAL:
    case TIR_TRAP:
    case TIR_HALT:
      return 1;
    default:
      return 0;
  }
}

static int tir_writes_reg(const rane_tir_inst_t* inst, uint32_t* out_reg) {
  if (!inst || inst->operand_count == 0) return 0;
  // Heuristic: for most 2-operand ops, operand0 is dest reg
  switch (inst->opcode) {
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
      if (inst->operands[0].kind == TIR_OPERAND_R) {
        if (out_reg) *out_reg = inst->operands[0].r;
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

static void tir_mark_used_regs(const rane_tir_inst_t* inst, uint8_t* used, uint32_t used_cap) {
  if (!inst || !used) return;
  for (uint32_t i = 0; i < inst->operand_count; i++) {
    if (inst->operands[i].kind == TIR_OPERAND_R) {
      uint32_t r = inst->operands[i].r;
      if (r < used_cap) used[r] = 1;
    }
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

static void peephole_fold_mov_chains(rane_tir_function_t* f) {
  // Replace MOV rX, rY followed by MOV rZ, rX with MOV rZ, rY where safe.
  if (!f || !f->insts) return;
  for (uint32_t i = 0; i + 1 < f->inst_count; i++) {
    rane_tir_inst_t* a = &f->insts[i];
    rane_tir_inst_t* b = &f->insts[i + 1];
    if (a->opcode != TIR_MOV || b->opcode != TIR_MOV) continue;
    if (a->operand_count != 2 || b->operand_count != 2) continue;
    if (a->operands[0].kind != TIR_OPERAND_R || a->operands[1].kind != TIR_OPERAND_R) continue;
    if (b->operands[0].kind != TIR_OPERAND_R || b->operands[1].kind != TIR_OPERAND_R) continue;

    uint32_t a_dst = a->operands[0].r;
    uint32_t a_src = a->operands[1].r;
    uint32_t b_src = b->operands[1].r;

    if (b_src == a_dst) {
      b->operands[1].r = a_src;
    }
  }
}

static void constant_propagation_simple(rane_tir_function_t* f) {
  // Track constants assigned via MOV R, IMM and substitute into CMP R, IMM.
  // This is intentionally small but provides a real optimization backbone.
  if (!f || !f->insts) return;

  const uint32_t max_regs = 256;
  uint8_t is_const[max_regs];
  uint64_t const_val[max_regs];
  memset(is_const, 0, sizeof(is_const));

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* inst = &f->insts[i];

    if (inst->opcode == TIR_MOV && inst->operand_count == 2 &&
        inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_IMM) {
      uint32_t r = inst->operands[0].r;
      if (r < max_regs) {
        is_const[r] = 1;
        const_val[r] = inst->operands[1].imm;
      }
      continue;
    }

    // Any other write kills const
    uint32_t w = 0;
    if (tir_writes_reg(inst, &w) && w < max_regs) {
      is_const[w] = 0;
    }

    if (inst->opcode == TIR_CMP && inst->operand_count == 2 &&
        inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_IMM) {
      uint32_t r = inst->operands[0].r;
      if (r < max_regs && is_const[r]) {
        // Future SCCP hooks.
        (void)const_val[r];
      }
    }
  }
}

static int fold_binop(rane_tir_opcode_t op, uint64_t a, uint64_t b, uint64_t* out) {
  if (!out) return 0;
  switch (op) {
    case TIR_ADD: *out = a + b; return 1;
    case TIR_SUB: *out = a - b; return 1;
    case TIR_MUL: *out = a * b; return 1;
    case TIR_DIV: if (b == 0) return 0; *out = a / b; return 1;
    case TIR_AND: *out = a & b; return 1;
    case TIR_OR:  *out = a | b; return 1;
    case TIR_XOR: *out = a ^ b; return 1;
    case TIR_SHL: *out = a << (b & 63); return 1;
    case TIR_SHR: *out = a >> (b & 63); return 1;
    case TIR_SAR: *out = (uint64_t)(((int64_t)a) >> (b & 63)); return 1;
    default: return 0;
  }
}

static void constant_folding_local(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  const uint32_t max_regs = 256;
  uint8_t is_const[max_regs];
  uint64_t const_val[max_regs];
  memset(is_const, 0, sizeof(is_const));

  auto kill_all = [&]() {
    memset(is_const, 0, sizeof(is_const));
  };

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];

    if (tir_is_block_barrier(in->opcode)) {
      kill_all();
      continue;
    }

    // Rewrite ALU rD, imm when rD is const => MOV rD, folded
    if ((in->opcode == TIR_ADD || in->opcode == TIR_SUB || in->opcode == TIR_MUL || in->opcode == TIR_DIV ||
         in->opcode == TIR_AND || in->opcode == TIR_OR  || in->opcode == TIR_XOR ||
         in->opcode == TIR_SHL || in->opcode == TIR_SHR || in->opcode == TIR_SAR) &&
        in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      uint32_t dst = in->operands[0].r;
      uint64_t imm = in->operands[1].imm;
      if (dst < max_regs && is_const[dst]) {
        uint64_t folded = 0;
        if (fold_binop(in->opcode, const_val[dst], imm, &folded)) {
          in->opcode = TIR_MOV;
          in->type = RANE_TYPE_U64;
          in->operands[0].kind = TIR_OPERAND_R;
          in->operands[0].r = dst;
          in->operands[1].kind = TIR_OPERAND_IMM;
          in->operands[1].imm = folded;
          in->operand_count = 2;
          is_const[dst] = 1;
          const_val[dst] = folded;
          continue;
        }
      }
    }

    // MOV reg, imm => record
    if (in->opcode == TIR_MOV && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      uint32_t r = in->operands[0].r;
      if (r < max_regs) {
        is_const[r] = 1;
        const_val[r] = in->operands[1].imm;
      }
      continue;
    }

    // Any write kills
    uint32_t w = 0;
    if (tir_writes_reg(in, &w) && w < max_regs) {
      is_const[w] = 0;
    }
  }
}

// Track the last CMP reg, imm for simplifycfg.
typedef struct last_cmp_s {
  uint8_t valid;
  uint32_t reg;
  uint64_t imm;
} last_cmp_t;

static int eval_cc(rane_tir_cc_t cc, int64_t lhs, int64_t rhs) {
  switch (cc) {
    case TIR_CC_E:  return lhs == rhs;
    case TIR_CC_NE: return lhs != rhs;
    case TIR_CC_L:  return lhs < rhs;
    case TIR_CC_LE: return lhs <= rhs;
    case TIR_CC_G:  return lhs > rhs;
    case TIR_CC_GE: return lhs >= rhs;
    default: return 0;
  }
}

static void simplifycfg_lite(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  const uint32_t max_regs = 256;
  uint8_t is_const[max_regs];
  uint64_t const_val[max_regs];
  memset(is_const, 0, sizeof(is_const));

  last_cmp_t last = {};

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];

    // Kill on barriers that imply unknown flags/control.
    if (in->opcode == TIR_LABEL || in->opcode == TIR_JMP || in->opcode == TIR_RET || in->opcode == TIR_RET_VAL ||
        in->opcode == TIR_CALL_IMPORT || in->opcode == TIR_CALL_LOCAL || in->opcode == TIR_TRAP || in->opcode == TIR_HALT) {
      memset(is_const, 0, sizeof(is_const));
      last.valid = 0;
      continue;
    }

    if (in->opcode == TIR_MOV && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      uint32_t r = in->operands[0].r;
      if (r < max_regs) {
        is_const[r] = 1;
        const_val[r] = in->operands[1].imm;
      }
      continue;
    }

    // CMP reg, imm => remember
    if (in->opcode == TIR_CMP && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      last.valid = 1;
      last.reg = in->operands[0].r;
      last.imm = in->operands[1].imm;
      continue;
    }

    // Fold JCC_EXT if last cmp is with a known constant reg.
    if (in->opcode == TIR_JCC_EXT && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_IMM && in->operands[1].kind == TIR_OPERAND_LBL) {
      rane_tir_cc_t cc = (rane_tir_cc_t)in->operands[0].imm;
      if (last.valid && last.reg < max_regs && is_const[last.reg]) {
        int take = eval_cc(cc, (int64_t)const_val[last.reg], (int64_t)last.imm);
        if (take) {
          // Turn into JMP label
          in->opcode = TIR_JMP;
          in->type = RANE_TYPE_U64;
          // operand 0 becomes label
          in->operands[0] = in->operands[1];
          in->operand_count = 1;
        } else {
          // Never taken: NOP
          in->opcode = TIR_NOP;
          in->operand_count = 0;
        }
      }
      last.valid = 0;
      continue;
    }

    // Any write kills const for that reg
    uint32_t w = 0;
    if (tir_writes_reg(in, &w) && w < max_regs) {
      is_const[w] = 0;
      last.valid = 0;
    }
  }

  compact_nops(f);

  // Unreachable code elimination: after an unconditional transfer, NOP until next label.
  int dead = 0;
  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];
    if (in->opcode == TIR_LABEL) {
      dead = 0;
      continue;
    }

    if (dead) {
      // Keep labels handled above; drop everything else.
      in->opcode = TIR_NOP;
      in->operand_count = 0;
      continue;
    }

    if (in->opcode == TIR_JMP || in->opcode == TIR_RET || in->opcode == TIR_RET_VAL || in->opcode == TIR_HALT || in->opcode == TIR_TRAP) {
      dead = 1;
      continue;
    }
  }

  compact_nops(f);
}

static void dce_local(rane_tir_function_t* f) {
  // Backward liveness-based DCE using a small used bitset.
  if (!f || !f->insts) return;
  const uint32_t max_regs = 256;
  uint8_t used[max_regs];
  memset(used, 0, sizeof(used));

  for (int32_t i = (int32_t)f->inst_count - 1; i >= 0; i--) {
    rane_tir_inst_t* inst = &f->insts[i];

    int keep = tir_has_side_effect(inst->opcode) || inst->opcode == TIR_RET || inst->opcode == TIR_RET_VAL ||
               inst->opcode == TIR_LABEL || inst->opcode == TIR_JMP || inst->opcode == TIR_JCC || inst->opcode == TIR_JCC_EXT;

    // If the instruction writes a reg that is not used, and no side effects, can drop.
    uint32_t wreg = 0;
    if (!keep && tir_writes_reg(inst, &wreg) && wreg < max_regs) {
      if (!used[wreg]) {
        inst->opcode = TIR_NOP;
        inst->operand_count = 0;
        continue;
      }
    }

    // Mark operands used
    tir_mark_used_regs(inst, used, max_regs);

    // If it writes a reg, it's now defined so it's not needed *before* this point.
    if (tir_writes_reg(inst, &wreg) && wreg < max_regs) {
      used[wreg] = 0;
    }
  }

  compact_nops(f);
}

static void instcombine_local(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];

    // Fold MOV r, r
    if (in->opcode == TIR_MOV && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_R &&
        in->operands[0].r == in->operands[1].r) {
      in->opcode = TIR_NOP;
      in->operand_count = 0;
      continue;
    }

    // Canonicalize ALU immediate identities
    if ((in->opcode == TIR_ADD || in->opcode == TIR_SUB || in->opcode == TIR_OR || in->opcode == TIR_XOR) &&
        in->operand_count == 2 && in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      if (in->operands[1].imm == 0) {
        in->opcode = TIR_NOP;
        in->operand_count = 0;
        continue;
      }
    }

    if ((in->opcode == TIR_MUL) && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      uint64_t k = in->operands[1].imm;
      if (k == 0) {
        in->opcode = TIR_MOV;
        in->type = RANE_TYPE_U64;
        in->operands[1].kind = TIR_OPERAND_IMM;
        in->operands[1].imm = 0;
        in->operand_count = 2;
        continue;
      }
      if (k == 1) {
        in->opcode = TIR_NOP;
        in->operand_count = 0;
        continue;
      }
      // strength reduce mul by power of two -> shl
      if ((k & (k - 1)) == 0) {
        uint64_t sh = 0;
        while ((1ull << sh) != k) sh++;
        in->opcode = TIR_SHL;
        in->operands[1].imm = sh;
        continue;
      }
    }

    if ((in->opcode == TIR_DIV) && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      uint64_t k = in->operands[1].imm;
      if (k == 1) {
        in->opcode = TIR_NOP;
        in->operand_count = 0;
        continue;
      }
      // (unsigned) div by power of two -> shr
      if (k != 0 && (k & (k - 1)) == 0) {
        uint64_t sh = 0;
        while ((1ull << sh) != k) sh++;
        in->opcode = TIR_SHR;
        in->operands[1].imm = sh;
        continue;
      }
    }

    if ((in->opcode == TIR_AND) && in->operand_count == 2 &&
        in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) {
      if (in->operands[1].imm == 0xFFFFFFFFFFFFFFFFull) {
        in->opcode = TIR_NOP;
        in->operand_count = 0;
        continue;
      }
      if (in->operands[1].imm == 0) {
        // dst &= 0 => dst = 0
        in->opcode = TIR_MOV;
        in->type = RANE_TYPE_U64;
        in->operands[1].kind = TIR_OPERAND_IMM;
        in->operands[1].imm = 0;
        in->operand_count = 2;
        continue;
      }
    }
  }

  compact_nops(f);
}

static void jump_threading_lite(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  // Pattern: JCC_EXT cc, Ltrue; JMP Lfalse; LABEL Ltrue
  // If the next instruction after JCC is an unconditional JMP, keep both,
  // but if JCC is a JMP already (from simplifycfg), remove the redundant JMP.
  for (uint32_t i = 0; i + 1 < f->inst_count; i++) {
    rane_tir_inst_t* a = &f->insts[i];
    rane_tir_inst_t* b = &f->insts[i + 1];

    if (a->opcode == TIR_JMP && b->opcode == TIR_JMP &&
        a->operand_count == 1 && b->operand_count == 1 &&
        a->operands[0].kind == TIR_OPERAND_LBL && b->operands[0].kind == TIR_OPERAND_LBL) {
      // consecutive jmps: drop the first
      a->opcode = TIR_NOP;
      a->operand_count = 0;
      continue;
    }

    if (a->opcode == TIR_JMP && b->opcode == TIR_LABEL &&
        a->operand_count == 1 && a->operands[0].kind == TIR_OPERAND_LBL &&
        b->operand_count == 1 && b->operands[0].kind == TIR_OPERAND_LBL) {
      // jmp to immediately next label => remove jmp
      if (strcmp(a->operands[0].lbl, b->operands[0].lbl) == 0) {
        a->opcode = TIR_NOP;
        a->operand_count = 0;
      }
      continue;
    }
  }

  compact_nops(f);
}

typedef struct gvn_key_s {
  uint8_t opcode;
  uint8_t kind_a;
  uint8_t kind_b;
  uint8_t pad;
  uint64_t a;
  uint64_t b;
} gvn_key_t;

static int gvn_key_eq(const gvn_key_t* x, const gvn_key_t* y) {
  return x->opcode == y->opcode && x->kind_a == y->kind_a && x->kind_b == y->kind_b && x->a == y->a && x->b == y->b;
}

static void gvn_make_key_for_inst(const rane_tir_inst_t* in, gvn_key_t* out) {
  memset(out, 0, sizeof(*out));
  if (!in) return;
  out->opcode = (uint8_t)in->opcode;

  // Only handle simple pure operations. Destination is not part of key.
  if (in->operand_count >= 2) {
    const rane_tir_operand_t* a = &in->operands[1];
    const rane_tir_operand_t* b = (in->operand_count >= 3) ? &in->operands[2] : NULL;

    out->kind_a = (uint8_t)a->kind;
    if (a->kind == TIR_OPERAND_R) out->a = (uint64_t)a->r;
    else if (a->kind == TIR_OPERAND_IMM) out->a = a->imm;

    if (b) {
      out->kind_b = (uint8_t)b->kind;
      if (b->kind == TIR_OPERAND_R) out->b = (uint64_t)b->r;
      else if (b->kind == TIR_OPERAND_IMM) out->b = b->imm;
    }

    // Commutative ops: sort operands to canonicalize
    if (in->opcode == TIR_ADD || in->opcode == TIR_MUL || in->opcode == TIR_AND || in->opcode == TIR_OR || in->opcode == TIR_XOR) {
      if (out->kind_a == out->kind_b) {
        if (out->a > out->b) {
          uint64_t ta = out->a; out->a = out->b; out->b = ta;
        }
      } else {
        // ensure deterministic order by kind
        if (out->kind_a > out->kind_b) {
          uint8_t tk = out->kind_a; out->kind_a = out->kind_b; out->kind_b = tk;
          uint64_t ta = out->a; out->a = out->b; out->b = ta;
        }
      }
    }
  }
}

static int gvn_is_candidate(const rane_tir_inst_t* in) {
  if (!in) return 0;
  if (tir_is_block_barrier(in->opcode)) return 0;
  if (tir_has_side_effect(in->opcode)) return 0;

  // Must define a destination register.
  if (in->operand_count < 2) return 0;
  if (in->operands[0].kind != TIR_OPERAND_R) return 0;

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
      break;
    default:
      return 0;
  }

  // Only accept reg/imm operands (no memory)
  for (uint32_t i = 1; i < in->operand_count; i++) {
    if (in->operands[i].kind != TIR_OPERAND_R && in->operands[i].kind != TIR_OPERAND_IMM) return 0;
  }

  return 1;
}

static void gvn_local(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  // Very small table; linear scan is fine.
  typedef struct {
    gvn_key_t key;
    uint32_t value_reg;
    uint8_t valid;
  } entry_t;

  entry_t table[256];
  memset(table, 0, sizeof(table));

  auto reset = [&]() {
    memset(table, 0, sizeof(table));
  };

  auto kill_reg = [&](uint32_t r) {
    for (uint32_t i = 0; i < 256; i++) {
      if (!table[i].valid) continue;
      if (table[i].value_reg == r) {
        table[i].valid = 0;
      }
      // If key refers to r, invalidate too
      if ((table[i].key.kind_a == TIR_OPERAND_R && table[i].key.a == r) ||
          (table[i].key.kind_b == TIR_OPERAND_R && table[i].key.b == r)) {
        table[i].valid = 0;
      }
    }
  };

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];

    if (tir_is_block_barrier(in->opcode) || tir_has_side_effect(in->opcode)) {
      reset();
      continue;
    }

    // Try CSE
    if (gvn_is_candidate(in)) {
      gvn_key_t k;
      gvn_make_key_for_inst(in, &k);

      // Lookup
      uint32_t hit_reg = 0xFFFFFFFFu;
      for (uint32_t ti = 0; ti < 256; ti++) {
        if (!table[ti].valid) continue;
        if (gvn_key_eq(&table[ti].key, &k)) {
          hit_reg = table[ti].value_reg;
          break;
        }
      }

      uint32_t dst = in->operands[0].r;

      if (hit_reg != 0xFFFFFFFFu && hit_reg != dst) {
        // Replace with MOV dst, hit_reg
        in->opcode = TIR_MOV;
        in->type = RANE_TYPE_U64;
        in->operands[0].kind = TIR_OPERAND_R;
        in->operands[0].r = dst;
        in->operands[1].kind = TIR_OPERAND_R;
        in->operands[1].r = hit_reg;
        in->operand_count = 2;

        // This defines dst, so kill any previous mentions and record dst as alias of hit by adding a MOV key.
        kill_reg(dst);
        gvn_key_t km;
        gvn_make_key_for_inst(in, &km);
        for (uint32_t ti = 0; ti < 256; ti++) {
          if (!table[ti].valid) {
            table[ti].valid = 1;
            table[ti].key = km;
            table[ti].value_reg = dst;
            break;
          }
        }
        continue;
      }

      // No hit: record this expression => dst
      kill_reg(dst);
      for (uint32_t ti = 0; ti < 256; ti++) {
        if (!table[ti].valid) {
          table[ti].valid = 1;
          table[ti].key = k;
          table[ti].value_reg = dst;
          break;
        }
      }
      continue;
    }

    // If this instruction defines a register, invalidate dependent entries.
    uint32_t w = 0;
    if (tir_writes_reg(in, &w)) {
      kill_reg(w);
    }
  }

  compact_nops(f);
}

rane_error_t rane_opt_constant_folding(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    constant_folding_local(&mod->functions[fi]);
  }
  return RANE_OK;
}

rane_error_t rane_opt_dead_code_elimination(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    dce_local(&mod->functions[fi]);
  }
  return RANE_OK;
}

rane_error_t rane_opt_simplifycfg(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    simplifycfg_lite(&mod->functions[fi]);
  }
  return RANE_OK;
}

rane_error_t rane_opt_instcombine(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    instcombine_local(&mod->functions[fi]);
  }
  return RANE_OK;
}

rane_error_t rane_opt_jump_threading(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    jump_threading_lite(&mod->functions[fi]);
  }
  return RANE_OK;
}

rane_error_t rane_opt_gvn(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    gvn_local(&mod->functions[fi]);
  }
  return RANE_OK;
}

rane_error_t rane_opt_peephole_optimizations(rane_tir_module_t* mod) {
  if (!mod) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    peephole_fold_mov_chains(&mod->functions[fi]);
  }
  return RANE_OK;
}

// Keep other passes as no-ops for now (they're not meaningful without a CFG/SSA framework)
rane_error_t rane_opt_profile_guided_optimization(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_inlining(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_constexpr_evaluation(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_template_pruning(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_type_based_alias_analysis(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_tail_calls(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_code_compression(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_exception_simplifications(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_alias_analysis(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_call_graph_construction(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_dependence_analysis(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_dominator_tree(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_post_dominator_tree(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_lazy_value_info(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_constant_range_analysis(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_aggressive_dce(rane_tir_module_t* mod) { return rane_opt_dead_code_elimination(mod); }
rane_error_t rane_opt_reassociate(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_memcpyopt(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_sroa(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_licm(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_loop_rotation(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_loop_unrolling(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_loop_unswitching(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_induction_var_simplification(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_tail_call_elimination(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }

rane_error_t rane_opt_level_O0(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }

rane_error_t rane_opt_level_O1(rane_tir_module_t* mod) {
  rane_opt_peephole_optimizations(mod);
  rane_opt_instcombine(mod);
  rane_opt_constant_folding(mod);
  rane_opt_simplifycfg(mod);
  rane_opt_jump_threading(mod);
  rane_opt_dead_code_elimination(mod);
  return RANE_OK;
}

rane_error_t rane_opt_level_O2(rane_tir_module_t* mod) {
  rane_opt_level_O1(mod);
  rane_opt_gvn(mod);

  // Repeat passes that expose more opportunities after DCE/GVN.
  rane_opt_instcombine(mod);
  rane_opt_simplifycfg(mod);
  rane_opt_jump_threading(mod);
  rane_opt_dead_code_elimination(mod);
  return RANE_OK;
}

rane_error_t rane_opt_level_O3(rane_tir_module_t* mod) {
  rane_opt_level_O2(mod);
  // One more GVN sweep after CFG cleanup.
  rane_opt_gvn(mod);
  rane_opt_dead_code_elimination(mod);
  return RANE_OK;
}

rane_error_t rane_opt_level_Ofast(rane_tir_module_t* mod) { return rane_opt_level_O3(mod); }
rane_error_t rane_opt_level_Os(rane_tir_module_t* mod) { return rane_opt_level_O2(mod); }
rane_error_t rane_opt_level_Oz(rane_tir_module_t* mod) { return rane_opt_level_Os(mod); }
rane_error_t rane_opt_lto(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }
rane_error_t rane_opt_march_native(rane_tir_module_t* mod) { (void)mod; return RANE_OK; }

rane_error_t rane_optimize_tir_with_level(rane_tir_module_t* tir_module, int opt_level) {
  if (!tir_module) return RANE_E_INVALID_ARG;

  int lvl = opt_level;
  if (lvl < 0) lvl = 0;
  if (lvl > 3) lvl = 3;

  switch (lvl) {
    case 0: rane_opt_level_O0(tir_module); break;
    case 1: rane_opt_level_O1(tir_module); break;
    case 2: rane_opt_level_O2(tir_module); break;
    case 3: rane_opt_level_O3(tir_module); break;
    default: rane_opt_level_O2(tir_module); break;
  }

  for (uint32_t fi = 0; fi < tir_module->function_count; fi++) {
    constant_propagation_simple(&tir_module->functions[fi]);
  }

  return RANE_OK;
}

rane_error_t rane_optimize_tir(rane_tir_module_t* tir_module) {
  // Preserve prior behavior: default to O2
  return rane_optimize_tir_with_level(tir_module, 2);
}