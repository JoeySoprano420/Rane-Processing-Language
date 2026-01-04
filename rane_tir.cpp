#include "rane_tir.h"
#include "rane_x64.h"
#include "rane_ast.h"
#include "rane_label.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration
static void lower_expr_to_tir(rane_expr_t* expr, rane_tir_function_t* func);
static void lower_stmt_to_tir(const rane_stmt_t* st, rane_tir_function_t* func);

static rane_label_gen_t g_lbl;

static int find_label_offset(const rane_x64_label_map_entry_t* labels, uint32_t label_count, const char* name, uint64_t* out_off) {
  if (!labels || !name || !out_off) return 0;
  for (uint32_t i = 0; i < label_count; i++) {
    if (strcmp(labels[i].label, name) == 0) {
      *out_off = labels[i].offset;
      return 1;
    }
  }
  return 0;
}

// Lowering AST to TIR
rane_error_t rane_lower_ast_to_tir(const rane_stmt_t* ast_root, rane_tir_module_t* tir_module) {
  if (!ast_root || !tir_module) return RANE_E_INVALID_ARG;

  // Initialize module
  tir_module->name[0] = 0;
  tir_module->functions = (rane_tir_function_t*)malloc(sizeof(rane_tir_function_t));
  tir_module->function_count = 1;
  tir_module->max_functions = 1;
  rane_tir_function_t* func = &tir_module->functions[0];
  strcpy_s(func->name, sizeof(func->name), "main");
  func->insts = (rane_tir_inst_t*)malloc(sizeof(rane_tir_inst_t) * 1024);
  func->inst_count = 0;
  func->max_insts = 1024;

  rane_label_gen_init(&g_lbl);
  lower_stmt_to_tir(ast_root, func);

  // Ensure a RET at end
  rane_tir_inst_t ret_inst;
  ret_inst.opcode = TIR_RET;
  ret_inst.type = RANE_TYPE_U64;
  ret_inst.operand_count = 0;
  func->insts[func->inst_count++] = ret_inst;

  return RANE_OK;
}

static void emit_label(rane_tir_function_t* func, const char* name) {
  rane_tir_inst_t inst;
  inst.opcode = TIR_LABEL;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), name);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

static void emit_jmp(rane_tir_function_t* func, const char* target) {
  rane_tir_inst_t inst;
  inst.opcode = TIR_JMP;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), target);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

static void emit_jcc(rane_tir_function_t* func, const char* target) {
  rane_tir_inst_t inst;
  inst.opcode = TIR_JCC;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), target);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

static void emit_jcc_ext(rane_tir_function_t* func, rane_tir_cc_t cc, const char* target) {
  rane_tir_inst_t inst;
  inst.opcode = TIR_JCC_EXT;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_IMM;
  inst.operands[0].imm = (uint64_t)cc;
  inst.operands[1].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[1].lbl, sizeof(inst.operands[1].lbl), target);
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

static void emit_cmp_r0_imm0(rane_tir_function_t* func) {
  rane_tir_inst_t c;
  c.opcode = TIR_CMP;
  c.type = RANE_TYPE_U64;
  c.operands[0].kind = TIR_OPERAND_R;
  c.operands[0].r = 0;
  c.operands[1].kind = TIR_OPERAND_IMM;
  c.operands[1].imm = 0;
  c.operand_count = 2;
  func->insts[func->inst_count++] = c;
}

static void lower_stmt_to_tir(const rane_stmt_t* st, rane_tir_function_t* func) {
  if (!st) return;

  if (st->kind == STMT_BLOCK) {
    for (uint32_t i = 0; i < st->block.stmt_count; i++) {
      lower_stmt_to_tir(st->block.stmts[i], func);
    }
    return;
  }

  if (st->kind == STMT_LET) {
    lower_expr_to_tir(st->let.expr, func);
    return;
  }

  if (st->kind == STMT_ASSIGN) {
    lower_expr_to_tir(st->assign.expr, func);
    return;
  }

  if (st->kind == STMT_IF) {
    char lbl_then[64];
    char lbl_else[64];
    char lbl_end[64];
    rane_label_gen_make(&g_lbl, lbl_then);
    rane_label_gen_make(&g_lbl, lbl_else);
    rane_label_gen_make(&g_lbl, lbl_end);

    // cond in r0
    lower_expr_to_tir(st->if_stmt.cond, func);
    emit_cmp_r0_imm0(func);

    // if cond != 0 -> then else -> else
    emit_jcc(func, lbl_then);
    emit_jmp(func, lbl_else);

    emit_label(func, lbl_then);
    lower_stmt_to_tir(st->if_stmt.then_branch, func);
    emit_jmp(func, lbl_end);

    emit_label(func, lbl_else);
    if (st->if_stmt.else_branch) {
      lower_stmt_to_tir(st->if_stmt.else_branch, func);
    }
    emit_label(func, lbl_end);
    return;
  }

  if (st->kind == STMT_WHILE) {
    char lbl_head[64];
    char lbl_body[64];
    char lbl_end[64];
    rane_label_gen_make(&g_lbl, lbl_head);
    rane_label_gen_make(&g_lbl, lbl_body);
    rane_label_gen_make(&g_lbl, lbl_end);

    emit_label(func, lbl_head);
    lower_expr_to_tir(st->while_stmt.cond, func);
    emit_cmp_r0_imm0(func);
    emit_jcc(func, lbl_body);
    emit_jmp(func, lbl_end);

    emit_label(func, lbl_body);
    lower_stmt_to_tir(st->while_stmt.body, func);
    emit_jmp(func, lbl_head);

    emit_label(func, lbl_end);
    return;
  }

  if (st->kind == STMT_JUMP) {
    emit_jmp(func, st->jump.marker);
    return;
  }

  if (st->kind == STMT_MARKER) {
    emit_label(func, st->marker.name);
    return;
  }

  // Control flow lowering to real labels/fixups is next step.
}

