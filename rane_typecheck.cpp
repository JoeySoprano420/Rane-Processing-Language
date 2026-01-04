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

static rane_type_e infer_expr_type(rane_expr_t* e, rane_scope_t* scope, rane_diag_t* diag);

static rane_type_e unify_num_types(rane_type_e a, rane_type_e b) {
  // Bootstrap: treat all numeric ops as u64 for now.
  (void)a; (void)b;
  return RANE_TYPE_U64;
}

static rane_type_e infer_expr_type(rane_expr_t* e, rane_scope_t* scope, rane_diag_t* diag) {
  if (!e) return RANE_TYPE_U64;
  switch (e->kind) {
    case EXPR_LIT_INT:
      return e->lit_int.type;
    case EXPR_LIT_BOOL:
      return RANE_TYPE_B1;
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
      for (uint32_t i = 0; i < e->call.arg_count; i++) {
        (void)infer_expr_type(e->call.args[i], scope, diag);
        if (diag && diag->code != RANE_DIAG_OK) return RANE_TYPE_U64;
      }
      return RANE_TYPE_U64;
    case EXPR_UNARY: {
      rane_type_e t = infer_expr_type(e->unary.expr, scope, diag);
      if (diag && diag->code != RANE_DIAG_OK) return t;
      if (e->unary.op == UN_NOT) return RANE_TYPE_B1;
      return t;
    }
    case EXPR_BINARY: {
      rane_type_e lt = infer_expr_type(e->binary.left, scope, diag);
      if (diag && diag->code != RANE_DIAG_OK) return lt;
      rane_type_e rt = infer_expr_type(e->binary.right, scope, diag);
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
    default:
      return RANE_TYPE_U64;
  }
}

static rane_error_t typecheck_stmt(rane_stmt_t* s, rane_scope_t* scope, rane_diag_t* diag) {
  if (!s) return RANE_E_INVALID_ARG;
  if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;

  switch (s->kind) {
    case STMT_BLOCK: {
      rane_scope_t* inner = scope_push(scope);
      if (!inner) { diag_set(diag, RANE_DIAG_INTERNAL_ERROR, s->span, "Out of memory"); return RANE_E_OS_API_FAIL; }
      for (uint32_t i = 0; i < s->block.stmt_count; i++) {
        rane_error_t e = typecheck_stmt(s->block.stmts[i], inner, diag);
        if (e != RANE_OK) { scope_free_chain(inner); return e; }
      }
      // Pop only `inner`
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
      rane_type_e t = infer_expr_type(s->let.expr, scope, diag);
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
        (void)infer_expr_type(s->assign.expr, scope, diag);
        return (diag && diag->code != RANE_DIAG_OK) ? RANE_E_INVALID_ARG : RANE_OK;
      }

      rane_sym_t* sym = scope_lookup(scope, s->assign.target);
      if (!sym) {
        diag_set(diag, RANE_DIAG_UNDEFINED_NAME, s->span, "Assignment to undefined variable");
        return RANE_E_INVALID_ARG;
      }
      rane_type_e rhs = infer_expr_type(s->assign.expr, scope, diag);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
      if (rhs != sym->type) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->span, "Type mismatch in assignment");
        return RANE_E_INVALID_ARG;
      }
      return RANE_OK;
    }

    case STMT_IF: {
      rane_type_e ct = infer_expr_type(s->if_stmt.cond, scope, diag);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
      if (ct != RANE_TYPE_B1 && ct != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->if_stmt.cond->span, "Condition must be bool-like");
        return RANE_E_INVALID_ARG;
      }
      if (s->if_stmt.then_branch) {
        rane_error_t e = typecheck_stmt(s->if_stmt.then_branch, scope, diag);
        if (e != RANE_OK) return e;
      }
      if (s->if_stmt.else_branch) {
        rane_error_t e = typecheck_stmt(s->if_stmt.else_branch, scope, diag);
        if (e != RANE_OK) return e;
      }
      return RANE_OK;
    }

    case STMT_WHILE: {
      rane_type_e ct = infer_expr_type(s->while_stmt.cond, scope, diag);
      if (diag && diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
      if (ct != RANE_TYPE_B1 && ct != RANE_TYPE_U64) {
        diag_set(diag, RANE_DIAG_TYPE_MISMATCH, s->while_stmt.cond->span, "Condition must be bool-like");
        return RANE_E_INVALID_ARG;
      }
      if (s->while_stmt.body) return typecheck_stmt(s->while_stmt.body, scope, diag);
      return RANE_OK;
    }

    case STMT_PROC: {
      // New function scope: params are defined as u64 for now.
      rane_scope_t* inner = scope_push(scope);
      if (!inner) { diag_set(diag, RANE_DIAG_INTERNAL_ERROR, s->span, "Out of memory"); return RANE_E_OS_API_FAIL; }
      for (uint32_t i = 0; i < s->proc.param_count; i++) {
        scope_define(inner, s->proc.params[i], RANE_TYPE_U64);
      }
      rane_error_t e = typecheck_stmt(s->proc.body, inner, diag);
      rane_scope_t* parent = inner->parent;
      inner->parent = NULL;
      scope_free_chain(inner);
      (void)parent;
      return e;
    }

    case STMT_RETURN: {
      if (s->ret.expr) {
        (void)infer_expr_type(s->ret.expr, scope, diag);
      }
      return (diag && diag->code != RANE_DIAG_OK) ? RANE_E_INVALID_ARG : RANE_OK;
    }

    default:
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

  rane_scope_t* root = scope_push(NULL);
  if (!root) {
    diag_set(out_diag, RANE_DIAG_INTERNAL_ERROR, ast_root->span, "Out of memory");
    return RANE_E_OS_API_FAIL;
  }

  rane_error_t e = typecheck_stmt(ast_root, root, out_diag);
  scope_free_chain(root);

  if (out_diag && out_diag->code != RANE_DIAG_OK) return RANE_E_INVALID_ARG;
  return e;
}

rane_error_t rane_typecheck_ast(rane_stmt_t* ast_root) {
  return rane_typecheck_ast_ex(ast_root, NULL);
}