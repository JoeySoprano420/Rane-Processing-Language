#include "rane_ssa.h"
#include <stdlib.h>
#include <string.h>

// Basic SSA construction: rename variables to SSA form
// For simplicity, assign unique SSA numbers to each use/def

typedef struct ssa_var_s {
  char name[64];
  int ssa_num;
} ssa_var_t;

static int ssa_counter = 0;

static void rename_vars(rane_tir_function_t* func) {
  // Simple renaming: for each instruction, assign new SSA numbers
  for (uint32_t i = 0; i < func->inst_count; i++) {
    rane_tir_inst_t* inst = &func->insts[i];
    // For operands that are registers, assign new SSA num
    // This is a placeholder; real SSA needs dominance, etc.
    if (inst->operands[0].kind == TIR_OPERAND_R) {
      inst->operands[0].r = ssa_counter++;
    }
    if (inst->operand_count > 1 && inst->operands[1].kind == TIR_OPERAND_R) {
      inst->operands[1].r = ssa_counter++;
    }
  }
}

rane_error_t rane_build_ssa(rane_tir_module_t* tir_module) {
  for (uint32_t f = 0; f < tir_module->function_count; f++) {
    rename_vars(&tir_module->functions[f]);
  }
  return RANE_OK;
}