#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <stdint.h>
#include <stdlib.h>

#include "rane_common.h"
#include "rane_ast.h"

// ---------------------------
// RANE Typed IR (TIR) Opcodes
// ---------------------------

typedef enum rane_tir_opcode_e {
  // Module / Symbol / Metadata
  TIR_MOD_BEGIN,
  TIR_MOD_END,
  TIR_SECTION,
  TIR_ALIGN,
  TIR_LABEL,
  TIR_GLOBAL,
  TIR_EXTERN,
  TIR_IMPORT,
  TIR_EXPORT,
  TIR_DATA_BEGIN,
  TIR_DATA_BYTES,
  TIR_DATA_U32,
  TIR_DATA_U64,
  TIR_DATA_ZSTR,
  TIR_DATA_END,
  TIR_CONST,
  TIR_DBG_LINE,

  // New: materialize address of a label into a register (RIP-relative in codegen)
  // operands:
  //  - r0: dst
  //  - lbl: target label
  TIR_ADDR_OF,

  // Control flow
  TIR_JMP,
  TIR_JCC,
  TIR_JCC_EXT,
  TIR_SWITCH,

  // Call ABI helpers (Win64 bootstrap)
  // Prepare a call frame for upcoming CALL_*.
  // operands:
  //  - imm0: stack_arg_bytes (bytes of stack args, not including shadow space)
  //  - imm1: flags (reserved)
  TIR_CALL_PREP,

  TIR_CALL_LOCAL,
  TIR_CALL_IMPORT,
  TIR_RET,
  TIR_RET_VAL,
  TIR_TRAP,
  TIR_HALT,
  TIR_NOP,

  // Integer data movement / conversion
  TIR_MOV,
  TIR_XCHG,
  TIR_WIDEN,
  TIR_NARROW,
  TIR_BITCAST,

  // Integer arithmetic (scalar)
  TIR_ADD,
  TIR_SUB,
  TIR_MUL,
  TIR_DIV,
  TIR_NEG,
  TIR_INC,
  TIR_DEC,

  // Bitwise & shifts/rotates
  TIR_AND,
  TIR_OR,
  TIR_XOR,
  TIR_NOT,
  TIR_SHL,
  TIR_SHR,
  TIR_SAR,
  TIR_ROL,
  TIR_ROR,
  TIR_BSWAP,
  TIR_CLZ,
  TIR_CTZ,
  TIR_POPCNT,

  // Comparisons & booleanization
  TIR_CMP,
  TIR_TEST,
  TIR_SETCC,
  TIR_CMOVCC,
  TIR_SELECT,

  // Addressing & memory
  TIR_LEA,
  TIR_LD,
  TIR_ST,
  TIR_LDV,
  TIR_STV,
  TIR_MEMCPY,
  TIR_MEMMOVE,
  TIR_MEMSET,
  TIR_MEMZERO,
  TIR_BOUNDS_CHECK,

  // Concurrency & deterministic scheduler
  TIR_SCHED_INIT,
  TIR_SCHED_TICK,
  TIR_SCHED_DISPATCH,
  TIR_SCHED_BARRIER,
  TIR_SCHED_YIELD,
  TIR_PROC_ENTER,
  TIR_PROC_EXIT,
  TIR_CHAN_DEF,
  TIR_CHAN_PUSH,
  TIR_CHAN_POP,
  TIR_CHAN_PEEK,
  TIR_CHAN_STATUS,
  TIR_ATOM_FENCE,
  TIR_LOCKSTEP_ASSERT,

  // Security / policy proof ops
  TIR_PERMIT,
  TIR_REQUIRE,
  TIR_TAINT,
  TIR_SANITIZE,
  TIR_PATCHPOINT
} rane_tir_opcode_t;

typedef enum rane_tir_cc_e {
  TIR_CC_NE,
  TIR_CC_E,
  TIR_CC_L,
  TIR_CC_LE,
  TIR_CC_G,
  TIR_CC_GE
} rane_tir_cc_t;

// ---------------------------
// Operand kinds
// ---------------------------

typedef enum rane_tir_operand_kind_e {
  TIR_OPERAND_R,   // virtual register (uint32_t index)
  TIR_OPERAND_S,   // stack slot (uint32_t index)
  TIR_OPERAND_IMM, // immediate (uint64_t value)
  TIR_OPERAND_LBL, // label (char[64] name)
  TIR_OPERAND_M    // memory (base_r, index_r, scale, disp)
} rane_tir_operand_kind_t;

typedef struct rane_tir_operand_s {
  rane_tir_operand_kind_t kind;
  union {
    uint32_t r;       // register index
    uint32_t s;       // slot index
    uint64_t imm;     // immediate value
    char lbl[64];     // label name
    struct {
      uint32_t base_r;  // base register (0 if none)
      uint32_t index_r; // index register (0 if none)
      uint32_t scale;   // 1,2,4,8
      int32_t disp;     // displacement
    } m;
  };
} rane_tir_operand_t;

// ---------------------------
// TIR Instruction
// ---------------------------

typedef struct rane_tir_inst_s {
  rane_tir_opcode_t opcode;
  rane_type_e type;              // for typed ops (e.g., LD.U32)
  rane_tir_operand_t operands[4]; // max 4 operands
  uint32_t operand_count;
} rane_tir_inst_t;

// ---------------------------
// TIR Function / Module
// ---------------------------

typedef struct rane_tir_function_s {
  char name[64];
  rane_tir_inst_t* insts;
  uint32_t inst_count;
  uint32_t max_insts; // for dynamic array

  // Stack frame info (bootstrap)
  uint32_t stack_slot_count; // number of 8-byte stack slots used by this function
} rane_tir_function_t;

typedef struct rane_tir_module_s {
  char name[64];
  rane_tir_function_t* functions;
  uint32_t function_count;
  uint32_t max_functions;
} rane_tir_module_t;

// ---------------------------
// Lowering functions (AST -> TIR)
// ---------------------------

rane_error_t rane_lower_ast_to_tir(const rane_stmt_t* ast_root, rane_tir_module_t* tir_module);

// ---------------------------
// Code generation (TIR -> x64)
// ---------------------------

typedef struct rane_codegen_ctx_s {
  uint8_t* code_buffer;
  uint64_t buffer_size;
  uint64_t code_size;

  // Optional: capture import callsites for deterministic patching.
  void* call_fixups;
  uint32_t call_fixup_count;
  uint32_t call_fixup_capacity;
} rane_codegen_ctx_t;

rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx);