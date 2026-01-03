#include "rane_regalloc.h"
#include <stdlib.h>

// Simple linear scan register allocation for x64
// Assign physical registers to virtual SSA regs

#define MAX_REGS 16  // x64 GPRs

typedef struct reg_interval_s {
  int vreg;
  int start;
  int end;
  int phys_reg;
} reg_interval_t;

static int phys_regs[MAX_REGS] = {0}; // 0 = free

static int alloc_reg() {
  for (int i = 0; i < MAX_REGS; i++) {
    if (!phys_regs[i]) {
      phys_regs[i] = 1;
      return i;
    }
  }
  return -1; // spill
}

rane_error_t rane_allocate_registers(rane_tir_module_t* tir_module) {
  for (uint32_t f = 0; f < tir_module->function_count; f++) {
    rane_tir_function_t* func = &tir_module->functions[f];
    for (uint32_t i = 0; i < func->inst_count; i++) {
      rane_tir_inst_t* inst = &func->insts[i];
      if (inst->operands[0].kind == TIR_OPERAND_R && inst->operands[0].r >= 0) {
        int phys = alloc_reg();
        if (phys == -1) phys = 0; // spill to RAX
        inst->operands[0].r = phys;
      }
      if (inst->operand_count > 1 && inst->operands[1].kind == TIR_OPERAND_R && inst->operands[1].r >= 0) {
        int phys = alloc_reg();
        if (phys == -1) phys = 1; // spill to RCX
        inst->operands[1].r = phys;
      }
    }
  }
  return RANE_OK;
}