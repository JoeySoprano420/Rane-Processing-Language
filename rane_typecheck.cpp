#include "rane_typecheck.h"

static rane_error_t typecheck_expr(rane_expr_t* e) {
  if (!e) return RANE_E_INVALID_ARG;
  switch (e->kind) {
    case EXPR_LIT_INT:
      return RANE_OK;
    case EXPR_VAR:
      return RANE_OK;
    case EXPR_CALL:
      // v0: accept builtin calls; deeper checking later.
      return RANE_OK;
    case EXPR_BINARY:
      return typecheck_expr(e->binary.left) == RANE_OK ? typecheck_expr(e->binary.right) : RANE_E_INVALID_ARG;
    case EXPR_UNARY:
      return typecheck_expr(e->unary.expr);
    default:
      return RANE_E_INVALID_ARG;
  }
}

// Stub implementation for type checking
rane_error_t rane_typecheck_ast(rane_stmt_t* ast_root) {
  if (!ast_root) return RANE_E_INVALID_ARG;

  if (ast_root->kind == STMT_BLOCK) {
    for (uint32_t i = 0; i < ast_root->block.stmt_count; i++) {
      rane_error_t e = rane_typecheck_ast(ast_root->block.stmts[i]);
      if (e != RANE_OK) return e;
    }
    return RANE_OK;
  }

  if (ast_root->kind == STMT_LET) {
    return typecheck_expr(ast_root->let.expr);
  }
  if (ast_root->kind == STMT_ASSIGN) {
    return typecheck_expr(ast_root->assign.expr);
  }
  if (ast_root->kind == STMT_JUMP || ast_root->kind == STMT_MARKER) {
    return RANE_OK;
  }
  if (ast_root->kind == STMT_IF) {
    if (typecheck_expr(ast_root->if_stmt.cond) != RANE_OK) return RANE_E_INVALID_ARG;
    if (ast_root->if_stmt.then_branch && rane_typecheck_ast(ast_root->if_stmt.then_branch) != RANE_OK) return RANE_E_INVALID_ARG;
    if (ast_root->if_stmt.else_branch && rane_typecheck_ast(ast_root->if_stmt.else_branch) != RANE_OK) return RANE_E_INVALID_ARG;
    return RANE_OK;
  }
  if (ast_root->kind == STMT_WHILE) {
    if (typecheck_expr(ast_root->while_stmt.cond) != RANE_OK) return RANE_E_INVALID_ARG;
    if (ast_root->while_stmt.body && rane_typecheck_ast(ast_root->while_stmt.body) != RANE_OK) return RANE_E_INVALID_ARG;
    return RANE_OK;
  }

  return RANE_E_INVALID_ARG;
}