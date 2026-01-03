#pragma once

#include "rane_loader.h"
#include "rane_ast.h"

// Parser for RANE source code
// Parses a string into AST

rane_error_t rane_parse_source(const char* source, rane_stmt_t** out_ast_root);