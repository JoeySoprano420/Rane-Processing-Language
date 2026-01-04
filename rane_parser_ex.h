#pragma once

#include "rane_loader.h"
#include "rane_ast.h"
#include "rane_diag.h"

#ifdef __cplusplus
extern "C" {
#endif

// Extended parse API with structured diagnostic.
rane_error_t rane_parse_source_ex(const char* source, rane_stmt_t** out_ast_root, rane_diag_t* out_diag);

#ifdef __cplusplus
} // extern "C"
#endif
