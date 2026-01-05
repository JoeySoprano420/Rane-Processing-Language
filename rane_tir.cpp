#include "rane_tir.h"
#include "rane_x64.h"
#include "rane_ast.h"
#include "rane_aot.h"
#include "rane_label.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

// Scratch slots for lowering (used for call argument staging).
// These are allocated from the same per-function slot counter so they reserve
// space in the stack frame.
static uint32_t alloc_scratch_slot() {
  return g_next_slot++;
}

static void ensure_stack_slots(rane_tir_function_t* func) {
  if (!func) return;
  if (func->stack_slot_count < g_next_slot) func->stack_slot_count = g_next_slot;
}

static uint32_t lower_expr_to_scratch_slot(rane_expr_t* expr, rane_tir_function_t* func) {
  lower_expr_to_tir(expr, func);
  uint32_t slot = alloc_scratch_slot();
  emit_st_r0_to_slot(func, slot);
  ensure_stack_slots(func);
  return slot;
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

static void emit_call_prep(rane_tir_function_t* func, uint32_t stack_arg_bytes) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_CALL_PREP;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_IMM;
  inst.operands[0].imm = (uint64_t)stack_arg_bytes;
  inst.operands[1].kind = TIR_OPERAND_IMM;
  inst.operands[1].imm = 0;
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

static void emit_import_decl(rane_tir_function_t* func, const char* sym) {
  if (!func || !sym) return;
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_IMPORT;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), sym);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

static void emit_export_decl(rane_tir_function_t* func, const char* sym) {
  if (!func || !sym) return;
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_EXPORT;
  inst.type = RANE_TYPE_U64;
  inst.operands[0].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[0].lbl, sizeof(inst.operands[0].lbl), sym);
  inst.operand_count = 1;
  func->insts[func->inst_count++] = inst;
}

// --- module-level constant pool for text literals (bootstrap) ---
typedef struct rane_text_lit_s {
  uint64_t hash;
  char label[64];
  const char* start;
  uint32_t length;
} rane_text_lit_t;

static rane_text_lit_t g_text_lits[512];
static uint32_t g_text_lit_count = 0;

static void text_lits_reset() { g_text_lit_count = 0; }

static uint64_t fnv1a64(const char* s, uint32_t n) {
  uint64_t h = 1469598103934665603ull;
  for (uint32_t i = 0; i < n; i++) {
    h ^= (uint8_t)s[i];
    h *= 1099511628211ull;
  }
  return h;
}

static const char* text_lit_get_or_define_label(const char* start, uint32_t len) {
  if (!start) start = "";
  uint64_t h = fnv1a64(start, len);

  for (uint32_t i = 0; i < g_text_lit_count; i++) {
    if (g_text_lits[i].hash == h && g_text_lits[i].length == len && memcmp(g_text_lits[i].start, start, len) == 0) {
      return g_text_lits[i].label;
    }
  }

  if (g_text_lit_count >= 512) return "";

  rane_text_lit_t* e = &g_text_lits[g_text_lit_count++];
  memset(e, 0, sizeof(*e));
  e->hash = h;
  e->start = start;
  e->length = len;
  sprintf_s(e->label, sizeof(e->label), "__rane_str_%u", (unsigned)(g_text_lit_count - 1));
  return e->label;
}

static void emit_addr_of_label_to_r0(rane_tir_function_t* func, const char* label) {
  rane_tir_inst_t inst;
  memset(&inst, 0, sizeof(inst));
  inst.opcode = TIR_ADDR_OF;
  inst.type = RANE_TYPE_P64;
  inst.operands[0].kind = TIR_OPERAND_R;
  inst.operands[0].r = 0;
  inst.operands[1].kind = TIR_OPERAND_LBL;
  strcpy_s(inst.operands[1].lbl, sizeof(inst.operands[1].lbl), label);
  inst.operand_count = 2;
  func->insts[func->inst_count++] = inst;
}

// --- bootstrap module state for mmio regions (name -> base, size) ---
typedef struct rane_mmio_region_s {
  char name[64];
  uint64_t base;
  uint64_t size;
} rane_mmio_region_t;

static rane_mmio_region_t g_mmio_regions[64];
static uint32_t g_mmio_region_count = 0;

static void mmio_regions_reset() { g_mmio_region_count = 0; }

