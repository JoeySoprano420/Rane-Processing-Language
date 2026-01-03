#include "rane_tir.h"
#include "rane_x64.h"
#include "rane_ast.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration
static void lower_expr_to_tir(rane_expr_t* expr, rane_tir_function_t* func);

// Lowering AST to TIR
rane_error_t rane_lower_ast_to_tir(const rane_stmt_t* ast_root, rane_tir_module_t* tir_module) {
  if (!ast_root || !tir_module) return RANE_E_INVALID_ARG;

  // Initialize module
  tir_module->name[0] = 0; // default
  tir_module->functions = (rane_tir_function_t*)malloc(sizeof(rane_tir_function_t));
  tir_module->function_count = 1;
  tir_module->max_functions = 1;
  rane_tir_function_t* func = &tir_module->functions[0];
  strcpy_s(func->name, sizeof(func->name), "main");
  func->insts = (rane_tir_inst_t*)malloc(sizeof(rane_tir_inst_t) * 10); // arbitrary
  func->inst_count = 0;
  func->max_insts = 10;

  // Lower the statement
  if (ast_root->kind == STMT_LET) {
    // For let x = expr;
    lower_expr_to_tir(ast_root->let.expr, func);
    // Assume result in reg 0
  } else if (ast_root->kind == STMT_ASSIGN) {
    // Similar
    lower_expr_to_tir(ast_root->assign.expr, func);
  } else if (ast_root->kind == STMT_JUMP) {
    // JMP to label
    rane_tir_inst_t inst;
    inst.opcode = TIR_JMP;
    inst.type = RANE_TYPE_U64;
    inst.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), ast_root->jump.marker);
    inst.operand_count = 1;
    func->insts[func->inst_count++] = inst;
  } else if (ast_root->kind == STMT_MARKER) {
    // Label
    rane_tir_inst_t inst;
    inst.opcode = TIR_LABEL;
    inst.type = RANE_TYPE_U64;
    inst.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), ast_root->marker.name);
    inst.operand_count = 1;
    func->insts[func->inst_count++] = inst;
  } else if (ast_root->kind == STMT_IF) {
    // Lower cond, then branches
    lower_expr_to_tir(ast_root->if_stmt.cond, func);
    // Assume cond result in reg 0, CMP reg0, 0, JNE then_label
    rane_tir_inst_t cmp_inst;
    cmp_inst.opcode = TIR_CMP;
    cmp_inst.type = RANE_TYPE_U64;
    cmp_inst.operands[0].kind = TIR_OPERAND_R;
    cmp_inst.operands[0].r = 0;
    cmp_inst.operands[1].kind = TIR_OPERAND_IMM;
    cmp_inst.operands[1].imm = 0;
    cmp_inst.operand_count = 2;
    func->insts[func->inst_count++] = cmp_inst;

    // JNE to then
    rane_tir_inst_t jne_inst;
    jne_inst.opcode = TIR_JCC; // assume JCC for !=
    jne_inst.type = RANE_TYPE_U64;
    jne_inst.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(jne_inst.operands[0].lbl, sizeof(jne_inst.operands[0].lbl), "then");
    jne_inst.operand_count = 1;
    func->insts[func->inst_count++] = jne_inst;

    // Else branch if exists
    if (ast_root->if_stmt.else_branch) {
      // Lower else
      // For simplicity, just lower the statements
    }
    // Then branch
    // Lower then
  } else if (ast_root->kind == STMT_WHILE) {
    // Similar for while
  }

  // Add RET
  rane_tir_inst_t ret_inst;
  ret_inst.opcode = TIR_RET;
  ret_inst.type = RANE_TYPE_U64;
  ret_inst.operand_count = 0;
  func->insts[func->inst_count++] = ret_inst;

  return RANE_OK;
}

