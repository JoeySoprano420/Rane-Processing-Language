#include "rane_typecheck.h"

// Stub implementation for type checking
rane_error_t rane_typecheck_ast(rane_stmt_t* ast_root) {
  if (!ast_root) return RANE_E_INVALID_ARG;

  // Basic type checking
  if (ast_root->kind == STMT_LET) {
    if (ast_root->let.expr->kind == EXPR_LIT_INT) {
      // OK
    } else if (ast_root->let.expr->kind == EXPR_VAR) {
      // Assume OK for now
    } else {
      return RANE_E_INVALID_ARG; // type error
    }
  } else if (ast_root->kind == STMT_ASSIGN) {
    if (ast_root->assign.expr->kind == EXPR_LIT_INT) {
      // OK
    } else {
      return RANE_E_INVALID_ARG;
    }
  } else if (ast_root->kind == STMT_JUMP) {
    // OK
  } else if (ast_root->kind == STMT_MARKER) {
    // OK
  } else if (ast_root->kind == STMT_IF) {
    // Check cond is bool-like, branches OK
  } else if (ast_root->kind == STMT_WHILE) {
    // Similar
  }

  return RANE_OK;
}