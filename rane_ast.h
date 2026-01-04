#pragma once

#include <stdint.h>

#include "rane_diag.h"

// ---------------------------
// RANE Types
// ---------------------------

typedef enum rane_type_e {
  RANE_TYPE_U8,
  RANE_TYPE_U16,
  RANE_TYPE_U32,
  RANE_TYPE_U64,
  RANE_TYPE_I8,
  RANE_TYPE_I16,
  RANE_TYPE_I32,
  RANE_TYPE_I64,
  RANE_TYPE_P64,
  RANE_TYPE_B1,
  RANE_TYPE_STRING
} rane_type_e;

// ---------------------------
// Expression Kinds
// ---------------------------

typedef enum rane_expr_kind_e {
  EXPR_VAR,
  EXPR_LIT_INT,
  EXPR_LIT_BOOL,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_CALL,
  EXPR_CHOOSE,
  EXPR_ADDR,
  EXPR_LOAD,
  EXPR_STORE
} rane_expr_kind_e;

// ---------------------------
// Binary Ops
// ---------------------------

typedef enum rane_bin_op_e {
  BIN_ADD,
  BIN_SUB,
  BIN_MUL,
  BIN_DIV,
  BIN_MOD,
  BIN_AND,
  BIN_OR,
  BIN_XOR,
  BIN_SHL,
  BIN_SHR,
  BIN_SAR,
  BIN_LT,
  BIN_LE,
  BIN_GT,
  BIN_GE,
  BIN_EQ,
  BIN_NE,
  BIN_LOGAND,
  BIN_LOGOR
} rane_bin_op_e;

// ---------------------------
// Unary Ops
// ---------------------------

typedef enum rane_unary_op_e {
  UN_NEG,
  UN_NOT,
  UN_BITNOT
} rane_unary_op_e;

// ---------------------------
// Statement Kinds
// ---------------------------

typedef enum rane_stmt_kind_e {
  STMT_BLOCK,
  STMT_LET,
  STMT_ASSIGN,
  STMT_JUMP,
  STMT_CJUMP,
  STMT_MARKER,
  STMT_REPEAT,
  STMT_DECIDE,
  STMT_PROC_CALL,
  STMT_CHAN_PUSH,
  STMT_CHAN_POP,
  STMT_GUARD,
  STMT_ZONE,
  STMT_IMPORT_DECL,
  STMT_EXPORT_DECL,
  STMT_MMIO_REGION_DECL,
  STMT_MEM_COPY,
  STMT_IF,
  STMT_WHILE,
  STMT_PROC,
  STMT_RETURN
} rane_stmt_kind_e;

// ---------------------------
// Forward declarations
// ---------------------------

typedef struct rane_expr_s rane_expr_t;
typedef struct rane_stmt_s rane_stmt_t;

// ---------------------------
// Expressions
// ---------------------------

struct rane_expr_s {
  rane_span_t span;
  rane_expr_kind_e kind;
  union {
    struct {
      char name[64];
    } var;
    struct {
      uint64_t value;
      rane_type_e type;
    } lit_int;
    struct {
      int value;
    } lit_bool;
    struct {
      rane_unary_op_e op;
      rane_expr_t* expr;
    } unary;
    struct {
      rane_bin_op_e op;
      rane_expr_t* left;
      rane_expr_t* right;
    } binary;
    struct {
      char name[64];
      rane_expr_t** args;
      uint32_t arg_count;
    } call;
    struct {
      enum {
        CHOOSE_MAX,
        CHOOSE_MIN
      } kind;
      rane_expr_t* a;
      rane_expr_t* b;
    } choose;
    struct {
      rane_expr_t* base;
      rane_expr_t* index;
      uint64_t scale;
      uint64_t disp;
    } addr;
    struct {
      rane_type_e type;
      rane_expr_t* addr_expr;
      int volatility;
    } load;
    struct {
      rane_type_e type;
      rane_expr_t* addr_expr;
      rane_expr_t* value_expr;
      int volatility;
    } store;
  };
};

// ---------------------------
// Statements
// ---------------------------

struct rane_stmt_s {
  rane_span_t span;
  rane_stmt_kind_e kind;
  union {
    struct {
      rane_stmt_t** stmts;
      uint32_t stmt_count;
    } block;
    struct {
      char name[64];
      rane_type_e type;
      rane_expr_t* expr;
    } let;
    struct {
      char target[64];
      rane_expr_t* expr;
    } assign;
    struct {
      char marker[64];
    } jump;
    struct {
      rane_expr_t* cond;
      char true_marker[64];
      char false_marker[64];
    } cjump;
    struct {
      char name[64];
    } marker;
    struct {
      rane_expr_t* count;
      rane_stmt_t* block;
    } repeat;
    struct {
      rane_expr_t* expr;
      struct {
        uint64_t key;
        char marker[64];
      } cases[32];
      uint32_t case_count;
      char default_marker[64];
    } decide;
    struct {
      char proc_name[64];
      uint32_t slot;
      rane_expr_t** args;
      uint32_t arg_count;
    } proc_call;
    struct {
      char chan[64];
      rane_expr_t* value;
    } chan_push;
    struct {
      char chan[64];
      char target[64];
    } chan_pop;
    struct {
      rane_expr_t* cond;
      rane_stmt_t* block;
    } guard;
    struct {
      enum {
        ZONE_HOT,
        ZONE_COLD,
        ZONE_DETERMINISTIC
      } kind;
      rane_stmt_t* block;
    } zone;
    struct {
      char sym[64];
      char dll[64];
      char hint[64];
    } import_decl;
    struct {
      char sym[64];
      uint32_t ordinal;
      uint32_t flags;
    } export_decl;
    struct {
      char name[64];
      uint64_t base;
      uint64_t size;
    } mmio_region_decl;
    struct {
      rane_expr_t* dst;
      rane_expr_t* src;
      rane_expr_t* size;
    } mem_copy;
    struct {
      rane_expr_t* cond;
      rane_stmt_t* then_branch;
      rane_stmt_t* else_branch;
    } if_stmt;
    struct {
      rane_expr_t* cond;
      rane_stmt_t* body;
    } while_stmt;
    struct {
      char name[64];
      char** params;
      uint32_t param_count;
      rane_stmt_t* body; // block
    } proc;
    struct {
      rane_expr_t* expr; // may be NULL for bare return
    } ret;
  };
};