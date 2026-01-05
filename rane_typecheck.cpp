#include "rane_typecheck.h"
#include "rane_diag.h"
#include <string.h>
#include <stdlib.h>

typedef struct rane_sym_s {
  char name[64];
  rane_type_e type;
} rane_sym_t;

typedef struct rane_scope_s {
  rane_sym_t syms[256];
  uint32_t count;
  struct rane_scope_s* parent;
} rane_scope_t;

static void diag_set(rane_diag_t* d, rane_diag_code_t code, rane_span_t span, const char* msg) {
  if (!d) return;
  d->code = code;
  d->span = span;
  strncpy_s(d->message, sizeof(d->message), msg ? msg : "", _TRUNCATE);
}

static rane_scope_t* scope_push(rane_scope_t* parent) {
  rane_scope_t* s = (rane_scope_t*)calloc(1, sizeof(rane_scope_t));
  if (!s) return NULL;
  s->parent = parent;
  return s;
}

static void scope_free_chain(rane_scope_t* s) {
  while (s) {
    rane_scope_t* p = s->parent;
    free(s);
    s = p;
  }
}

static rane_sym_t* scope_lookup(rane_scope_t* s, const char* name) {
  for (rane_scope_t* cur = s; cur; cur = cur->parent) {
    for (uint32_t i = 0; i < cur->count; i++) {
      if (strcmp(cur->syms[i].name, name) == 0) return &cur->syms[i];
    }
  }
  return NULL;
}

static int scope_lookup_local(rane_scope_t* s, const char* name) {
  if (!s) return 0;
  for (uint32_t i = 0; i < s->count; i++) {
    if (strcmp(s->syms[i].name, name) == 0) return 1;
  }
  return 0;
}

static int scope_define(rane_scope_t* s, const char* name, rane_type_e type) {
  if (!s || !name) return 0;
  if (s->count >= 256) return 0;
  strncpy_s(s->syms[s->count].name, sizeof(s->syms[s->count].name), name, _TRUNCATE);
  s->syms[s->count].type = type;
  s->count++;
  return 1;
}

typedef struct rane_tc_caps_s {
  int allow_heap;
} rane_tc_caps_t;

static rane_type_e infer_expr_type(rane_expr_t* e, rane_scope_t* scope, rane_diag_t* diag,
                                  const rane_tc_caps_t* caps);

static rane_type_e unify_num_types(rane_type_e a, rane_type_e b) {
  // Bootstrap: treat all numeric ops as u64 for now.
  (void)a; (void)b;
  return RANE_TYPE_U64;
}

static int sym_is_policy_import(const rane_stmt_t* s, const char* dotted) {
  if (!s || s->kind != STMT_IMPORT_DECL) return 0;
  return (strcmp(s->import_decl.sym, dotted) == 0);
}

static void scan_caps_from_ast(const rane_stmt_t* s, rane_tc_caps_t* caps) {
  if (!s || !caps) return;
  if (s->kind == STMT_BLOCK) {
    for (uint32_t i = 0; i < s->block.stmt_count; i++) scan_caps_from_ast(s->block.stmts[i], caps);
    return;
  }
  // v1 policy: heap only allowed when importing sys.alloc
  if (sym_is_policy_import(s, "sys.alloc")) caps->allow_heap = 1;
}

