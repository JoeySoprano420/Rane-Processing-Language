#pragma once

#include "rane_loader.h"
#include "rane_ast.h"
#include "rane_diag.h"

// Parser for RANE source code
// Parses a string into AST

rane_error_t rane_parse_source(const char* source, rane_stmt_t** out_ast_root);
rane_error_t rane_parse_source_ex(const char* source, rane_stmt_t** out_ast_root, rane_diag_t* out_diag);

// Length-aware variants (recommended for file input)
rane_error_t rane_parse_source_len(const char* source, size_t len, rane_stmt_t** out_ast_root);
rane_error_t rane_parse_source_len_ex(const char* source, size_t len, rane_stmt_t** out_ast_root, rane_diag_t* out_diag);