// Helper to lower expression
static void lower_expr_to_tir(rane_expr_t* expr, rane_tir_function_t* func) {
  if (!expr) return;

  if (expr->kind == EXPR_LIT_INT) {
    rane_tir_inst_t inst;
    inst.opcode = TIR_MOV;
    inst.type = expr->lit_int.type;
    inst.operands[0].kind = TIR_OPERAND_R;
    inst.operands[0].r = 0;
    inst.operands[1].kind = TIR_OPERAND_IMM;
    inst.operands[1].imm = expr->lit_int.value;
    inst.operand_count = 2;
    func->insts[func->inst_count++] = inst;
    return;
  } else if (expr->kind == EXPR_CALL) {
    // v0 builtin: print(x) -> printf("%s", x)
    if (strcmp(expr->call.name, "print") == 0) {
      // Arg0: format string (will be patched to .rdata+0 by driver)
      rane_tir_inst_t fmt;
      fmt.opcode = TIR_MOV;
      fmt.type = RANE_TYPE_P64;
      fmt.operands[0].kind = TIR_OPERAND_R;
      fmt.operands[0].r = 0; // reg0
      fmt.operands[1].kind = TIR_OPERAND_IMM;
      fmt.operands[1].imm = 0; // placeholder, patched by driver
      fmt.operand_count = 2;
      func->insts[func->inst_count++] = fmt;

      // Arg1: user string
      if (expr->call.arg_count >= 1) {
        lower_expr_to_tir(expr->call.args[0], func);
        // Move result reg0 -> reg1 (second arg)
        rane_tir_inst_t mv;
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_P64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 1;
        mv.operands[1].kind = TIR_OPERAND_R;
        mv.operands[1].r = 0;
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
      }

      rane_tir_inst_t call;
      call.opcode = TIR_CALL_IMPORT;
      call.type = RANE_TYPE_U64;
      call.operands[0].kind = TIR_OPERAND_LBL;
      strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), "printf");
      call.operand_count = 1;
      func->insts[func->inst_count++] = call;
      return;
    }

    // fallback
    rane_tir_inst_t call;
    call.opcode = TIR_CALL_IMPORT;
    call.type = RANE_TYPE_U64;
    call.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), expr->call.name);
    call.operand_count = 1;
    func->insts[func->inst_count++] = call;
    return;
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
    // Comparisons produce boolean in r0 (0/1)
    if (expr->binary.op == BIN_LT || expr->binary.op == BIN_LE || expr->binary.op == BIN_GT ||
        expr->binary.op == BIN_GE || expr->binary.op == BIN_EQ || expr->binary.op == BIN_NE) {
      // Evaluate left -> r0
      lower_expr_to_tir(expr->binary.left, func);
      // Move left to r2 temp
      {
        rane_tir_inst_t mv;
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_U64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 2;
        mv.operands[1].kind = TIR_OPERAND_R;
        mv.operands[1].r = 0;
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
      }

      // Evaluate right -> r0, then move to r1
      lower_expr_to_tir(expr->binary.right, func);
      {
        rane_tir_inst_t mv;
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_U64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 1;
        mv.operands[1].kind = TIR_OPERAND_R;
        mv.operands[1].r = 0;
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
      }

      // CMP r2, r1
      {
        rane_tir_inst_t c;
        c.opcode = TIR_CMP;
        c.type = RANE_TYPE_U64;
        c.operands[0].kind = TIR_OPERAND_R;
        c.operands[0].r = 2;
        c.operands[1].kind = TIR_OPERAND_R;
        c.operands[1].r = 1;
        c.operand_count = 2;
        func->insts[func->inst_count++] = c;
      }

      // Materialize boolean with labels (branch-based)
      char lbl_true[64];
      char lbl_false[64];
      char lbl_end[64];
      rane_label_gen_make(&g_lbl, lbl_true);
      rane_label_gen_make(&g_lbl, lbl_false);
      rane_label_gen_make(&g_lbl, lbl_end);

      // Emit conditional branch to lbl_true based on comparison.
      // For now we re-use TIR_JCC as "jump if condition true" with label string encoding.
      // Encode opcode name into label prefix is avoided; instead we use marker labels and a side-channel:
      // bootstrap: treat BIN_NE as JCC(JNE) and BIN_EQ as JCC(JE), others default to JNE.
      // (x64 codegen below is upgraded to handle these via a new TIR_JCC_EXT form.)

      // Simplified: for EQ/NE only in v0. Others fall back to NE.
      // true if (r2 != r1)
      if (expr->binary.op == BIN_EQ) {
        // false-first JNE -> false; then true
        emit_jcc(func, lbl_false);
        emit_jmp(func, lbl_true);
      } else {
        // default NE: JCC -> true
        emit_jcc(func, lbl_true);
        emit_jmp(func, lbl_false);
      }

      emit_label(func, lbl_true);
      {
        rane_tir_inst_t one;
        one.opcode = TIR_MOV;
        one.type = RANE_TYPE_U64;
        one.operands[0].kind = TIR_OPERAND_R;
        one.operands[0].r = 0;
        one.operands[1].kind = TIR_OPERAND_IMM;
        one.operands[1].imm = 1;
        one.operand_count = 2;
        func->insts[func->inst_count++] = one;
      }
      emit_jmp(func, lbl_end);

      emit_label(func, lbl_false);
      {
        rane_tir_inst_t zero;
        zero.opcode = TIR_MOV;
        zero.type = RANE_TYPE_U64;
        zero.operands[0].kind = TIR_OPERAND_R;
        zero.operands[0].r = 0;
        zero.operands[1].kind = TIR_OPERAND_IMM;
        zero.operands[1].imm = 0;
        zero.operand_count = 2;
        func->insts[func->inst_count++] = zero;
      }
      emit_label(func, lbl_end);
      return;
    }

    // Arithmetic
    lower_expr_to_tir(expr->binary.left, func);
    // Move left to r1
    {
      rane_tir_inst_t mv;
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_U64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 1;
      mv.operands[1].kind = TIR_OPERAND_R;
      mv.operands[1].r = 0;
      mv.operand_count = 2;
      func->insts[func->inst_count++] = mv;
    }
    lower_expr_to_tir(expr->binary.right, func);

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
    return;
  }
}