static rane_type_e infer_expr_type(rane_expr_t* e, rane_scope_t* scope, rane_diag_t* diag,
                                  const rane_tc_caps_t* caps) {
  if (!e) return RANE_TYPE_U64;
  switch (e->kind) {
    case EXPR_LIT_INT:
      return e->lit_int.type;
    case EXPR_LIT_BOOL:
      return RANE_TYPE_B1;
    case EXPR_LIT_TEXT:
      return RANE_TYPE_TEXT;
    case EXPR_LIT_BYTES:
      return RANE_TYPE_BYTES;
    case EXPR_VAR: {
      rane_sym_t* sym = scope_lookup(scope, e->var.name);
      if (!sym) {
        diag_set(diag, RANE_DIAG_UNDEFINED_NAME, e->span, "Undefined variable");
        return RANE_TYPE_U64;
      }
      return sym->type;
    }
    case EXPR_CALL:
      // Bootstrap: treat calls as u64 return for now; `print` returns u64.
      if (e->call.callee) {
        (void)infer_expr_type(e->call.callee, scope, diag, caps);
        if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_U64;
      }
      for (uint32_t i = 0; i < e->call.arg_count; i++) {
        (void)infer_expr_type(e->call.args[i], scope, diag, caps);
        if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_U64;
      }
      return RANE_TYPE_U64;

    case EXPR_MEMBER:
      (void)infer_expr_type(e->member.base, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_U64;
      // No struct typing in bootstrap; member access is pointer-sized placeholder.
      return RANE_TYPE_U64;

    case EXPR_INDEX: {
      (void)infer_expr_type(e->index.base, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_U64;
      (void)infer_expr_type(e->index.index, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_U64;
      // No array typing in bootstrap.
      return RANE_TYPE_U64;
    }
    case EXPR_UNARY: {
      rane_type_e t = infer_expr_type(e->unary.expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return t;
      if (e->unary.op == UN_NOT) return RANE_TYPE_B1;
      return t;
    }
    case EXPR_BINARY: {
      rane_type_e lt = infer_expr_type(e->binary.left, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return lt;
      rane_type_e rt = infer_expr_type(e->binary.right, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return rt;

      switch (e->binary.op) {
        case BIN_LT:
        case BIN_LE:
        case BIN_GT:
        case BIN_GE:
        case BIN_EQ:
        case BIN_NE:
          return RANE_TYPE_B1;
        case BIN_LOGAND:
        case BIN_LOGOR:
          // Require booleans
          if (lt != RANE_TYPE_B1 || rt != RANE_TYPE_B1) {
            diag_set(diag, RANE_DIAG_TYPE_MISMATCH, e->span, "and/or require boolean operands");
            return RANE_TYPE_B1;
          }
          return RANE_TYPE_B1;
        default:
          return unify_num_types(lt, rt);
      }
    }
    case EXPR_LIT_NULL:
      // Bootstrap: represent null as P64 (0).
      return RANE_TYPE_P64;
    case EXPR_TERNARY: {
      rane_type_e ct = infer_expr_type(e->ternary.cond, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return ct;
      // Condition must be bool-like.
      if (ct != RANE_TYPE_B1 && ct != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, e->span, "Ternary condition must be bool-like");
        return RANE_TYPE_U64;
      }
      rane_type_e tt = infer_expr_type(e->ternary.then_expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return tt;
      rane_type_e ft = infer_expr_type(e->ternary.else_expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return ft;
      // Bootstrap: require equal types.
      if (tt != ft) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, e->span, "Ternary branches must have same type");
        return tt;
      }
      return tt;
    }
    case EXPR_ADDR: {
      // Address expression: always yields a pointer-sized value.
      (void)infer_expr_type(e->addr.base, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_P64;
      if (e->addr.index) {
        (void)infer_expr_type(e->addr.index, scope, diag, caps);
        if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_P64;
      }
      return RANE_TYPE_P64;
    }
    case EXPR_MMIO_ADDR: {
      // Region base + offset => pointer.
      (void)infer_expr_type(e->mmio_addr.offset, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_P64;
      return RANE_TYPE_P64;
    }
    case EXPR_LOAD: {
      // load(type, addr) => type
      rane_type_e at = infer_expr_type(e->load.addr_expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return e->load.type;
      if (at != RANE_TYPE_P64 && at != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, e->span, "load requires pointer-like address");
        return e->load.type;
      }
      return e->load.type;
    }
    case EXPR_STORE: {
      // store(type, addr, value) => type (returns stored value)
      rane_type_e at = infer_expr_type(e->store.addr_expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return e->store.type;
      if (at != RANE_TYPE_P64 && at != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, e->span, "store requires pointer-like address");
        return e->store.type;
      }

      rane_type_e vt = infer_expr_type(e->store.value_expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return e->store.type;
      // Bootstrap: enforce exact match.
      if (vt != e->store.type) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, e->span, "store value type mismatch");
        return e->store.type;
      }
      return e->store.type;
    }
    case EXPR_LIT_IDENT:
      // Bootstrap: treat as pointer-sized symbol (resolved later).
      return RANE_TYPE_P64;

    default:
      return RANE_TYPE_U64;
  }
}

static rane_error_t typecheck_stmt(rane_stmt_t* s, rane_scope_t* scope, rane_diag_t* diag,
                                  const rane_tc_caps_t* caps) {
  if (!s) return RANE_E_INVALID_ARG;
  if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;

  switch (s->kind) {
    case STMT_BLOCK: {
      rane_scope_t* inner = scope_push(scope);
      if (!inner) { diag_set(diag, RANE_DIAG_INTERNAL_ERROR, s->span, "Out of memory"); return RANE_E_OS_API_FAIL; }
      for (uint32_t i = 0; i < s->block.stmt_count; i++) {
        rane_error_t e = typecheck_stmt(s->block.stmt_count ? s->block.stmts[i] : NULL, inner, diag, caps);
        if (e != RANE_OK) { scope_free_chain(inner); return e; }
      }
      rane_scope_t* parent = inner->parent;
      inner->parent = NULL;
      scope_free_chain(inner);
      (void)parent;
      return RANE_OK;
    }

    case STMT_LET: {
      if (scope_lookup_local(scope, s->let.name)) {
        diag_set(diag, RANE_DIAG_REDECLARED_NAME, s->span, "Redeclared variable in same scope");
        return RANE_E_INVALID_ARG;
      }
      rane_type_e t = infer_expr_type(s->let.expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
      s->let.type = t;
      if (!scope_define(scope, s->let.name, t)) {
        diag_set(diag, RANE_DIAG_INTERNAL_ERROR, s->span, "Symbol table overflow");
        return RANE_E_INVALID_ARG;
      }
      return RANE_OK;
    }

    case STMT_ASSIGN: {
      // Assignment target "_" is used for expression/call statements.
      if (strcmp(s->assign.target, "_") == 0) {
        (void)infer_expr_type(s->assign.expr, scope, diag, caps);
        return (diag && diag->code != RANE_DIAG_OK) ? RANE_E_INVALID_ARG : RANE_OK;
      }

      // v1: declare-if-missing
      rane_sym_t* sym = scope_lookup(scope, s->assign.target);
      rane_type_e rhs = infer_expr_type(s->assign.expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;

      if (!sym) {
        if (!scope_define(scope, s->assign.target, rhs)) {
          diag_set(diag, RANE_DIAG_INTERNAL_ERROR, s->span, "Symbol table overflow");
          return RANE_E_INVALID_ARG;
        }
        return RANE_OK;
      }

      if (rhs != sym->type) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->span, "Type mismatch in assignment");
        return RANE_E_INVALID_ARG;
      }
      return RANE_OK;
    }

    case STMT_IF: {
      rane_type_e ct = infer_expr_type(s->if_stmt.cond, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
      if (ct != RANE_TYPE_B1 && ct != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->if_stmt.cond->span, "Condition must be bool-like");
        return RANE_E_INVALID_ARG;
      }
      if (s->if_stmt.then_branch) {
        rane_error_t e = typecheck_stmt(s->if_stmt.then_branch, scope, diag, caps);
        if (e != RANE_OK) return e;
      }
      if (s->if_stmt.else_branch) {
        rane_error_t e = typecheck_stmt(s->if_stmt.else_branch, scope, diag, caps);
        if (e != RANE_OK) return e;
      }
      return RANE_OK;
    }

    case STMT_WHILE: {
      rane_type_e ct = infer_expr_type(s->while_stmt.cond, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
      if (ct != RANE_TYPE_B1 && ct != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->while_stmt.cond->span, "Condition must be bool-like");
        return RANE_E_INVALID_ARG;
      }
      if (s->while_stmt.body) return typecheck_stmt(s->while_stmt.body, scope, diag, caps);
      return RANE_OK;
    }

    case STMT_PROC: {
      rane_scope_t* inner = scope_push(scope);
      if (!inner) { diag_set(diag, RANE_DIAG_INTERNAL_ERROR, s->span, "Out of memory"); return RANE_E_OS_API_FAIL; }
      for (uint32_t i = 0; i < s->proc.param_count; i++) {
        scope_define(inner, s->proc.params[i], RANE_TYPE_U64);
      }
      rane_error_t e = typecheck_stmt(s->proc.body, inner, diag, caps);
      rane_scope_t* parent = inner->parent;
      inner->parent = NULL;
      scope_free_chain(inner);
      (void)parent;
      return e;
    }

    case STMT_RETURN: {
      if (s->ret.expr) {
        (void)infer_expr_type(s->ret.expr, scope, diag, caps);
      }
      return (diag && diag->code != RANE_DIAG_OK) ? RANE_E_INVALID_ARG : RANE_OK;
    }

    case STMT_MODULE:
      return RANE_OK;

    case STMT_NODE: {
      if (s->node_decl.body) return typecheck_stmt(s->node_decl.body, scope, diag, caps);
      return RANE_OK;
    }

    case STMT_START_AT:
    case STMT_GO_NODE:
      return RANE_OK;

    case STMT_IMPORT_DECL:
      // policy imports are handled in the caps scan; accept.
      return RANE_OK;

    case STMT_SAY: {
      if (!s->say.expr) return RANE_OK;

      rane_type_e t = infer_expr_type(s->say.expr, scope, diag, caps);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;

      // v1: `say` only accepts text|bytes.
      if (t != RANE_TYPE_TEXT && t != RANE_TYPE_BYTES) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->span, "say expects text or bytes");
        return RANE_E_INVALID_ARG;
      }

      // Policy: if heap is forbidden, reject string literals (parser currently allocates those).
      if (caps && !caps->allow_heap) {
        if (s->say.expr->kind == EXPR_LIT_INT && s->say.expr->lit_int.type == RANE_TYPE_TEXT) {
          diag_set(diag, RANE_DIAG_SECURITY_VIOLATION, s->span, "Heap use forbidden (import sys.alloc to allow string literals)");
          return RANE_E_INVALID_ARG;
        }
      }

      return RANE_OK;
    }

    default:
      // Existing behavior
      return RANE_OK;
  }
}

rane_error_t rane_typecheck_ast_ex(rane_stmt_t* ast_root, rane_diag_t* out_diag) {
  if (out_diag) {
    out_diag->code = RANE_DIAG_OK;
    out_diag->span = rane_span_from_token(0, 0, 0);
    out_diag->message[0] = 0;
  }

  if (!ast_root) {
    diag_set(out_diag, RANE_DIAG_INTERNAL_ERROR, rane_span_from_token(0, 0, 0), "Null AST");
    return RANE_E_INVALID_ARG;
  }

  rane_tc_caps_t caps = {};
  scan_caps_from_ast(ast_root, &caps);

  rane_scope_t* root = scope_push(NULL);
  if (!root) {
    diag_set(out_diag, RANE_DIAG_INTERNAL_ERROR, ast_root->span, "Out of memory");
    return RANE_E_OS_API_FAIL;
  }

  rane_error_t e = typecheck_stmt(ast_root, root, out_diag, &caps);
  scope_free_chain(root);

  if (out_diag && out_diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
  return e;
}

rane_error_t rane_typecheck_ast(rane_stmt_t* ast_root) {
  return rane_typecheck_ast_ex(ast_root, NULL);
}