#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rane_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rane_diag_code_e {
  RANE_DIAG_OK = 0,

  // Parse
  RANE_DIAG_PARSE_ERROR,

  // Name/type
  RANE_DIAG_UNDEFINED_NAME,
  RANE_DIAG_REDECLARED_NAME,
  RANE_DIAG_TYPE_MISMATCH,

  // Policy / security
  RANE_DIAG_SECURITY_VIOLATION,

  // Misc
  RANE_DIAG_INTERNAL_ERROR
} rane_diag_code_t;

typedef struct rane_span_s {
  uint32_t line;
  uint32_t col;
  uint32_t length;
} rane_span_t;

typedef struct rane_diag_s {
  rane_diag_code_t code;
  rane_span_t span;
  char message[256];
} rane_diag_t;

static inline rane_span_t rane_span_from_token(uint32_t line, uint32_t col, uint32_t len) {
  rane_span_t s;
  s.line = line;
  s.col = col;
  s.length = len;
  return s;
}

#ifdef __cplusplus
} // extern "C"
#endif