static int mmio_region_find(const char* name, rane_mmio_region_t* out) {
  if (!name) return 0;
  for (uint32_t i = 0; i < g_mmio_region_count; i++) {
    if (strcmp(g_mmio_regions[i].name, name) == 0) {
      if (out) *out = g_mmio_regions[i];
      return 1;
    }
  }
  return 0;
}

static void mmio_region_define(const char* name, uint64_t base, uint64_t size) {
  if (!name) return;
  // overwrite if already present
  for (uint32_t i = 0; i < g_mmio_region_count; i++) {
    if (strcmp(g_mmio_regions[i].name, name) == 0) {
      g_mmio_regions[i].base = base;
      g_mmio_regions[i].size = size;
      return;
    }
  }
  if (g_mmio_region_count < 64) {
    strncpy_s(g_mmio_regions[g_mmio_region_count].name, sizeof(g_mmio_regions[g_mmio_region_count].name), name, _TRUNCATE);
    g_mmio_regions[g_mmio_region_count].base = base;
    g_mmio_regions[g_mmio_region_count].size = size;
    g_mmio_region_count++;
  }
}

// Helper: after lower_expr_to_tir(expr) => r0 holds value, copy to a vreg.
static void emit_mov_vreg_from_r0(rane_tir_function_t* func, uint32_t dst_r) {
  rane_tir_inst_t mv;
  memset(&mv, 0, sizeof(mv));
  mv.opcode = TIR_MOV;
  mv.type = RANE_TYPE_U64;
  mv.operands[0].kind = TIR_OPERAND_R;
  mv.operands[0].r = dst_r;
  mv.operands[1].kind = TIR_OPERAND_R;
  mv.operands[1].r = 0;
  mv.operand_count = 2;
  func->insts[func->inst_count++] = mv;
}

