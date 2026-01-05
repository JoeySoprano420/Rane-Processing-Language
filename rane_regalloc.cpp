#include "rane_regalloc.h"
#include <string.h>

// The bootstrap backend treats TIR `TIR_OPERAND_R` indices as *physical x64 regs*
// (0=RAX,1=RCX,2=RDX,4=RSP,5=RBP,8=R8,9=R9,...). Because of that, we cannot run
// a traditional reg allocator that renames everything.
//
// However, lowering sometimes uses temporary virtual regs above the usual ABI set.
// This pass does a conservative block-local remap of those temporaries into a
// small scratch register set, avoiding the ABI argument registers and avoiding
// crossing barriers (calls/labels/jumps).

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

static void remap_block_temporaries(rane_tir_function_t* f) {
  if (!f || !f->insts) return;

  // Scratch set: R10-R15 (10..15) are caller-saved on Win64 and not used by our
  // fixed-ABI argument plumbing.
  const uint32_t scratch[6] = {10, 11, 12, 13, 14, 15};
  const uint32_t scratch_count = 6;

  const uint32_t max_regs = 256;
  uint32_t map[max_regs];
  uint8_t used[max_regs];

  auto reset = [&]() {
    memset(map, 0xFF, sizeof(map));
    memset(used, 0, sizeof(used));
  };

  reset();

  auto phys_is_reserved = [&](uint32_t r) -> int {
    // Never remap these.
    if (r == 0) return 1; // RAX return
    if (r == 1 || r == 2 || r == 8 || r == 9) return 1; // arg regs
    if (r == 4 || r == 5) return 1; // RSP/RBP
    return 0;
  };

  auto alloc_scratch = [&]() -> uint32_t {
    for (uint32_t i = 0; i < scratch_count; i++) {
      if (!used[scratch[i]]) {
        used[scratch[i]] = 1;
        return scratch[i];
      }
    }
    return 0xFFFFFFFFu;
  };

  for (uint32_t i = 0; i < f->inst_count; i++) {
    rane_tir_inst_t* in = &f->insts[i];

    if (inst_is_barrier(in)) {
      reset();
      continue;
    }

    // Remap all register operands through map if present.
    for (uint32_t oi = 0; oi < in->operand_count; oi++) {
      if (in->operands[oi].kind != TIR_OPERAND_R) continue;
      uint32_t r = in->operands[oi].r;
      if (r < max_regs && map[r] != 0xFFFFFFFFu) {
        in->operands[oi].r = map[r];
      }
    }

    // If instruction writes to a high non-reserved reg, remap it to scratch.
    // Conservative heuristic: only for MOV/ALU ops where operand0 is a reg dest.
    if (in->operand_count >= 1 && in->operands[0].kind == TIR_OPERAND_R) {
      uint32_t dst = in->operands[0].r;
      if (dst < max_regs && !phys_is_reserved(dst) && dst >= 16) {
        if (map[dst] == 0xFFFFFFFFu) {
          uint32_t s = alloc_scratch();
          if (s != 0xFFFFFFFFu) {
            map[dst] = s;
            in->operands[0].r = s;
          }
        } else {
          in->operands[0].r = map[dst];
        }
      }
    }
  }
}

rane_error_t rane_allocate_registers(rane_tir_module_t* tir_module) {
  if (!tir_module) return RANE_E_INVALID_ARG;
  for (uint32_t fi = 0; fi < tir_module->function_count; fi++) {
    remap_block_temporaries(&tir_module->functions[fi]);
  }
  return RANE_OK;
}