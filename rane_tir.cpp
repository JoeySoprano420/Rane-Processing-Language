#include "rane_tir.h"
#include "rane_x64.h"
#include "rane_ast.h"
#include "rane_aot.h"
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

static void emit_label(rane_tir_function_t* func, const char* name) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_LABEL;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), name);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

static void emit_jmp(rane_tir_function_t* func, const char* target) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_JMP;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), target);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

static void emit_jcc_ext(rane_tir_function_t* func, rane_tir_cc_t cc, const char* target) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_JCC_EXT;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_IMM;
  inst.operands[0].imm = (uint64_t)cc;
  inst.operands[1].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[1].lbl, sizeof(inst.operands[1].lbl), target);
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

// Local slot map (per-function, bootstrap)
typedef struct rane_local_s {
  char name[64];
  uint32_t slot;
} rane_local_t;

static rane_local_t g_locals[256];
static uint32_t g_local_count = 0;
static uint32_t g_next_slot = 0;

static void locals_reset() {
  g_local_count = 0;
  g_next_slot = 0;
}

static int local_find_slot(const char* name, uint32_t* out_slot) {
  if (!name) return 0;
  for (uint32_t i = 0; i < g_local_count; i++) {
    if (strcmp(g_locals[i].name, name) == 0) {
      if (out_slot) *out_slot = g_locals[i].slot;
      return 1;
    }
  }
  return 0;
}

static uint32_t local_get_or_define_slot(const char* name) {
  uint32_t s = 0;
  if (local_find_slot(name, &s)) return s;
  if (g_local_count < 256) {
    strncpy_s(g_locals[g_local_count].name, sizeof(g_locals[g_local_count].name), name, _TRUNCATE);
    g_locals[g_local_count].slot = g_next_slot;
    g_local_count++;
    return g_next_slot++;
  }
  return 0;
}

static void emit_ld_slot_to_r0(rane_tir_function_t* func, uint32_t slot) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_LD;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_R;
  inst.operands[0].r = 0;
  inst.operands[1].kind = TIR_OPERAND_S;
  inst.operands[1].s = slot;
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

static void emit_st_r0_to_slot(rane_tir_function_t* func, uint32_t slot) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_ST;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_S;
  inst.operands[0].s = slot;
  inst.operands[1].kind = TIR_OPERAND_R;
  inst.operands[1].r = 0;
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

static void emit_mov_r0_imm(rane_tir_function_t* func, uint64_t imm, rane_type_e ty) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_MOV;
  inst.type = ty;
  inst.operands[0].kind = TIR_OPERAND_R;
  inst.operands[0].r = 0;
  inst.operands[1].kind = TIR_OPERAND_IMM;
  inst.operands[1].imm = imm;
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

static void emit_cmp_r0_imm(rane_tir_function_t* func, uint64_t imm) {
  rane_tir_inst_t c;
  memset(&c, 0, sizeof(c));
  c.opcode = TIR_CMP;
  c.type = RANE_TYPE_U64;
  c.operands[0].kind = TIR_OPERAND_R;
  c.operands[0].r = 0;
  c.operands[1].kind = TIR_OPERAND_IMM;
  c.operands[1].imm = imm;
  c.operand_count = 2;
  func->insts[func->inst_count++] = c;
}

static void emit_bool_from_cmp_cc(rane_tir_function_t* func, rane_tir_cc_t cc) {
  char lbl_true[64];
  char lbl_false[64];
  char lbl_end[64];
  rane_label_gen_make(&g_lbl, lbl_true);
  rane_label_gen_make(&g_lbl, lbl_false);
  rane_label_gen_make(&g_lbl, lbl_end);

  emit_jcc_ext(func, cc, lbl_true);
  emit_jmp(func, lbl_false);

  emit_label(func, lbl_true);
  emit_mov_r0_imm(func, 1, RANE_TYPE_U64);
  emit_jmp(func, lbl_end);

  emit_label(func, lbl_false);
  emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
  emit_label(func, lbl_end);
}

// ---- bootstrap proc table (module-level) ----
typedef struct rane_proc_entry_s {
  char name[64];
  const rane_stmt_t* proc_stmt;
} rane_proc_entry_t;