// Code generation TIR to x64
rane_error_t rane_codegen_tir_to_x64(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx) {
  if (!tir_module || !ctx || !ctx->code_buffer) return RANE_E_INVALID_ARG;

  rane_x64_emitter_t emitter;
  rane_x64_init_emitter(&emitter, ctx->code_buffer, ctx->buffer_size);

  // Collect labels and fixups
  rane_x64_label_map_entry_t labels[512];
  uint32_t label_count = 0;
  rane_x64_fixup_t fixups[512];
  uint32_t fixup_count = 0;
  typedef struct call_fixup_s {
    char sym[64];
    uint64_t patch_offset; // start of instruction sequence that needs rewriting
  } call_fixup_t;
  call_fixup_t call_fixups[128];
  uint32_t call_fixup_count = 0;

  if (tir_module->function_count > 0) {
    const rane_tir_function_t* func = &tir_module->functions[0];

    // First pass: emit code, record label offsets & fixups
    for (uint32_t i = 0; i < func->inst_count; i++) {
      const rane_tir_inst_t* inst = &func->insts[i];

      if (inst->opcode == TIR_LABEL && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        if (label_count < 512) {
          strcpy_s(labels[label_count].label, sizeof(labels[label_count].label), inst->operands[0].lbl);
          labels[label_count].offset = emitter.offset;
          label_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_JMP && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        // Emit E9 + rel32 placeholder
        rane_x64_emit_jmp_rel32(&emitter, 0);
        if (fixup_count < 512) {
          strcpy_s(fixups[fixup_count].label, sizeof(fixups[fixup_count].label), inst->operands[0].lbl);
          fixups[fixup_count].patch_offset = emitter.offset - 4;
          fixups[fixup_count].kind = 1;
          fixup_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_JCC && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        // Use JNE rel32 for now (cond != 0)
        rane_x64_emit_jne_rel32(&emitter, 0);
        if (fixup_count < 512) {
          strcpy_s(fixups[fixup_count].label, sizeof(fixups[fixup_count].label), inst->operands[0].lbl);
          fixups[fixup_count].patch_offset = emitter.offset - 4;
          fixups[fixup_count].kind = 3;
          fixup_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_MOV && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_IMM) {
        rane_x64_emit_mov_reg_imm64(&emitter, (uint8_t)inst->operands[0].r, inst->operands[1].imm);
        continue;
      }

      if (inst->opcode == TIR_MOV && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_mov_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_CMP && inst->operand_count == 2) {
        if (inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_IMM) {
          rane_x64_emit_cmp_reg_imm(&emitter, (uint8_t)inst->operands[0].r, inst->operands[1].imm);
        } else if (inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
          rane_x64_emit_cmp_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        }
        continue;
      }

      if (inst->opcode == TIR_ADD && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_add_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_SUB && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_sub_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_MUL && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_mul_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_DIV && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_div_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_CALL_IMPORT && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        // Win64: RCX=arg0, RDX=arg1
        rane_x64_emit_mov_reg_reg(&emitter, 1 /*RCX*/, 0 /*RAX*/);
        rane_x64_emit_mov_reg_reg(&emitter, 2 /*RDX*/, 1 /*RCX*/);

        // Placeholder: emit 9 bytes to be rewritten by the driver into:
        //   mov rax, [rip+disp32]
        //   call rax
        // Patch offset points at start of this placeholder.
        uint64_t patch_start = emitter.offset;
        uint8_t placeholder[9] = { 0xE8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90 }; // call + nops
        rane_x64_emit_bytes(&emitter, placeholder, sizeof(placeholder));

        if (call_fixup_count < 128) {
          strcpy_s(call_fixups[call_fixup_count].sym, sizeof(call_fixups[call_fixup_count].sym), inst->operands[0].lbl);
          call_fixups[call_fixup_count].patch_offset = patch_start;
          call_fixup_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_RET) {
        uint8_t add_rsp[] = { 0x48, 0x83, 0xC4, 0x28 };
        uint8_t mov_rsp_rbp[] = { 0x48, 0x89, 0xEC };
        uint8_t pop_rbp = 0x5D;
        rane_x64_emit_bytes(&emitter, add_rsp, sizeof(add_rsp));
        rane_x64_emit_bytes(&emitter, mov_rsp_rbp, sizeof(mov_rsp_rbp));
        rane_x64_emit_bytes(&emitter, &pop_rbp, 1);
        rane_x64_emit_ret(&emitter);
        continue;
      }

      if (inst->opcode == TIR_JCC_EXT && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_IMM && inst->operands[1].kind == TIR_OPERAND_LBL) {
        rane_tir_cc_t cc = (rane_tir_cc_t)inst->operands[0].imm;
        // Emit rel32 placeholder
        switch (cc) {
          case TIR_CC_E:  rane_x64_emit_je_rel32(&emitter, 0); break;
          case TIR_CC_NE: rane_x64_emit_jne_rel32(&emitter, 0); break;
          case TIR_CC_L:  rane_x64_emit_jl_rel32(&emitter, 0); break;
          case TIR_CC_LE: rane_x64_emit_jle_rel32(&emitter, 0); break;
          case TIR_CC_G:  rane_x64_emit_jg_rel32(&emitter, 0); break;
          case TIR_CC_GE: rane_x64_emit_jge_rel32(&emitter, 0); break;
          default:        rane_x64_emit_jne_rel32(&emitter, 0); break;
        }

        if (fixup_count < 512) {
          strcpy_s(fixups[fixup_count].label, sizeof(fixups[fixup_count].label), inst->operands[1].lbl);
          fixups[fixup_count].patch_offset = emitter.offset - 4;
          fixups[fixup_count].kind = 0; // unused
          fixup_count++;
        }
        continue;
      }
    }

    // Second pass: patch rel32
    for (uint32_t fi = 0; fi < fixup_count; fi++) {
      uint64_t target_off = 0;
      if (!find_label_offset(labels, label_count, fixups[fi].label, &target_off)) continue;

      // rel32 is relative to next instruction after rel32 immediate
      uint64_t patch_off = fixups[fi].patch_offset;
      uint64_t next_ip = patch_off + 4;
      int32_t rel = (int32_t)((int64_t)target_off - (int64_t)next_ip);
      memcpy(emitter.buffer + patch_off, &rel, 4);
    }
  }

  ctx->code_size = emitter.offset;
  return RANE_OK;
}

// Preferred/public API name
rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx) {
  return rane_codegen_tir_to_x64(tir_module, ctx);
}