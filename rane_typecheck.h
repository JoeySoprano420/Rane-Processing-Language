#pragma once

#include "rane_loader.h"
#include "rane_ast.h"

// Type checker for RANE AST

rane_error_t rane_typecheck_ast(rane_stmt_t* ast_root);