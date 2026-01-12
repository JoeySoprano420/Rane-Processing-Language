// rane_lexpath_contract.h
// Exact “Lexical Path” contract for RANE parser → CIAM determinism
// As of 01_12_2026
//
// Purpose:
//   Provide a stable, structural identity for every node so CIAM can assign
//   deterministic IDs without span hashing.
//
// Core idea:
//   Every node has:
//     - stable parent pointer (or parent id)
//     - stable "slot kind" within its parent (what role it plays)
//     - stable ordinal within that slot (0..N-1), where the slot list order is
//       canonicalized to lexical source order, not container insertion order.
//
// Lexical Path = sequence of (slot_kind, ordinal) pairs from ProcRoot → Node.
// CIAM uses this path as the stable_key input.
//
// Guarantee target:
//   - Editing unrelated parts of the file does not change paths for nodes whose
//     textual region and structural position are unchanged.
//   - Rebuilding AST vectors in a different order does not change paths.
//   - Adding a new statement only affects ordinals after insertion point within
//     the same statement list slot.
//
// Implementation constraint:
//   - Lexical order MUST be determined from token positions (start offset),
//     not from the order nodes were pushed into a vector.
//
// This header defines the contract; your AST can store it compactly.

#pragma once
#include <cstdint>
#include <span>

namespace rane {

//------------------------------------------------------------------------------
// Token position model (required for lexical ordering)
//------------------------------------------------------------------------------
struct tok_pos {
  // Byte offset from beginning of file (preferred), or (line,col) folded into u32.
  // byte_offset is ideal because it’s monotonic and stable across newline styles.
  uint32_t byte_offset = 0;
  uint32_t byte_len    = 0;
};

//------------------------------------------------------------------------------
// Lexical path encoding
//------------------------------------------------------------------------------
enum class slot_kind : uint16_t {
  // Top-level structure
  file_items = 1,       // declarations at module/file scope
  ns_items   = 2,       // namespace members
  type_items = 3,       // fields/variants/enum members inside a type

  // Procedure structure
  proc_params   = 10,   // parameter list
  proc_requires = 11,   // capability list entries
  proc_body     = 12,   // statements within proc body block

  // Block and statement lists
  block_stmts = 20,     // statement list within a block

  // Statement internal slots
  let_bindings   = 30,  // pattern binds in let (if any)
  assign_lhs     = 31,
  assign_rhs     = 32,
  if_cond        = 33,
  if_then        = 34,
  if_else        = 35,
  while_cond     = 36,
  while_body     = 37,
  for_init       = 38,
  for_cond       = 39,
  for_step       = 40,
  for_body       = 41,
  return_expr    = 42,
  try_body       = 43,
  catch_list     = 44,
  finally_body   = 45,
  throw_expr     = 46,

  // Call/expression slots
  call_callee = 60,
  call_args   = 61,
  unary_arg   = 62,
  binary_lhs  = 63,
  binary_rhs  = 64,
  ternary_cond= 65,
  ternary_then= 66,
  ternary_else= 67,
  field_base  = 68,
  index_base  = 69,
  index_expr  = 70,

  // Sugar constructs (CIAM-critical)
  with_acquire = 90,    // the resource acquisition expression (e.g., open(path))
  with_binding = 91,    // binder (e.g., "as f")
  with_body    = 92,    // with body statements block

  defer_body   = 93,    // deferred cleanup block/expression (canonical: block)
  defer_scope  = 94,    // the statements governed by the defer (implicit region)

  lock_target  = 95,    // lock(m) target expression
  lock_body    = 96,    // lock body block

  spawn_callee = 97,    // spawn <callable>
  spawn_args   = 98,    // spawn arguments (if surface supports)
  join_target  = 99,    // join <handle expr>

  match_scrutinee = 100,
  match_arms      = 101, // list of arms
  match_arm_pat   = 102,
  match_arm_guard = 103, // optional “if” guard per arm
  match_arm_body  = 104,

  // Node/prose surface (CIAM-critical)
  node_list       = 120, // list of node blocks in module
  node_header     = 121, // node name/label
  node_body       = 122, // node statements
  node_stmt_list  = 123, // node statement list (set/add/say/go)
  node_start_decl = 124, // “start at node X”

  // Control transfer constructs
  goto_cond     = 130,
  goto_targets  = 131,  // the two (or N) branch labels
  label_name    = 132,