static rane_proc_entry_t g_procs[128];
static uint32_t g_proc_count = 0;

static void procs_reset() { g_proc_count = 0; }

static void procs_collect(const rane_stmt_t* st) {
  if (!st) return;
  if (st->kind == STMT_BLOCK) {
    for (uint32_t i = 0; i < st->block.stmt_count; i++) procs_collect(st->block.stmts[i]);
    return;
  }
  if (st->kind == STMT_PROC) {
    if (g_proc_count < 128) {
      strncpy_s(g_procs[g_proc_count].name, sizeof(g_procs[g_proc_count].name), st->proc.name, _TRUNCATE);
      g_procs[g_proc_count].proc_stmt = st;
      g_proc_count++;
    }
    return;
  }
}

static const rane_stmt_t* procs_find(const char* name) {
  if (!name) return NULL;
  for (uint32_t i = 0; i < g_proc_count; i++) {
    if (strcmp(g_procs[i].name, name) == 0) return g_procs[i].proc_stmt;
  }
  return NULL;
}

static int stmt_is_proc_def(const rane_stmt_t* st) { return st && st->kind == STMT_PROC; }

static void lower_stmt_to_tir_skip_proc_defs(const rane_stmt_t* st, rane_tir_function_t* func) {
  if (!st) return;
  if (stmt_is_proc_def(st)) return;
  if (st->kind == STMT_BLOCK) {
    for (uint32_t i = 0; i < st->block.stmt_count; i++) {
      lower_stmt_to_tir_skip_proc_defs(st->block.stmts[i], func);
    }
    return;
  }
  lower_stmt_to_tir(st, func);
}

static void ensure_module_capacity(rane_tir_module_t* m, uint32_t needed) {
  if (m->max_functions >= needed) return;
  uint32_t new_cap = m->max_functions ? m->max_functions : 1;
  while (new_cap < needed) new_cap *= 2;
  rane_tir_function_t* nf = (rane_tir_function_t*)realloc(m->functions, sizeof(rane_tir_function_t) * new_cap);
  if (!nf) return;
  // zero new tail
  if (new_cap > m->max_functions) {
    memset(nf + m->max_functions, 0, sizeof(rane_tir_function_t) * (new_cap - m->max_functions));
  }
  m->functions = nf;
  m->max_functions = new_cap;
}

static rane_tir_function_t* module_add_function(rane_tir_module_t* m, const char* name) {
  ensure_module_capacity(m, m->function_count + 1);
  rane_tir_function_t* f = &m->functions[m->function_count++];
  memset(f, 0, sizeof(*f));
  strncpy_s(f->name, sizeof(f->name), name, _TRUNCATE);
  f->insts = (rane_tir_inst_t*)malloc(sizeof(rane_tir_inst_t) * 2048);
  f->inst_count = 0;
  f->max_insts = 2048;
  f->stack_slot_count = 0;
  return f;
}

static void emit_call_local(rane_tir_function_t* func, const char* target) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_CALL_LOCAL;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), target);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