// Helper to lower expression
static void lower_expr_to_tir(rane_expr_t* expr, rane_tir_function_t* func) {
  if (expr->kind == EXPR_LIT_INT) {
    rane_tir_inst_t inst;
    inst.opcode = TIR_MOV;
    inst.type = RANE_TYPE_U64;
    inst.operands[0].kind = TIR_OPERAND_R;
    inst.operands[0].r = 0; // result reg
    inst.operands[1].kind = TIR_OPERAND_IMM;
    inst.operands[1].imm = expr->lit_int.value;
    inst.operand_count = 2;
    func->insts[func->inst_count++] = inst;
  } else if (expr->kind == EXPR_VAR) {
    // Assume var in reg 1
    rane_tir_inst_t inst;
    inst.opcode = TIR_MOV;
    inst.type = RANE_TYPE_U64;
    inst.operands[0].kind = TIR_OPERAND_R;
    inst.operands[0].r = 0;
    inst.operands[1].kind = TIR_OPERAND_R;
    inst.operands[1].r = 1;
    inst.operand_count = 2;
    func->insts[func->inst_count++] = inst;
  } else if (expr->kind == EXPR_BINARY) {
    lower_expr_to_tir(expr->binary.left, func);
    // Assume left in reg 0
    lower_expr_to_tir(expr->binary.right, func);
    // Assume right in reg 1, result in reg 0
    rane_tir_inst_t inst;
    rane_tir_opcode_t opcode;
    if (expr->binary.op == BIN_ADD) opcode = TIR_ADD;
    else if (expr->binary.op == BIN_SUB) opcode = TIR_SUB;
    else if (expr->binary.op == BIN_MUL) opcode = TIR_MUL;
    else if (expr->binary.op == BIN_DIV) opcode = TIR_DIV;
    else opcode = TIR_ADD;
    inst.opcode = opcode;
    inst.type = RANE_TYPE_U64;
    inst.operands[0].kind = TIR_OPERAND_R;
    inst.operands[0].r = 0;
    inst.operands[1].kind = TIR_OPERAND_R;
    inst.operands[1].r = 1;
    inst.operand_count = 2;
    func->insts[func->inst_count++] = inst;
  }
}

// Code generation TIR to x64
rane_error_t rane_codegen_tir_to_x64(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx) {
  if (!tir_module || !ctx || !ctx->code_buffer) return RANE_E_INVALID_ARG;

  rane_x64_emitter_t emitter;
  rane_x64_init_emitter(&emitter, ctx->code_buffer, ctx->buffer_size);

  // For simplicity, emit for the first function
  if (tir_module->function_count > 0) {
    const rane_tir_function_t* func = &tir_module->functions[0];
    for (uint32_t i = 0; i < func->inst_count; i++) {
      const rane_tir_inst_t* inst = &func->insts[i];
      // Simple mapping
      if (inst->opcode == TIR_MOV && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_IMM) {
        rane_x64_emit_mov_reg_imm64(&emitter, inst->operands[0].r, inst->operands[1].imm);
      } else if (inst->opcode == TIR_JMP && inst->operand_count == 1 &&
                 inst->operands[0].kind == TIR_OPERAND_LBL) {
        rane_x64_emit_jmp_rel32(&emitter, 0); // placeholder
      } else if (inst->opcode == TIR_JCC && inst->operand_count == 1 &&
                 inst->operands[0].kind == TIR_OPERAND_LBL) {
        rane_x64_emit_jne_rel32(&emitter, 0); // assume JNE
      } else if (inst->opcode == TIR_CMP && inst->operand_count == 2) {
        // rane_x64_emit_cmp_reg_imm(&emitter, inst->operands[0].r, inst->operands[1].imm);
      } else if (inst->opcode == TIR_ADD && inst->operand_count == 2 &&
                 inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_add_reg_reg(&emitter, inst->operands[0].r, inst->operands[1].r);
      } else if (inst->opcode == TIR_SUB && inst->operand_count == 2 &&
                 inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_sub_reg_reg(&emitter, inst->operands[0].r, inst->operands[1].r);
      } else if (inst->opcode == TIR_MUL && inst->operand_count == 2 &&
                 inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_mul_reg_reg(&emitter, inst->operands[0].r, inst->operands[1].r);
      } else if (inst->opcode == TIR_DIV && inst->operand_count == 2 &&
                 inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_div_reg_reg(&emitter, inst->operands[0].r, inst->operands[1].r);
      } else if (inst->opcode == TIR_RET) {
        rane_x64_emit_ret(&emitter);
      }
      // Skip LABEL
    }
  }

  ctx->code_size = emitter.offset;
  return RANE_OK;
}