  // Attributes / annotations
  attr_list     = 140,
  attr_args     = 141,
};

// One step in the lexical path = (slot_kind, ordinal within that slot)
struct lexpath_step {
  slot_kind slot;
  uint32_t ordinal;
};

// Contract: every node must be able to provide its path as a view.
// Storage can be:
//   - inline small buffer in node (e.g., up to 8 steps), plus overflow arena
//   - or store parent pointer + (slot,ordinal) and let CIAM reconstruct by climbing
struct lexpath_view {
  const lexpath_step* steps = nullptr;
  uint32_t count = 0;
};

//------------------------------------------------------------------------------
// Required node identity API (parser must provide)
//------------------------------------------------------------------------------
using node_id = uint32_t;

struct lexpath_contract {
  // Get lexical span for ordering + diagnostics.
  // start must be the first token that belongs to the node (including keywords).
  tok_pos (*node_pos)(node_id n) noexcept;

  // Parent relationship. Root returns 0.
  node_id (*parent)(node_id n) noexcept;

  // Slot kind under its parent. Must be stable and correct.
  slot_kind (*slot)(node_id n) noexcept;

  // Ordinal within that slot list (0..N-1), computed by lexical ordering.
  uint32_t (*ordinal)(node_id n) noexcept;

  // Convenience: produce the full path. Optional if CIAM can climb.
  // If provided, must return ProcRoot→Node (root-first order).
  lexpath_view (*path)(node_id n) noexcept;
};

//------------------------------------------------------------------------------
// EXACT RULES FOR COMPUTING ORDINALS (the heart of the contract)
//------------------------------------------------------------------------------
//
// For any parent P and a specific slot kind S, define the “slot list” L(P,S):
//   L(P,S) = all children C of P such that slot(C)==S.
//
// The parser must compute ordinal(C) as:
//
//   ordinal(C) = rank of C in sort_by_lexical_start(L(P,S))
//
// where sort_by_lexical_start compares:
//   1) node_pos(C).byte_offset (ascending)
//   2) node_pos(C).byte_len (descending)  [ties: longer first to stabilize nesting]
//   3) node_id (ascending)                [final tie break; deterministic]
//
// CRITICAL:
//   - Ordinals must NOT depend on vector insertion order.
//   - Children lists must be rankable even if stored in different containers.
//   - If two children have identical byte_offset (possible for error nodes),
//     you still resolve deterministically via length then node_id.
//
// For statement lists:
//   - node_pos(stmt) must point to the first token of that statement.
//   - Example: for “defer { ... }”, position is at “defer”, not the inner block.
//
// For expressions:
//   - node_pos(expr) must point to the first token of the expression.
//
//------------------------------------------------------------------------------
// Slot-by-slot requirements (what children belong to which slots)
//------------------------------------------------------------------------------
//
// FILE / MODULE
// - file_items: all top-level declarations in file/module order:
//     import, module, namespace, type/struct/enum/variant/union, proc, const/constexpr, etc.
//   child pos: keyword start
//
// NAMESPACE
// - ns_items: members inside namespace block (same as file_items)
//
// TYPE DECLARATIONS
// - type_items:
//   - struct: fields in source order
//   - enum: enumerators in source order
//   - variant: variants/constructors in source order
//   - union: fields in source order
//
// PROCEDURE DECL
// - proc_params: each param node (name:type) in source order
// - proc_requires: each capability token in source order
// - proc_body: exactly one block node representing the body (ordinal 0)
//
// BLOCK
// - block_stmts: each statement node in source order
//
// IF
// - if_cond: one expression node (ordinal 0)
// - if_then: one block node (ordinal 0)
// - if_else: zero or one block node (ordinal 0 if present)
//
// WHILE
// - while_cond: expression node (0)
// - while_body: block node (0)
//
// FOR (C-like “for init; cond; step { body }”)
// - for_init: stmt/expression node (0)
// - for_cond: expr node (0)
// - for_step: expr node (0)
// - for_body: block node (0)
//
// RETURN / THROW
// - return_expr: expr node (0 if present)
// - throw_expr: expr node (0)
//
// TRY
// - try_body: block node (0)
// - catch_list: catch clause nodes in source order
// - finally_body: optional block node (0)
//
// CALL
// - call_callee: expr node (0)
// - call_args: argument expr nodes in source order
//
// BINARY / UNARY / TERNARY
// - unary_arg: (0)
// - binary_lhs (0), binary_rhs (0)
// - ternary_cond (0), ternary_then (0), ternary_else (0)
//
// FIELD ACCESS
// - field_base: base expr (0)
//
// INDEX ACCESS
// - index_base (0)
// - index_expr (0)
//
// ATTRIBUTES
// - attr_list: attribute nodes in source order (e.g., @derive, @inline)
// - attr_args: per attribute, argument nodes in source order
//
//------------------------------------------------------------------------------
// CIAM-CRITICAL SUGAR NODES (must be exact)
//------------------------------------------------------------------------------
//
// WITH
// Surface: with open(path) as f { BODY }
// Node shape:
//   WithStmt node has children:
///    - with_acquire: the acquire expression node (open(path))
///    - with_binding: binder node (identifier pattern "f") [can be absent if unnamed]
///    - with_body: block node
// Ordinals in each slot are trivial (single child => 0), except if you allow
// multiple resources in one with (then with_acquire is a list and must be ordered).
//
// DEFER
// Surface: defer { CLEANUP }  (or defer CLEANUP_STMT;)
// Node shape:
//   DeferStmt node has children:
///    - defer_body: a block node OR a single stmt/expression wrapped as a block
// Additionally: parser MUST attach DeferStmt as a statement within block_stmts;
// CIAM defines the governed region as “following statements in same block”;
// the parser does NOT need to encode defer_scope—CIAM can infer it from position.
//
// LOCK
// Surface: lock(m) { BODY }
// Node shape:
//   LockStmt children:
///    - lock_target: expr node (m)
///    - lock_body: block node
//
// SPAWN
// Surface: spawn foo(1,2)  OR spawn foo 1 2 (if verb form)
// Node shape:
//   SpawnExpr children:
///    - spawn_callee: expr node (callable)
//   If spawn takes args explicitly:
///    - spawn_args: expr nodes in source order
// NOTE: If surface is “spawn <call-expr>”, you can represent that as spawn_callee
// being a Call node; then spawn_args slot can be empty (CIAM sees call_args).
//
// JOIN
// Surface: join th
// Node shape:
//   JoinExpr children:
///    - join_target: expr node (th)
//
// MATCH
// Surface:
//   match SCRUT { case PAT [if GUARD]: BODY; ... default: BODY; }
// Node shape:
//   MatchStmt children:
///    - match_scrutinee: expr node
///    - match_arms: arm nodes in source order (lexical)
// Each MatchArm node children:
///    - match_arm_pat: pattern node (Some(x), 0, _, default)
///    - match_arm_guard: optional expr node (if present)
///    - match_arm_body: block node (or single stmt wrapped as block)
//
// NODE/PROSE
// Surface:
//   node start: <node statements> end
//   start at node start
// Node shape:
//   NodeModule contains:
///    - node_list: NodeBlock nodes in source order
///    - node_start_decl: StartAtNode node (if present) [pos at “start” keyword]
// Each NodeBlock contains:
///    - node_header: label/name node (pos at identifier after “node”)
///    - node_body: block-ish node
// NodeBody contains:
///    - node_stmt_list: node-prose statements in source order
//
// Node-prose statements (set/add/say/go/halt/trap) must be real nodes with
// their own node_pos at the verb token.
//
//------------------------------------------------------------------------------
// Malformed code handling
//------------------------------------------------------------------------------
// If parsing produces error-recovery nodes:
//   - They STILL must be assigned parent/slot/ordinal deterministically.
//   - node_pos.byte_offset MUST be set to the best-known token location.
//   - If a node has no token (e.g., missing block), synthesize byte_offset as
//     the parent's end + small deterministic delta (0,1,2...) in recovery order.
//
// CIAM is allowed to fall back to span hash ONLY when:
//   - contract pointers are null OR
//   - parent/slot/ordinal produce cycles/invalid data OR
//   - byte_offset is 0 for non-root nodes (meaning truly unknown).
//
//------------------------------------------------------------------------------
// Recommended compact storage in AST (so CIAM can climb cheaply)
//------------------------------------------------------------------------------
//
// For each node store:
//   u32 parent_id
//   u16 slot_kind
//   u32 ordinal
//   tok_pos pos
//
// Then lexpath_view can be reconstructed by walking parents and reversing.
// That avoids storing full vectors of steps on every node.
//
//------------------------------------------------------------------------------
// End contract
//------------------------------------------------------------------------------

} // namespace rane