// Lowering AST to TIR (module-aware)
rane_error_t rane_lower_ast_to_tir(const rane_stmt_t* ast_root, rane_tir_module_t* tir_module) {
  if (!ast_root || !tir_module) return RANE_E_INVALID_ARG;

  memset(tir_module, 0, sizeof(*tir_module));
  tir_module->functions = NULL;
  tir_module->function_count = 0;
  tir_module->max_functions = 0;

  rane_label_gen_init(&g_lbl);
  procs_reset();
  procs_collect(ast_root);

  // Emit user procs first.
  for (uint32_t pi = 0; pi < g_proc_count; pi++) {
    const rane_stmt_t* p = g_procs[pi].proc_stmt;
    if (!p) continue;

    rane_tir_function_t* f = module_add_function(tir_module, p->proc.name);

    locals_reset();

    // Map parameters to initial slots (slot0..slotN-1)
    for (uint32_t i = 0; i < p->proc.param_count; i++) {
      local_get_or_define_slot(p->proc.params[i]);
    }

    // Prologue moves: store incoming args (RCX/RDX) to param slots.
    // Our virtual reg mapping in codegen treats: reg1=RCX, reg2=RDX.
    for (uint32_t i = 0; i < p->proc.param_count; i++) {
      uint32_t slot = 0;
      if (!local_find_slot(p->proc.params[i], &slot)) continue;
      if (i == 0) {
        rane_tir_inst_t st;
        memset(&st, 0, sizeof(st));
        st.opcode = TIR_ST;
        st.type = RANE_TYPE_U64;
        st.operands[0].kind = TIR_OPERAND_S;
        st.operands[0].s = slot;
        st.operands[1].kind = TIR_OPERAND_R;
        st.operands[1].r = 1; // RCX
        st.operand_count = 2;
        f->insts[f->inst_count++] = st;
      } else if (i == 1) {
        rane_tir_inst_t st;
        memset(&st, 0, sizeof(st));
        st.opcode = TIR_ST;
        st.type = RANE_TYPE_U64;
        st.operands[0].kind = TIR_OPERAND_S;
        st.operands[0].s = slot;
        st.operands[1].kind = TIR_OPERAND_R;
        st.operands[1].r = 2; // RDX
        st.operand_count = 2;
        f->insts[f->inst_count++] = st;
      }
    }

    lower_stmt_to_tir(p->proc.body, f);

    // Ensure function ends with RET
    if (f->inst_count == 0 || (f->insts[f->inst_count - 1].opcode != TIR_RET && f->insts[f->inst_count - 1].opcode != TIR_RET_VAL)) {
      rane_tir_inst_t ret_inst;
      memset(&ret_inst, 0, sizeof(ret_inst));
      ret_inst.opcode = TIR_RET;
      ret_inst.type = RANE_TYPE_U64;
      ret_inst.operand_count = 0;
      f->insts[f->inst_count++] = ret_inst;
    }

    f->stack_slot_count = g_next_slot;
  }

  // Emit implicit main wrapper for top-level statements (excluding proc defs).
  {
    rane_tir_function_t* mainf = module_add_function(tir_module, "main");
    locals_reset();
    lower_stmt_to_tir_skip_proc_defs(ast_root, mainf);

    rane_tir_inst_t ret_inst;
    memset(&ret_inst, 0, sizeof(ret_inst));
    ret_inst.opcode = TIR_RET;
    ret_inst.type = RANE_TYPE_U64;
    ret_inst.operand_count = 0;
    mainf->insts[mainf->inst_count++] = ret_inst;

    mainf->stack_slot_count = g_next_slot;
  }

  return RANE_OK;
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
    uint32_t slot = local_get_or_define_slot(st->let.name);
    lower_expr_to_tir(st->let.expr, func);
    emit_st_r0_to_slot(func, slot);
    return;
  }

  if (st->kind == STMT_ASSIGN) {
    if (strcmp(st->assign.target, "_") == 0) {
      lower_expr_to_tir(st->assign.expr, func);
      return;
    }

    uint32_t slot = 0;
    if (!local_find_slot(st->assign.target, &slot)) {
      slot = local_get_or_define_slot(st->assign.target);
    }
    lower_expr_to_tir(st->assign.expr, func);
    emit_st_r0_to_slot(func, slot);
    return;
  }

  if (st->kind == STMT_IF) {
    char lbl_then[64];
    char lbl_else[64];
    char lbl_end[64];
    rane_label_gen_make(&g_lbl, lbl_then);
    rane_label_gen_make(&g_lbl, lbl_else);
    rane_label_gen_make(&g_lbl, lbl_end);

    lower_expr_to_tir(st->if_stmt.cond, func);
    emit_cmp_r0_imm(func, 0);

    // true => then
    emit_jcc_ext(func, TIR_CC_NE, lbl_then);
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
    emit_cmp_r0_imm(func, 0);

    emit_jcc_ext(func, TIR_CC_NE, lbl_body);
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

  if (st->kind == STMT_RETURN) {
    if (st->ret.expr) {
      lower_expr_to_tir(st->ret.expr, func);
      rane_tir_inst_t rv;
      memset(&rv, 0, sizeof(rv));
      rv.opcode = TIR_RET_VAL;
      rv.type = RANE_TYPE_U64;
      rv.operands[0].kind = TIR_OPERAND_R;
      rv.operands[0].r = 0;
      rv.operand_count = 1;
      func->insts[func->inst_count++] = rv;
    } else {
      rane_tir_inst_t r;
      memset(&r, 0, sizeof(r));
      r.opcode = TIR_RET;
      r.type = RANE_TYPE_U64;
      r.operand_count = 0;
      func->insts[func->inst_count++] = r;
    }
    return;
  }

  // STMT_PROC is not lowered yet (multi-function support is next milestone).
}

