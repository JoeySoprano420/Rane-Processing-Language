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
  // Bootstrap legacy: heap pointer to NUL-terminated string
  RANE_TYPE_STRING,
  // v1: distinct types for `say` (both are represented as pointer-sized values in bootstrap)
  RANE_TYPE_TEXT,
  RANE_TYPE_BYTES
} rane_type_e;

// ---------------------------
// Named types (v1)
// ---------------------------

typedef struct rane_type_name_s {
  char name[64];
} rane_type_name_t;

// ---------------------------
// Expression Kinds
// ---------------------------

typedef enum rane_expr_kind_e {
  EXPR_VAR,
  EXPR_LIT_INT,
  EXPR_LIT_BOOL,
  EXPR_LIT_TEXT,
  EXPR_LIT_BYTES,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_CALL,
  EXPR_MEMBER,
  EXPR_INDEX,
  EXPR_CHOOSE,
  EXPR_ADDR,
  EXPR_LOAD,
  EXPR_STORE,
  EXPR_LIT_IDENT,
  EXPR_MMIO_ADDR,
  EXPR_LIT_NULL,
  EXPR_TERNARY,

  // v1: struct literal construction
  EXPR_STRUCT_LITERAL
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
  STMT_RETURN,
  STMT_BREAK,
  STMT_CONTINUE,

  // v1 data model
  STMT_STRUCT_DECL,
  STMT_SET,
  STMT_ADD,

  // v1 prose/node surface
  STMT_MODULE,
  STMT_NODE,
  STMT_START_AT,
  STMT_GO_NODE,
  STMT_SAY
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

    // New: literals represented directly as source slices.
    // For bootstrap, these slices still point into the input buffer.
    struct {
      const char* start;
      uint32_t length;
    } lit_text;

    struct {
      const char* start;
      uint32_t length;
    } lit_bytes;

    struct {
      rane_unary_op_e op;
      rane_expr_t* expr;
    } unary;
    struct {
      rane_bin_op_e op;
      rane_expr_t* left;
      rane_expr_t* right;
    } binary;

    // Call: either a simple name call or a callee expression call.
    // If `callee` is non-null, it is used; otherwise `name` is used.
    struct {
      char name[64];
      rane_expr_t* callee;
      rane_expr_t** args;
      uint32_t arg_count;
    } call;

    // New: member access: base.member
    struct {
      rane_expr_t* base;
      char member[64];
    } member;

    // New: index access: base[index]
    struct {
      rane_expr_t* base;
      rane_expr_t* index;
    } index;

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
    struct {
      char value[64];
    } lit_ident;
    struct {
      char region[64];
      rane_expr_t* offset;
    } mmio_addr;

    struct {
      uint64_t reserved;
    } lit_null;

    struct {
      rane_expr_t* cond;
      rane_expr_t* then_expr;
      rane_expr_t* else_expr;
    } ternary;

    // v1: struct literal
    // Name{ field: expr ... } or Name(expr, ...)
    struct {
      char type_name[64];
      // If named_fields is non-zero, use fields[]; otherwise use pos_args[].
      uint32_t named_fields;

      struct {
        char name[64];
        rane_expr_t* value;
      } fields[32];
      uint32_t field_count;

      rane_expr_t* pos_args[32];
      uint32_t pos_count;
    } struct_lit;
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

    // break/continue have no payload in bootstrap.
    struct { uint8_t _; } brk;
    struct { uint8_t _; } cont;

    // v1 prose/node surface
    struct {
      char name[64];
    } module_decl;

    struct {
      char name[64];
      rane_stmt_t* body; // STMT_BLOCK
    } node_decl;

    struct {
      char node_name[64];
    } start_at;

    struct {
      char node_name[64];
    } go_node;

    struct {
      rane_expr_t* expr;
    } say;

    // v1: struct definition
    struct {
      char name[64];
      struct {
        char name[64];
        // For now, keep field types as names (e.g. u32, Header).
        // Typechecking will resolve primitives vs named types.
        rane_type_name_t type_name;
      } fields[32];
      uint32_t field_count;
    } struct_decl;

    // v1: set statement
    // Forms:
    //  - set x: Ty to expr
    //  - set target_expr to expr   (where target_expr can be member access)
    struct {
      char name[64];
      rane_type_name_t type_name; // optional (name[0]==0 => not a declaration)
      rane_expr_t* target_expr;   // used when assigning into member
      rane_expr_t* value;
    } set_stmt;

    // v1: add target by expr (numeric update)
    struct {
      rane_expr_t* target_expr;
      rane_expr_t* value;
    } add_stmt;
  };
};