// Helper: move vreg -> r0
static void emit_mov_r0_from_vreg(rane_tir_function_t* func, uint32_t src_r) {
  rane_tir_inst_t mv;
  memset(&mv, 0, sizeof(mv));
  mv.opcode = TIR_MOV;
  mv.type = RANE_TYPE_U64;
  mv.operands[0].kind = TIR_OPERAND_R;
  mv.operands[0].r = 0;
  mv.operands[1].kind = TIR_OPERAND_R;
  mv.operands[1].r = src_r;
  mv.operand_count = 2;
  func->insts[func->inst_count++] = mv;
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
  mmio_regions_reset();
  text_lits_reset();

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

    // Win64: first 4 args in RCX/RDX/R8/R9, remaining on stack at:
    //   [RSP + 0x20 + 8*(i-4)] in the callee at entry.
    // Our frame uses: push rbp; mov rbp,rsp. Therefore at entry:
    //   RBP == RSP_entry - 8
    // and stack args are readable at:
    //   [RBP + 0x28 + 8*(i-4)]

    for (uint32_t i = 0; i < p->proc.param_count; i++) {
      uint32_t slot = 0;
      if (!local_find_slot(p->proc.params[i], &slot)) continue;

      if (i == 0 || i == 1 || i == 2 || i == 3) {
        // Store incoming register arg to its local slot.
        rane_tir_inst_t st;
        memset(&st, 0, sizeof(st));
        st.opcode = TIR_ST;
        st.type = RANE_TYPE_U64;
        st.operands[0].kind = TIR_OPERAND_S;
        st.operands[0].s = slot;
        st.operands[1].kind = TIR_OPERAND_R;
        st.operands[1].r = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 8 : 9; // RCX,RDX,R8,R9
        st.operand_count = 2;
        f->insts[f->inst_count++] = st;
      } else {
        // Load stack arg into r0 then store into slot.
        // We represent stack arg load using TIR_LD with a special negative slot encoding?
        // Keep it explicit: use TIR_LDV with memory operand (rbp + disp) -> r0.
        // For bootstrap, we reuse TIR_MOV into r0 then patch in codegen via TIR_LDV.
        rane_tir_inst_t ldv;
        memset(&ldv, 0, sizeof(ldv));
        ldv.opcode = TIR_LDV;
        ldv.type = RANE_TYPE_U64;
        ldv.operands[0].kind = TIR_OPERAND_R;
        ldv.operands[0].r = 0;
        ldv.operands[1].kind = TIR_OPERAND_M;
        ldv.operands[1].m.base_r = 5; // RBP
        ldv.operands[1].m.index_r = 0;
        ldv.operands[1].m.scale = 1;
        ldv.operands[1].m.disp = 0x28 + (int32_t)((i - 4u) * 8u);
        ldv.operand_count = 2;
        f->insts[f->inst_count++] = ldv;

        emit_st_r0_to_slot(f, slot);
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
    mmio_regions_reset();
    lower_stmt_to_tir_skip_proc_defs(ast_root, mainf);

    rane_tir_inst_t ret_inst;
    memset(&ret_inst, 0, sizeof(ret_inst));
    ret_inst.opcode = TIR_RET;
    ret_inst.type = RANE_TYPE_U64;
    ret_inst.operand_count = 0;
    mainf->insts[mainf->inst_count++] = ret_inst;

    mainf->stack_slot_count = g_next_slot;
  }

  // TODO: module-level data emission; for bootstrap we keep literals discovered in `g_text_lits`.

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

  // --- v1 prose/node surface ---
  if (st->kind == STMT_MODULE) {
    // Module declarations are compile-time only in this bootstrap; no codegen.
    return;
  }

  if (st->kind == STMT_NODE) {
    // Represent node entry as a label and lower its body.
    emit_label(func, st->node_decl.name);
    if (st->node_decl.body) lower_stmt_to_tir(st->node_decl.body, func);
    return;
  }

  if (st->kind == STMT_START_AT) {
    // Emit a jump to the start node.
    emit_jmp(func, st->start_at.node_name);
    return;
  }

  if (st->kind == STMT_GO_NODE) {
    emit_jmp(func, st->go_node.node_name);
    return;
  }

  if (st->kind == STMT_SAY) {
    // Bootstrap: say expr => print(expr)
    rane_expr_t call;
    memset(&call, 0, sizeof(call));
    call.kind = EXPR_CALL;
    strcpy_s(call.call.name, sizeof(call.call.name), "print");
    call.call.args = (rane_expr_t**)&st->say.expr;
    call.call.arg_count = (st->say.expr != NULL) ? 1 : 0;
    lower_expr_to_tir(&call, func);
    return;
  }

  // --- New statements ---
  if (st->kind == STMT_IMPORT_DECL) {
    emit_import_decl(func, st->import_decl.sym);
    return;
  }

  if (st->kind == STMT_EXPORT_DECL) {
    emit_export_decl(func, st->export_decl.sym);
    return;
  }

  if (st->kind == STMT_MMIO_REGION_DECL) {
    // Record region for subsequent read32/write32 lowering.
    mmio_region_define(st->mmio_region_decl.name, st->mmio_region_decl.base, st->mmio_region_decl.size);
    return;
  }

  if (st->kind == STMT_MEM_COPY) {
    // Lower to import memcpy(dst, src, size)
    // args: RCX=dst, RDX=src, R8=size
    lower_expr_to_tir(st->mem_copy.dst, func);
    emit_mov_vreg_from_r0(func, 2);

    lower_expr_to_tir(st->mem_copy.src, func);
    emit_mov_vreg_from_r0(func, 3);

    lower_expr_to_tir(st->mem_copy.size, func);
    emit_mov_vreg_from_r0(func, 4);

    emit_call_prep(func, 0);

    emit_mov_r0_from_vreg(func, 2);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_P64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 1; // RCX
      mv.operands[1].kind = TIR_OPERAND_R;
      mv.operands[1].r = 0;
      mv.operand_count = 2;
      func->insts[func->inst_count++] = mv;
    }

    emit_mov_r0_from_vreg(func, 3);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_P64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 2; // RDX
      mv.operands[1].kind = TIR_OPERAND_R;
      mv.operands[1].r = 0;
      mv.operand_count = 2;
      func->insts[func->inst_count++] = mv;
    }

    emit_mov_r0_from_vreg(func, 4);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_U64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 8; // R8
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
    strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), "memcpy");
    call.operand_count = 1;
    func->insts[func->inst_count++] = call;
    return;
  }

  if (st->kind == STMT_CHAN_PUSH) {
    // No runtime channel impl yet; preserve as an explicit TIR op (currently codegen ignores it).
    // This keeps the node from being silently dropped in the IR.
    lower_expr_to_tir(st->chan_push.value, func);
    rane_tir_inst_t op;
    memset(&op, 0, sizeof(op));
    op.opcode = TIR_CHAN_PUSH;
    op.type = RANE_TYPE_U64;
    op.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(op.operands[0].lbl, sizeof(op.operands[0].lbl), st->chan_push.chan);
    op.operands[1].kind = TIR_OPERAND_R;
    op.operands[1].r = 0;
    op.operand_count = 2;
    func->insts[func->inst_count++] = op;
    return;
  }

  if (st->kind == STMT_CHAN_POP) {
    // No runtime channel impl yet; represent as TIR_CHAN_POP producing r0,
    // then assign into target slot.
    rane_tir_inst_t op;
    memset(&op, 0, sizeof(op));
    op.opcode = TIR_CHAN_POP;
    op.type = RANE_TYPE_U64;
    op.operands[0].kind = TIR_OPERAND_LBL;
    strcpy_s(op.operands[0].lbl, sizeof(op.operands[0].lbl), st->chan_pop.chan);
    op.operands[1].kind = TIR_OPERAND_R;
    op.operands[1].r = 0;
    op.operand_count = 2;
    func->insts[func->inst_count++] = op;

    uint32_t slot = 0;
    if (!local_find_slot(st->chan_pop.target, &slot)) slot = local_get_or_define_slot(st->chan_pop.target);
    emit_st_r0_to_slot(func, slot);
    return;
  }

  if (st->kind == STMT_PROC_CALL) {
    // Built-in termination ops
    if (strcmp(st->proc_call.proc_name, "__rane_halt") == 0) {
      rane_tir_inst_t h;
      memset(&h, 0, sizeof(h));
      h.opcode = TIR_HALT;
      h.type = RANE_TYPE_U64;
      h.operand_count = 0;
      func->insts[func->inst_count++] = h;
      return;
    }

    if (strcmp(st->proc_call.proc_name, "__rane_trap") == 0) {
      uint64_t code = 0;
      if (st->proc_call.arg_count >= 1 && st->proc_call.args && st->proc_call.args[0]) {
        rane_expr_t* a0 = st->proc_call.args[0];
        if (a0->kind == EXPR_LIT_INT) {
          code = a0->lit_int.value;
        } else {
          // Evaluate dynamic trap code into r0 and pass as immediate is not supported in bootstrap;
          // fall back to 1.
          code = 1;
        }
      } else {
        code = 1;
      }

      rane_tir_inst_t t;
      memset(&t, 0, sizeof(t));
      t.opcode = TIR_TRAP;
      t.type = RANE_TYPE_U64;
      t.operands[0].kind = TIR_OPERAND_IMM;
      t.operands[0].imm = code;
      t.operand_count = 1;
      func->insts[func->inst_count++] = t;
      return;
    }

    // Statement form: like expression call, but may declare an output slot.
    // Lower by synthesizing an EXPR_CALL and reusing expression lowering.

    rane_expr_t* call = (rane_expr_t*)malloc(sizeof(rane_expr_t));
    memset(call, 0, sizeof(*call));
    call->span = st->span;
    call->kind = EXPR_CALL;
    strncpy_s(call->call.name, sizeof(call->call.name), st->proc_call.proc_name, _TRUNCATE);
    call->call.args = st->proc_call.args;
    call->call.arg_count = st->proc_call.arg_count;

    lower_expr_to_tir(call, func);

    // into slot N => store return value into local slot name "slotN"
    // (bootstrap convention; later this should be explicit virtual slot space)
    if (st->proc_call.slot != 0) {
      char tmp[64];
      sprintf_s(tmp, sizeof(tmp), "slot%u", (unsigned)st->proc_call.slot);
      uint32_t slot = local_get_or_define_slot(tmp);
      emit_st_r0_to_slot(func, slot);
    }

    free(call);
    return;
  }

  if (st->kind == STMT_CJUMP) {
    // cond jump: if cond != 0 -> true_marker else -> false_marker
    lower_expr_to_tir(st->cjump.cond, func);
    emit_cmp_r0_imm(func, 0);

    emit_jcc_ext(func, TIR_CC_NE, st->cjump.true_marker);
    emit_jmp(func, st->cjump.false_marker);
    return;
  }

  // --- Existing statements ---
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
    // If this is a callee expression call (postfix call), and we don't have a runtime to call function pointers yet,
    // lower only if it is a simple var callee: foo(...)
    if (expr->call.callee && expr->call.name[0] == 0) {
      if (expr->call.callee->kind == EXPR_VAR) {
        strncpy_s(expr->call.name, sizeof(expr->call.name), expr->call.callee->var.name, _TRUNCATE);
      }
    }

    // v0 builtin: print(x) -> rane_rt_print(x)
    if (strcmp(expr->call.name, "print") == 0) {
      uint32_t arg0_slot = 0;

      if (expr->call.arg_count >= 1) {
        arg0_slot = lower_expr_to_scratch_slot(expr->call.args[0], func);
      } else {
        // print() with no args => print empty string
        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_P64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 0;
        mv.operands[1].kind = TIR_OPERAND_IMM;
        mv.operands[1].imm = (uint64_t)(uintptr_t)"";
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
        arg0_slot = alloc_scratch_slot();
        emit_st_r0_to_slot(func, arg0_slot);
        ensure_stack_slots(func);
      }

      // Prepare call frame (no stack args)
      emit_call_prep(func, 0);

      // Load arg0 -> RCX
      emit_ld_slot_to_r0(func, arg0_slot);
      {
        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_P64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = 1; // RCX
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
      strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), "rane_rt_print");
      call.operand_count = 1;
      func->insts[func->inst_count++] = call;
      return;
    }

    // User-defined proc call? (lower via scratch slots)
    if (procs_find(expr->call.name)) {
      const uint32_t argc = expr->call.arg_count;
      const uint32_t reg_arg_count = (argc < 4u) ? argc : 4u;
      const uint32_t stack_arg_count = (argc > 4u) ? (argc - 4u) : 0u;
      const uint32_t stack_arg_bytes = stack_arg_count * 8u;

      // Evaluate all args left-to-right into scratch slots first.
      uint32_t arg_slots[64];
      uint32_t arg_slot_count = 0;
      if (argc > 64u) {
        emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
        return;
      }
      for (uint32_t i = 0; i < argc; i++) {
        arg_slots[arg_slot_count++] = lower_expr_to_scratch_slot(expr->call.args[i], func);
      }

      emit_call_prep(func, stack_arg_bytes);

      // Stack args (arg4+)
      for (uint32_t i = 4; i < argc; i++) {
        emit_ld_slot_to_r0(func, arg_slots[i]);

        rane_tir_inst_t st;
        memset(&st, 0, sizeof(st));
        st.opcode = TIR_ST;
        st.type = RANE_TYPE_U64;
        st.operands[0].kind = TIR_OPERAND_S;
        st.operands[0].s = 0x80000000u | (i - 4u);
        st.operands[1].kind = TIR_OPERAND_R;
        st.operands[1].r = 0;
        st.operand_count = 2;
        func->insts[func->inst_count++] = st;
      }

      // Register args (arg0-3)
      for (uint32_t i = 0; i < reg_arg_count; i++) {
        emit_ld_slot_to_r0(func, arg_slots[i]);
        uint32_t dst = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 8 : 9;

        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_U64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = dst;
        mv.operands[1].kind = TIR_OPERAND_R;
        mv.operands[1].r = 0;
        mv.operand_count = 2;
        func->insts[func->inst_count++] = mv;
      }

      emit_call_local(func, expr->call.name);
      return;
    }

    // Otherwise treat as import (lower via scratch slots)
    {
      const uint32_t argc = expr->call.arg_count;
      const uint32_t reg_arg_count = (argc < 4u) ? argc : 4u;
      const uint32_t stack_arg_count = (argc > 4u) ? (argc - 4u) : 0u;
      const uint32_t stack_arg_bytes = stack_arg_count * 8u;

      uint32_t arg_slots[64];
      uint32_t arg_slot_count = 0;
      if (argc > 64u) {
        emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
        return;
      }
      for (uint32_t i = 0; i < argc; i++) {
        arg_slots[arg_slot_count++] = lower_expr_to_scratch_slot(expr->call.args[i], func);
      }

      emit_call_prep(func, stack_arg_bytes);

      for (uint32_t i = 4; i < argc; i++) {
        emit_ld_slot_to_r0(func, arg_slots[i]);

        rane_tir_inst_t st;
        memset(&st, 0, sizeof(st));
        st.opcode = TIR_ST;
        st.type = RANE_TYPE_U64;
        st.operands[0].kind = TIR_OPERAND_S;
        st.operands[0].s = 0x80000000u | (i - 4u);
        st.operands[1].kind = TIR_OPERAND_R;
        st.operands[1].r = 0;
        st.operand_count = 2;
        func->insts[func->inst_count++] = st;
      }

      for (uint32_t i = 0; i < reg_arg_count; i++) {
        emit_ld_slot_to_r0(func, arg_slots[i]);
        uint32_t dst = (i == 0) ? 1 : (i == 1) ? 2 : (i == 2) ? 8 : 9;

        rane_tir_inst_t mv;
        memset(&mv, 0, sizeof(mv));
        mv.opcode = TIR_MOV;
        mv.type = RANE_TYPE_U64;
        mv.operands[0].kind = TIR_OPERAND_R;
        mv.operands[0].r = dst;
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
      strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), expr->call.name);
      call.operand_count = 1;
      func->insts[func->inst_count++] = call;
      return;
    }
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

  if (expr->kind == EXPR_CHOOSE) {
    // choose max(a,b) / choose min(a,b)

    lower_expr_to_tir(expr->choose.a, func);
    emit_mov_vreg_from_r0(func, 2);

    lower_expr_to_tir(expr->choose.b, func);
    emit_mov_vreg_from_r0(func, 1);

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

    char lbl_pick_a[64];
    char lbl_pick_b[64];
    char lbl_end[64];
    rane_label_gen_make(&g_lbl, lbl_pick_a);
    rane_label_gen_make(&g_lbl, lbl_pick_b);
    rane_label_gen_make(&g_lbl, lbl_end);

    // In `rane_ast.h` choose.kind is an anonymous enum { CHOOSE_MAX=0, CHOOSE_MIN=1 }.
    // MSVC doesn't let us qualify the enumerators here cleanly; compare by value.
    int want_min = ((int)expr->choose.kind == 1);
    if (!want_min) {
      emit_jcc_ext(func, TIR_CC_GE, lbl_pick_a);
    } else {
      emit_jcc_ext(func, TIR_CC_LE, lbl_pick_a);
    }
    emit_jmp(func, lbl_pick_b);

    emit_label(func, lbl_pick_a);
    emit_mov_r0_from_vreg(func, 2);
    emit_jmp(func, lbl_end);

    emit_label(func, lbl_pick_b);
    emit_mov_r0_from_vreg(func, 1);
    emit_label(func, lbl_end);
    return;
  }

  if (expr->kind == EXPR_LOAD) {
    // Bootstrap: lower load as call to imported builtin: rane_load(type, addr)
    // args: RCX=type, RDX=addr
    lower_expr_to_tir(expr->load.addr_expr, func);
    emit_mov_vreg_from_r0(func, 1);

    emit_call_prep(func, 0);
    // RCX = type enum
    emit_mov_r0_imm(func, (uint64_t)expr->load.type, RANE_TYPE_U64);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_P64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 1; // RCX
      mv.operands[1].kind = TIR_OPERAND_R;
      mv.operands[1].r = 0;
      mv.operand_count = 2;
      func->insts[func->inst_count++] = mv;
    }
    // RDX = addr
    emit_mov_r0_from_vreg(func, 1);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_P64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 2;
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
    strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), "rane_load");
    call.operand_count = 1;
    func->insts[func->inst_count++] = call;
    return;
  }

  if (expr->kind == EXPR_STORE) {
    // Bootstrap: lower store as call to imported builtin: rane_store(type, addr, value)
    // args: RCX=type, RDX=addr, R8=value
    lower_expr_to_tir(expr->store.addr_expr, func);
    emit_mov_vreg_from_r0(func, 2);

    lower_expr_to_tir(expr->store.value_expr, func);
    emit_mov_vreg_from_r0(func, 8);

    emit_call_prep(func, 0);

    // RCX = type
    emit_mov_r0_imm(func, (uint64_t)expr->store.type, RANE_TYPE_U64);
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

    // RDX = addr
    emit_mov_r0_from_vreg(func, 2);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_P64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 2;
      mv.operands[1].kind = TIR_OPERAND_R;
      mv.operands[1].r = 0;
      mv.operand_count = 2;
      func->insts[func->inst_count++] = mv;
    }

    // R8 = value
    emit_mov_r0_from_vreg(func, 8);
    {
      rane_tir_inst_t mv;
      memset(&mv, 0, sizeof(mv));
      mv.opcode = TIR_MOV;
      mv.type = RANE_TYPE_U64;
      mv.operands[0].kind = TIR_OPERAND_R;
      mv.operands[0].r = 8;
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
    strcpy_s(call.operands[0].lbl, sizeof(call.operands[0].lbl), "rane_store");
    call.operand_count = 1;
    func->insts[func->inst_count++] = call;

    // result: stored value
    emit_mov_r0_from_vreg(func, 8);
    return;
  }

  // Restore remaining lowering cases (ADDR, LIT_IDENT, MMIO_ADDR, fallthrough)
  if (expr->kind == EXPR_ADDR) {
    // base + index*scale + disp
    // Evaluate base -> r0, save r2
    lower_expr_to_tir(expr->addr.base, func);
    emit_mov_vreg_from_r0(func, 2);

    // index -> r0, save r1
    if (expr->addr.index) {
      lower_expr_to_tir(expr->addr.index, func);
      emit_mov_vreg_from_r0(func, 1);
    } else {
      emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
      emit_mov_vreg_from_r0(func, 1);
    }

    // r0 = r1 * scale
    emit_mov_r0_from_vreg(func, 1);
    if (expr->addr.scale != 1) {
      rane_tir_inst_t mul;
      memset(&mul, 0, sizeof(mul));
      mul.opcode = TIR_MUL;
      mul.type = RANE_TYPE_U64;
      mul.operands[0].kind = TIR_OPERAND_R;
      mul.operands[0].r = 0;
      mul.operands[1].kind = TIR_OPERAND_IMM;
      mul.operands[1].imm = expr->addr.scale;
      mul.operand_count = 2;
      func->insts[func->inst_count++] = mul;
    }

    // r0 = r2 + r0
    {
      rane_tir_inst_t add;
      memset(&add, 0, sizeof(add));
      add.opcode = TIR_ADD;
      add.type = RANE_TYPE_U64;
      add.operands[0].kind = TIR_OPERAND_R;
      add.operands[0].r = 0;
      add.operands[1].kind = TIR_OPERAND_R;
      add.operands[1].r = 2;
      add.operand_count = 2;
      func->insts[func->inst_count++] = add;
    }

    if (expr->addr.disp != 0) {
      rane_tir_inst_t add;
      memset(&add, 0, sizeof(add));
      add.opcode = TIR_ADD;
      add.type = RANE_TYPE_U64;
      add.operands[0].kind = TIR_OPERAND_R;
      add.operands[0].r = 0;
      add.operands[1].kind = TIR_OPERAND_IMM;
      add.operands[1].imm = expr->addr.disp;
      add.operand_count = 2;
      func->insts[func->inst_count++] = add;
    }
    return;
  }

  if (expr->kind == EXPR_LIT_IDENT) {
    // Ident literals are currently lowered as 0 (to be resolved by linker/import table later)
    emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
    return;
  }

  if (expr->kind == EXPR_MMIO_ADDR) {
    // Convert region+offset into an absolute pointer address.
    // Prefer using recorded module-level mmio region table (from STMT_MMIO_REGION_DECL).
    rane_mmio_region_t reg = {};
    if (!mmio_region_find(expr->mmio_addr.region, &reg)) {
      // Unknown region => treat as null pointer.
      emit_mov_r0_imm(func, 0, RANE_TYPE_P64);
      return;
    }

    // r0 = offset
    lower_expr_to_tir(expr->mmio_addr.offset, func);

    // r0 = r0 + reg.base
    {
      rane_tir_inst_t add;
      memset(&add, 0, sizeof(add));
      add.opcode = TIR_ADD;
      add.type = RANE_TYPE_U64;
      add.operands[0].kind = TIR_OPERAND_R;
      add.operands[0].r = 0;
      add.operands[1].kind = TIR_OPERAND_IMM;
      add.operands[1].imm = reg.base;
      add.operand_count = 2;
      func->insts[func->inst_count++] = add;
    }

    return;
  }

  // Fallback
  emit_mov_r0_imm(func, 0, RANE_TYPE_U64);
}