// Helper to lower expression
static void lower_expr_to_tir(rane_expr_t* expr, rane_tir_function_t* func) {
  if (!expr) return;

  if (expr->kind == EXPR_LIT_BOOL) {
    emit_mov_r0_imm(func, (uint64_t)(expr->lit_bool.value ? 1 : 0), RANE_TYPE_U64);
    return;
  }

  if (expr->kind == EXPR_LIT_INT) {
    rane_tir_inst_t inst;
    memset(&inst, 0, sizeof(inst));
    inst.opcode = TIR_MOV;
    inst.type = expr->lit_int.type;
    inst.operands[0].kind = TIR_OPERAND_R;
    inst.operands[0].r = 0;
    inst.operands[1].kind = TIR_OPERAND_IMM;
    inst.operands[1].imm = expr->lit_int.value;
    inst.operand_count = 2;
    func->insts[func->inst_count++] = inst;
    return;
  }

  if (expr->kind == EXPR_VAR) {
    uint32_t slot = 0;
    if (!local_find_slot(expr->var.name, &slot)) {
      // Undefined variable => treat as 0 in codegen; typechecker should have rejected.
      emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
      return;
    }
    emit_ld_slot_to_r0(func, slot);
    return;
  }

  if (expr->kind == EXPR_CALL) {
    // v0 builtin: print(x) -> printf("%s", x)
    if (strcmp(expr->call.name, "print") == 0) {
      // Arg0: format string (patched by driver)
      rane_tir_inst_t fmt;
      memset(&fmt, 0, sizeof(fmt));
      fmt.opcode = TIR_MOV;
      fmt.type = RANE_TYPE_P64;
      fmt.operands[0].kind = TIR_OPERAND_R;
      fmt.operands[0].r = 0;
      fmt.operands[1].kind = TIR_OPERAND_IMM;
      fmt.operands[1].imm = 0;
      fmt.operand_count = 2;
      func->insts[func->inst_count++] = fmt;

      // Arg1: user string
      if (expr->call.arg_count >= 1) {
        lower_expr_to_tir(expr->call.args[0], func);
        // Move r0 -> r1 (second arg)
        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
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
      memset(&call, 0, sizeof(call));
      call.opcode = TIR_CALL_IMPORT;
      call.type = RANE_TYPE_U64;
      call.operands[0].kind = TIR_OPERAND_LBL;
      strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), "printf");
      call.operand_count = 1;
      func->insts[func->inst_count++] = call;
      return;
    }

    // User-defined proc call?
    if (procs_find(expr->call.name)) {
      // Arg0 -> reg1 (RCX), Arg1 -> reg2 (RDX)
      if (expr->call.arg_count >= 1) {
        lower_expr_to_tir(expr->call.args[0], func);
        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_U64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 1;
        mv.operands[1].kind = TIR_OPERAND_R;
        mv.operands[1].r = 0;
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
      }
      if (expr->call.arg_count >= 2) {
        lower_expr_to_tir(expr->call.args[1], func);
        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_U64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 2;
        mv.operands[1].kind = TIR_OPERAND_R;
        mv.operands[1].r = 0;
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
      }

      emit_call_local(func, expr->call.name);
      // Return value is in reg0 implicitly.
      return;
    }

    // Otherwise treat as import
    rane_tir_inst_t call;
    memset(&call, 0, sizeof(call));
    call.opcode = TIR_CALL_IMPORT;
    call.type = RANE_TYPE_U64;
    call.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), expr->call.name);
    call.operand_count = 1;
    func->insts[func->inst_count++] = call;
    return;
  }

  if (expr->kind == EXPR_BINARY) {
    // Short-circuit boolean ops using labels and boolean materialization.
    if (expr->binary.op == BIN_LOGAND || expr->binary.op == BIN_LOGOR) {
      char lbl_rhs[64];
      char lbl_false[64];
      char lbl_true[64];
      char lbl_end[64];
      rane_label_gen_make(&g_lbl, lbl_rhs);
      rane_label_gen_make(&g_lbl, lbl_false);
      rane_label_gen_make(&g_lbl, lbl_true);
      rane_label_gen_make(&g_lbl, lbl_end);

      // Evaluate LHS into r0
      lower_expr_to_tir(expr->binary.left, func);
      emit_cmp_r0_imm(func, 0);

      if (expr->binary.op == BIN_LOGAND) {
        // if lhs == 0 -> false else evaluate rhs
        emit_jcc_ext(func, TIR_CC_E, lbl_false);
        emit_jmp(func, lbl_rhs);
      } else {
        // LOGOR: if lhs != 0 -> true else evaluate rhs
        emit_jcc_ext(func, TIR_CC_NE, lbl_true);
        emit_jmp(func, lbl_rhs);
      }

      emit_label(func, lbl_rhs);
      lower_expr_to_tir(expr->binary.right, func);
      emit_cmp_r0_imm(func, 0);
      // For both, the final value is (rhs != 0)
      emit_jcc_ext(func, TIR_CC_NE, lbl_true);
      emit_jmp(func, lbl_false);

      emit_label(func, lbl_true);
      emit_mov_r0_imm(func, 1, RANE_TYPE_U64);
      emit_jmp(func, lbl_end);

      emit_label(func, lbl_false);
      emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
      emit_label(func, lbl_end);
      return;
    }

    // Comparisons produce boolean in r0 (0/1)
    if (expr->binary.op == BIN_LT || expr->binary.op == BIN_LE || expr->binary.op == BIN_GT ||
        expr->binary.op == BIN_GE || expr->binary.op == BIN_EQ || expr->binary.op == BIN_NE) {
      // Evaluate left -> r0
      lower_expr_to_tir(expr->binary.left, func);
      // Move left to r2 temp
      {
        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
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
        memset(&mv, 0, sizeof(mv));
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
        memset(&c, 0, sizeof(c));
        c.opcode = TIR_CMP;
        c.type = RANE_TYPE_U64;
        c.operands[0].kind = TIR_OPERAND_R;
        c.operands[0].r = 2;
        c.operands[1].kind = TIR_OPERAND_R;
        c.operands[1].r = 1;
        c.operand_count = 2;
        func->insts[func->inst_count++] = c;
      }

      rane_tir_cc_t cc = TIR_CC_NE;
      switch (expr->binary.op) {
        case BIN_EQ: cc = TIR_CC_E; break;
        case BIN_NE: cc = TIR_CC_NE; break;
        case BIN_LT: cc = TIR_CC_L; break;
        case BIN_LE: cc = TIR_CC_LE; break;
        case BIN_GT: cc = TIR_CC_G; break;
        case BIN_GE: cc = TIR_CC_GE; break;
        default: break;
      }

      emit_bool_from_cmp_cc(func, cc);
      return;
    }

    // Arithmetic + bitwise (non-short-circuit)
    lower_expr_to_tir(expr->binary.left, func);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
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
    memset(&inst, 0, sizeof(inst));
    rane_tir_opcode_t opcode;
    if (expr->binary.op == BIN_ADD) opcode = TIR_ADD;
    else if (expr->binary.op == BIN_SUB) opcode = TIR_SUB;
    else if (expr->binary.op == BIN_MUL) opcode = TIR_MUL;
    else if (expr->binary.op == BIN_DIV) opcode = TIR_DIV;
    else if (expr->binary.op == BIN_AND) opcode = TIR_AND;
    else if (expr->binary.op == BIN_OR) opcode = TIR_OR;
    else if (expr->binary.op == BIN_XOR) opcode = TIR_XOR;
    else if (expr->binary.op == BIN_SHL) opcode = TIR_SHL;
    else if (expr->binary.op == BIN_SHR) opcode = TIR_SHR;
    else if (expr->binary.op == BIN_SAR) opcode = TIR_SAR;
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

  if (expr->kind == EXPR_UNARY) {
    if (!expr->unary.expr) {
      emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
      return;
    }

    switch (expr->unary.op) {
      case UN_NEG: {
        // r0 = 0 - (expr)
        lower_expr_to_tir(expr->unary.expr, func);
        // move (expr) into r1
        {
          rane_tir_inst_t mv;
          memset(&mv, 0, sizeof(mv));
          mv.opcode = TIR_MOV;
          mv.type = RANE_TYPE_U64;
          mv.operands[0].kind = TIR_OPERAND_R;
          mv.operands[0].r = 1;
          mv.operands[1].kind = TIR_OPERAND_R;
          mv.operands[1].r = 0;
          mv.operand_count = 2;
          func->insts[func->inst_count++] = mv;
        }
        emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
        {
          rane_tir_inst_t sub;
          memset(&sub, 0, sizeof(sub));
          sub.opcode = TIR_SUB;
          sub.type = RANE_TYPE_U64;
          sub.operands[0].kind = TIR_OPERAND_R;
          sub.operands[0].r = 0;
          sub.operands[1].kind = TIR_OPERAND_R;
          sub.operands[1].r = 1;
          sub.operand_count = 2;
          func->insts[func->inst_count++] = sub;
        }
        return;
      }

      case UN_NOT: {
        // booleanize: r0 = (expr == 0) ? 1 : 0
        lower_expr_to_tir(expr->unary.expr, func);
        emit_cmp_r0_imm(func, 0);
        emit_bool_from_cmp_cc(func, TIR_CC_E);
        return;
      }

      case UN_BITNOT: {
        // r0 = expr ^ 0xFFFF...FFFF
        lower_expr_to_tir(expr->unary.expr, func);
        // r1 = ~0
        {
          rane_tir_inst_t mv;
          memset(&mv, 0, sizeof(mv));
          mv.opcode = TIR_MOV;
          mv.type = RANE_TYPE_U64;
          mv.operands[0].kind = TIR_OPERAND_R;
          mv.operands[0].r = 1;
          mv.operands[1].kind = TIR_OPERAND_IMM;
          mv.operands[1].imm = 0xFFFFFFFFFFFFFFFFull;
          mv.operand_count = 2;
          func->insts[func->inst_count++] = mv;
        }
        {
          rane_tir_inst_t x;
          memset(&x, 0, sizeof(x));
          x.opcode = TIR_XOR;
          x.type = RANE_TYPE_U64;
          x.operands[0].kind = TIR_OPERAND_R;
          x.operands[0].r = 0;
          x.operands[1].kind = TIR_OPERAND_R;
          x.operands[1].r = 1;
          x.operand_count = 2;
          func->insts[func->inst_count++] = x;
        }
        return;
      }

      default:
        break;
    }

    emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
    return;
  }
}

// Code generation TIR to x64
rane_error_t rane_codegen_tir_to_x64(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx) {
  if (!tir_module || !ctx || !ctx->code_buffer) return RANE_E_INVALID_ARG;

  rane_x64_emitter_t emitter;
  rane_x64_init_emitter(&emitter, ctx->code_buffer, ctx->buffer_size);

  rane_x64_label_map_entry_t labels[2048];
  uint32_t label_count = 0;
  rane_x64_fixup_t fixups[2048];
  uint32_t fixup_count = 0;

  if (ctx && ctx->call_fixups) {
    ctx->call_fixup_count = 0;
  }

  auto emit_function = [&](const rane_tir_function_t* func) {
    if (!func) return;

    // Function entry label
    if (label_count < 2048) {
      strcpy_s(labels[label_count].label, sizeof(labels[label_count].label), func->name);
      labels[label_count].offset = emitter.offset;
      label_count++;
    }

    // Prologue + locals
    {
      rane_x64_emit_push_reg(&emitter, 5);
      uint8_t rex = 0x48;
      uint8_t op = 0x89;
      uint8_t modrm = 0xE5;
      rane_x64_emit_bytes(&emitter, &rex, 1);
      rane_x64_emit_bytes(&emitter, &op, 1);
      rane_x64_emit_bytes(&emitter, &modrm, 1);

      uint32_t locals_bytes = func->stack_slot_count * 8;
      uint32_t aligned = (locals_bytes + 15u) & ~15u;
      if (aligned) {
        rane_x64_emit_sub_rsp_imm32(&emitter, aligned);
      }
    }

    for (uint32_t i = 0; i < func->inst_count; i++) {
      const rane_tir_inst_t* inst = &func->insts[i];

      if (inst->opcode == TIR_LABEL && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        if (label_count < 2048) {
          strcpy_s(labels[label_count].label, sizeof(labels[label_count].label), inst->operands[0].lbl);
          labels[label_count].offset = emitter.offset;
          label_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_JMP && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        rane_x64_emit_jmp_rel32(&emitter, 0);
        if (fixup_count < 2048) {
          strcpy_s(fixups[fixup_count].label, sizeof(fixups[fixup_count].label), inst->operands[0].lbl);
          fixups[fixup_count].patch_offset = emitter.offset - 4;
          fixups[fixup_count].kind = 1;
          fixup_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_JCC_EXT && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_IMM && inst->operands[1].kind == TIR_OPERAND_LBL) {
        rane_tir_cc_t cc = (rane_tir_cc_t)inst->operands[0].imm;
        switch (cc) {
          case TIR_CC_E:  rane_x64_emit_je_rel32(&emitter, 0);  break;
          case TIR_CC_NE: rane_x64_emit_jne_rel32(&emitter, 0); break;
          case TIR_CC_L:  rane_x64_emit_jl_rel32(&emitter, 0);  break;
          case TIR_CC_LE: rane_x64_emit_jle_rel32(&emitter, 0); break;
          case TIR_CC_G:  rane_x64_emit_jg_rel32(&emitter, 0);  break;
          case TIR_CC_GE: rane_x64_emit_jge_rel32(&emitter, 0); break;
          default:        rane_x64_emit_jne_rel32(&emitter, 0); break;
        }
        if (fixup_count < 2048) {
          strcpy_s(fixups[fixup_count].label, sizeof(fixups[fixup_count].label), inst->operands[1].lbl);
          fixups[fixup_count].patch_offset = emitter.offset - 4;
          fixups[fixup_count].kind = 2;
          fixup_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_CALL_LOCAL && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        // call rel32 placeholder
        rane_x64_emit_call_rel32(&emitter, 0);
        if (fixup_count < 2048) {
          strcpy_s(fixups[fixup_count].label, sizeof(fixups[fixup_count].label), inst->operands[0].lbl);
          fixups[fixup_count].patch_offset = emitter.offset - 4;
          fixups[fixup_count].kind = 4; // call
          fixup_count++;
        }
        continue;
      }

      if (inst->opcode == TIR_LD && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_S) {
        // mov r, [rbp - (slot+1)*8]
        int32_t disp = -(int32_t)((inst->operands[1].s + 1u) * 8u);
        rane_x64_emit_mov_reg_mem(&emitter, (uint8_t)inst->operands[0].r, 5 /*RBP*/, disp);
        continue;
      }

      if (inst->opcode == TIR_ST && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_S && inst->operands[1].kind == TIR_OPERAND_R) {
        // mov [rbp - (slot+1)*8], r
        int32_t disp = -(int32_t)((inst->operands[0].s + 1u) * 8u);
        rane_x64_emit_mov_mem_reg(&emitter, 5 /*RBP*/, disp, (uint8_t)inst->operands[1].r);
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

      if (inst->opcode == TIR_XOR && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_xor_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_AND && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_and_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if (inst->opcode == TIR_OR && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_or_reg_reg(&emitter, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
        continue;
      }

      if ((inst->opcode == TIR_SHL || inst->opcode == TIR_SHR || inst->opcode == TIR_SAR) && inst->operand_count == 2 &&
          inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        // x64 variable shifts use CL as the count.
        // Move count reg -> RCX (so CL is set) if needed.
        if (inst->operands[1].r != 1) {
          rane_x64_emit_mov_reg_reg(&emitter, 1 /*RCX*/, (uint8_t)inst->operands[1].r);
        }

        uint8_t dst = (uint8_t)inst->operands[0].r;
        if (inst->opcode == TIR_SHL) rane_x64_emit_shl_reg_cl(&emitter, dst);
        else if (inst->opcode == TIR_SHR) rane_x64_emit_shr_reg_cl(&emitter, dst);
        else rane_x64_emit_sar_reg_cl(&emitter, dst);
        continue;
      }

      if (inst->opcode == TIR_CALL_IMPORT && inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_LBL) {
        // Win64: RCX=arg0, RDX=arg1 (virtual regs 1 and 2)
        // Our lowering already put args in reg1/reg2 for local calls; for imports we keep existing behavior.
        rane_x64_emit_mov_reg_reg(&emitter, 1 /*RCX*/, 0 /*RAX*/);
        rane_x64_emit_mov_reg_reg(&emitter, 2 /*RDX*/, 1 /*RCX*/);

        uint64_t patch_start = emitter.offset;
        uint8_t placeholder[9] = { 0xE8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90 };
        rane_x64_emit_bytes(&emitter, placeholder, sizeof(placeholder));

        if (ctx && ctx->call_fixups && ctx->call_fixup_count < ctx->call_fixup_capacity) {
          rane_aot_call_fixup_t* fixups_out = (rane_aot_call_fixup_t*)ctx->call_fixups;
          rane_aot_call_fixup_t* f = &fixups_out[ctx->call_fixup_count++];
          strcpy_s(f->sym, sizeof(f->sym), inst->operands[0].lbl);
          f->code_offset = (uint32_t)patch_start;
        }
        continue;
      }

      if (inst->opcode == TIR_RET_VAL) {
        if (inst->operand_count == 1 && inst->operands[0].kind == TIR_OPERAND_R) {
          if (inst->operands[0].r != 0) {
            rane_x64_emit_mov_reg_reg(&emitter, 0 /*RAX*/, (uint8_t)inst->operands[0].r);
          }
        }
      }

      if (inst->opcode == TIR_RET || inst->opcode == TIR_RET_VAL) {
        uint32_t locals_bytes = func->stack_slot_count * 8;
        uint32_t aligned = (locals_bytes + 15u) & ~15u;
        if (aligned) {
          rane_x64_emit_add_rsp_imm32(&emitter, aligned);
        }
        uint8_t rex = 0x48;
        uint8_t op = 0x89;
        uint8_t modrm = 0xEC;
        rane_x64_emit_bytes(&emitter, &rex, 1);
        rane_x64_emit_bytes(&emitter, &op, 1);
        rane_x64_emit_bytes(&emitter, &modrm, 1);
        rane_x64_emit_pop_reg(&emitter, 5);
        rane_x64_emit_ret(&emitter);
        continue;
      }
    }
  };

  // Emit main first if present
  const rane_tir_function_t* mainf = NULL;
  for (uint32_t i = 0; i < tir_module->function_count; i++) {
    if (strcmp(tir_module->functions[i].name, "main") == 0) {
      mainf = &tir_module->functions[i];
      break;
    }
  }
  if (mainf) emit_function(mainf);

  for (uint32_t i = 0; i < tir_module->function_count; i++) {
    const rane_tir_function_t* f = &tir_module->functions[i];
    if (mainf && f == mainf) continue;
    emit_function(f);
  }

  // Patch rel32 (jmp/jcc/call-local)
  for (uint32_t fi = 0; fi < fixup_count; fi++) {
    uint64_t target_off = 0;
    if (!find_label_offset(labels, label_count, fixups[fi].label, &target_off)) continue;

    uint64_t patch_off = fixups[fi].patch_offset;
    uint64_t next_ip = patch_off + 4;
    int32_t rel = (int32_t)((int64_t)target_off - (int64_t)next_ip);
    memcpy(emitter.buffer + patch_off, &rel, 4);
  }

  ctx->code_size = emitter.offset;
  return RANE_OK;
}

// Preferred/public API name
rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx) {
  return rane_codegen_tir_to_x64(tir_module, ctx);
}