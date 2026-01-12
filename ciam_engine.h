// ciam_engine.h
// C++20 header-style interfaces for the RANE CIAM engine
// As of 01_12_2026
//
// Scope:
//   - CIAM pass runner + rule application API
//   - Guard + capability enforcement hooks
//   - Artifact emission contracts
//   - On-disk formats: syntax.opt.ciam.ir (BNF + stable pretty printer rules)
//                      syntax.exec.meta  (binary + JSON mirror)
//
// Design goals:
//   - deterministic outputs (byte-for-byte stable across runs given same inputs)
//   - explicit error reporting (no exceptions required)
//   - minimal dependencies (std only)
//   - forward-compatible versioning + “unknown field” preservation for JSON

#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>
#include <vector>
#include <array>
#include <span>
#include <optional>
#include <utility>

namespace rane::ciam {

//==============================================================================
// Common diagnostics + spans (compatible with your rane_diag_t style)
//==============================================================================

enum class diag_code : uint32_t {
  ok = 0,

  // CIAM / pipeline
  ciam_rule_not_found,
  ciam_rule_precondition_failed,
  ciam_rewrite_failed,
  ciam_internal_error,

  // Policy / capability
  security_violation,
  missing_capability,

  // Format / IO
  io_error,
  format_error,
};

struct span {
  uint32_t line = 0;
  uint32_t col  = 0;
  uint32_t len  = 0;
};

struct diag {
  diag_code code = diag_code::ok;
  span      where{};
  std::string message; // keep it owned for aggregation

  static diag ok() { return {}; }
  static diag make(diag_code c, span s, std::string msg) {
    diag d; d.code = c; d.where = s; d.message = std::move(msg); return d;
  }
};

struct diag_list {
  std::vector<diag> items;

  void push(diag d) { items.emplace_back(std::move(d)); }
  bool ok() const {
    for (auto const& d : items) if (d.code != diag_code::ok) return false;
    return true;
  }
};

//==============================================================================
// Capability model
//==============================================================================

enum class capability : uint16_t {
  heap_alloc = 1,
  file_io    = 2,
  network_io = 3,
  dynamic_eval = 4,
  syscalls   = 5,
  threads    = 6,
  channels   = 7,
  crypto     = 8,
};

struct cap_set {
  // up to 16 caps (enough for your current list)
  uint16_t bits = 0;

  static constexpr uint16_t bit(capability c) {
    return uint16_t(1u) << (uint16_t(c) - 1u);
  }
  void add(capability c) { bits |= bit(c); }
  bool has(capability c) const { return (bits & bit(c)) != 0; }
};

//==============================================================================
// IDs used for stable metadata anchoring (must be deterministic)
//==============================================================================

using node_id  = uint32_t;   // stable id for AST/IG nodes
using block_id = uint32_t;   // stable id for blocks
using sym_id   = uint32_t;   // stable symbol id
using guard_id = uint32_t;   // stable guard id (assigned deterministically)
using tp_id    = uint32_t;   // tracepoint id

//==============================================================================
// Minimal node handle (works for AST or IntentGraph)
//==============================================================================

enum class node_kind : uint16_t {
  unknown = 0,

  // Structural
  block,
  stmt_list,

  // Statements
  let_decl,
  assign,
  if_stmt,
  while_stmt,
  for_stmt,
  return_stmt,
  try_stmt,
  throw_stmt,
  trap_stmt,
  halt_stmt,

  // Calls / ops
  call,
  field_access,
  index_access,

  // Sugar constructs
  with_stmt,
  defer_stmt,
  lock_stmt,
  spawn_expr,
  join_expr,
  match_stmt,
  node_prose,   // “node start: ...”
  goto_stmt,
  label_stmt,

  // Types / decls
  proc_decl,
  struct_decl,
  enum_decl,
  variant_decl,
  union_decl,
};

struct node_ref {
  node_id   id = 0;
  node_kind kind = node_kind::unknown;
  span      where{};
};

//==============================================================================
// Guard + trace metadata emitted by CIAM and consumed by executor/codegen
//==============================================================================

enum class guard_kind : uint16_t {
  defer_cleanup = 1,
  resource_acquire = 2,
  mutex_lock = 3,
  assert_guard = 4,
  determinism_boundary = 5,
};

enum class guard_enforcement : uint8_t {
  none = 0,
  must_run = 1,
  must_close = 2,
  must_unlock = 3,
  trap_on_fail = 4,
  must_succeed = 5,
};

struct guard_anchor {
  // Anchors must survive pretty-print and codegen:
  // - in IR: anchor can be (fn_sym, bb_id, inst_index)
  // - in native: becomes RVA ranges (filled later)
  sym_id   fn = 0;
  uint32_t bb = 0;
  uint32_t inst = 0;
};

struct guard_record {
  guard_id id = 0;
  guard_kind kind = guard_kind::assert_guard;
  guard_enforcement enforcement = guard_enforcement::none;
  span where{};
  guard_anchor anchor{};
};

// Tracepoints are optional; they can be stripped by policy
enum class trace_kind : uint16_t {
  spawn = 1,
  join = 2,
  await_pt = 3,
  file_open = 4,
  file_close = 5,
  eval = 6,
  node_enter = 7,
  node_exit = 8,
};

struct trace_record {
  tp_id id = 0;
  trace_kind kind = trace_kind::spawn;
  span where{};
  guard_anchor anchor{};
};

//==============================================================================
// CIAM Rule IDs + rule descriptors
//==============================================================================

using rule_id = uint32_t;

enum class pass_id : uint16_t {
  intentgraph_build = 0,
  desugar_core = 1,
  lower_smart_expr = 2,
  enforce_caps_contracts = 3,
  optimize = 4,
  bind_codegen_metadata = 5,
};

struct rule_desc {
  rule_id id = 0;
  std::string_view name;     // stable name: "D2_LOCK_TO_TRY_FINALLY"
  pass_id pass = pass_id::desugar_core;
  node_kind matches = node_kind::unknown; // primary match kind
};

//==============================================================================
// Artifacts: canonical surface, optimized IR, executor metadata
//==============================================================================

enum class artifact_kind : uint16_t {
  canonical_surface = 1,     // syntax.ciam.rane (text)
  optimized_ir_text = 2,     // syntax.opt.ciam.ir (text)
  exec_meta_bin     = 3,     // syntax.exec.meta (binary)
  exec_meta_json    = 4,     // syntax.exec.meta.json (json mirror)
};

struct artifact_view {
  artifact_kind kind{};
  std::string_view bytes;    // view into caller-owned buffer
};

struct artifact_buffer {
  artifact_kind kind{};
  std::vector<uint8_t> bytes;
};

struct out_artifacts {
  // caller provides empty buffers; CIAM fills deterministically
  std::vector<artifact_buffer> outputs;
};

//==============================================================================
// Context: policy profile, symbol/type info, deterministic options
//==============================================================================

enum class determinism_mode : uint8_t {
  ritual = 0,          // deterministic scheduling + deterministic IO boundaries
  relaxed = 1,         // allow declared relaxations
};

enum class opt_level : uint8_t {
  none = 0,
  speed = 1,
  size = 2,
};

struct policy_profile {
  determinism_mode det = determinism_mode::ritual;
  opt_level opt = opt_level::speed;

  // performance floor for resolver executor, e.g. 0.98
  uint16_t perf_floor_permille = 980;

  // if false, reject rules that introduce tracepoints (even optional)
  bool allow_tracepoints = true;

  // if false, CIAM must not emit asserts/guards unless “must_*”
  bool allow_optional_invariants = true;
};

struct symbol {
  sym_id id = 0;
  std::string_view name;
};

struct symbol_table {
  std::vector<symbol> syms;
  // implementation-defined lookup helpers
};

struct ctx {
  policy_profile policy{};
  cap_set        available_caps{};   // caps granted to the compilation unit / proc
  symbol_table*  symtab = nullptr;   // owned by frontend/toolchain
  diag_list*     diags  = nullptr;   // sink for diagnostics

  // deterministic guard/trace id allocation:
  // seed must be stable for a given input (e.g., hash of canonical source)
  uint64_t stable_seed = 0;

  // outputs collected during CIAM
  std::vector<guard_record> guards;
  std::vector<trace_record> traces;

  // helper: emit diag
  void error(diag_code c, span s, std::string msg) {
    if (diags) diags->push(diag::make(c, s, std::move(msg)));
  }
};

//==============================================================================
// IR building interface (CIAM produces CIAM-IR; optimizer mutates it)
//==============================================================================

// A deliberately small, stable IR model: functions -> basic blocks -> instructions.
// Your real implementation can be richer; this is the contract CIAM depends on.

enum class ir_type : uint16_t {
  void_t, bool_t,
  i8, i16, i32, i64,
  u8, u16, u32, u64,
  f32, f64,
  string_t,
  thandle,
  opaque,     // structs/unions/variants by symbol id elsewhere
};

enum class ir_op : uint16_t {
  // constants
  const_i64,
  const_u64,
  const_f64,
  const_bool,
  const_str,

  // arithmetic / logic
  add_i64, sub_i64, mul_i64, div_i64, mod_i64,
  and_i64, or_i64, xor_i64,
  shl_i64, shr_i64, sar_i64,
  cmp_eq_i64, cmp_ne_i64, cmp_lt_i64, cmp_le_i64, cmp_gt_i64, cmp_ge_i64,

  // control
  br,
  brnz,
  jmp,
  ret,
  trap,
  halt,
  switch_u8,
  switch_i64,

  // calls / intrinsics
  call,
  max_i64,
  min_i64,

  // memory-ish
  field_load,
  field_store,

  // variant ops
  variant_tag,
  variant_payload_i64,
  variant_make_some_i64,
  variant_make_none,

  // guard markers (anchors)
  guard_begin,
  guard_end,

  // await
  await_i64,
};

struct ir_value {
  uint32_t id = 0;     // SSA-ish value id
  ir_type  type = ir_type::opaque;
};

struct ir_inst {
  ir_op op = ir_op::call;
  span  where{};
  std::array<ir_value, 4> args{}; // compact; extend in impl
  uint8_t arg_count = 0;
  ir_value result{};             // result.id==0 means no result

  // for call:
  sym_id callee = 0;

  // for guard markers:
  guard_id guard = 0;

  // for switches:
  uint32_t switch_table_index = 0;
};

struct ir_block {
  uint32_t id = 0;               // bb id
  std::vector<ir_inst> insts;
};

struct ir_fn {
  sym_id id = 0;
  cap_set requires{};
  std::vector<ir_block> blocks;
};

struct ir_module {
  uint32_t ir_version = 1;
  std::string_view target = "x86_64";
  opt_level opt = opt_level::speed;

  std::vector<ir_fn> fns;
};

//==============================================================================
// Required API: pass runner + rule application + guard/cap enforcement
//==============================================================================

// Runs a single CIAM pass over AST or IntentGraph, producing rewrites and/or IR.
// - ast_or_intentgraph is an opaque pointer to your frontend’s node storage.
// - The pass may emit artifacts (canonical surface, optimized IR text, meta).
diag ciam_pass_run(
  ctx& C,
  pass_id pass,
  const void* ast_or_intentgraph,
  out_artifacts& out);

// Applies a single rule to a node (AST/IG node), may rewrite tree, emit IR, etc.
diag ciam_apply_rule(
  ctx& C,
  rule_id rid,
  node_ref n,
  const void* ast_or_intentgraph);

// Emits a guard record + returns its guard_id.
// Anchor can be filled immediately (if you have IR anchor) or later.
guard_id ciam_emit_guard(
  ctx& C,
  guard_kind kind,
  guard_enforcement enforcement,
  span where,
  std::optional<guard_anchor> anchor = std::nullopt);

// Enforces that a capability is available; emits a deterministic diagnostic otherwise.
diag ciam_require_cap(
  ctx& C,
  capability cap,
  span where,
  std::string_view reason);

// Optional helper: tracepoint emission (policy-gated)
std::optional<tp_id> ciam_emit_tracepoint(
  ctx& C,
  trace_kind kind,
  span where,
  std::optional<guard_anchor> anchor = std::nullopt);

//==============================================================================
// Serialization APIs (text IR + binary/meta + JSON mirror)
//==============================================================================

// ---- syntax.opt.ciam.ir (text) ----
struct ir_print_opts {
  bool emit_line_directives = false;      // if you want stable “;@L:C” comments
  bool emit_spans = true;                  // include spans as comments
  bool sort_locals = true;                 // stable ordering
  bool stable_spacing = true;              // enforce canonical whitespace
};

diag write_opt_ciam_ir_text(
  const ir_module& M,
  std::vector<uint8_t>& out_utf8,
  const ir_print_opts& opts);

// ---- syntax.exec.meta (binary) ----
struct exec_meta_opts {
  uint32_t meta_version = 1;
  std::string_view target = "x86_64";
  std::string_view abi = "win64";
  uint16_t perf_floor_permille = 980;
};

struct exec_meta_bin {
  // in-memory representation used for binary + json writing
  uint32_t meta_version = 1;
  uint32_t build_id = 0xDEADBEEF;

  std::string target;
  std::string abi;
  uint16_t perf_floor_permille = 980;

  cap_set required_caps{};
  policy_profile policy{};

  std::vector<guard_record> guards;
  std::vector<trace_record> traces;

  // address_map + imports filled by later stages; still part of format
  struct sym_range {
    sym_id sym = 0;
    uint32_t section = 0;  // 0=.text 1=.rdata 2=.data etc
    uint32_t rva_start = 0;
    uint32_t rva_end = 0;
  };
  std::vector<sym_range> address_map;

  std::vector<sym_id> imports; // symbol ids for imported runtime calls
};

diag write_exec_meta_binary(
  const exec_meta_bin& M,
  std::vector<uint8_t>& out_bytes);

diag read_exec_meta_binary(
  exec_meta_bin& out_M,
  std::span<const uint8_t> bytes);

// ---- syntax.exec.meta.json (mirror) ----
// The JSON mirror must be stable, diffable, and include unknown fields pass-through.
// Implementations typically store unknown fields as raw JSON text blobs per object.
diag write_exec_meta_json(
  const exec_meta_bin& M,
  std::vector<uint8_t>& out_utf8,
  bool pretty = true);

diag read_exec_meta_json(
  exec_meta_bin& out_M,
  std::span<const uint8_t> utf8);

//==============================================================================
// On-disk format specs
//==============================================================================

/*
===============================================================================
FORMAT A: syntax.opt.ciam.ir  (TEXT)
===============================================================================

A.1 BNF (EBNF-ish)

<file>        ::= <header> <fn_list>
<header>      ::= "ir_version" <u32> "\n"
                  "target" <ident> "\n"
                  "build_id" <hex_u32> "\n"
                  "opt_level" <ident> "\n"
                  "determinism" <ident> "\n"
                  "\n"

<fn_list>     ::= { <fn> "\n" }

<fn>          ::= "fn" <ident> "(" [ <arg_list> ] ")" "->" <type> "\n"
                  [ "  requires" <cap_list> "\n" ]
                  [ "  local" <local_list> "\n" ]*
                  <bb_list>
                  "endfn" "\n"

<arg_list>    ::= <arg> { "," <arg> }
<arg>         ::= <ident> ":" <type>

<cap_list>    ::= <cap> { "," <cap> }
<cap>         ::= "heap_alloc" | "file_io" | "network_io" | "dynamic_eval" |
                  "syscalls" | "threads" | "channels" | "crypto"

<local_list>  ::= <local> { "," <local> }
<local>       ::= <ident> ":" <type>

<bb_list>     ::= { <bb> }
<bb>          ::= <bb_label> ":" "\n" { "  " <inst> "\n" }

<bb_label>    ::= "bb" <u32> | <ident>   // allow named bbs in dumps

<inst>        ::= <assign_inst> | <term_inst> | <guard_inst> | <comment>
<assign_inst> ::= <value> "=" <op> <opargs>
<term_inst>   ::= <op> <opargs>
<guard_inst>  ::= "guard_begin" "kind=" <ident> "id=" <u32>
                  | "guard_end" "id=" <u32>

<value>       ::= "%" <ident> ":" <type> | "%" <ident>
<op>          ::= <ident>
<opargs>      ::= [ <atom> { "," <atom> } ]
<atom>        ::= <value> | <imm> | <ident> | <labelref>

<labelref>    ::= "->" <bb_label> | "case" <imm> "->" <bb_label>

<type>        ::= "void" | "bool" | "i8" | "i16" | "i32" | "i64" |
                  "u8" | "u16" | "u32" | "u64" |
                  "f32" | "f64" | "string" | "thandle" | "opaque(" <ident> ")"

<imm>         ::= <int> | <hex> | <bool> | <string>
<int>         ::= ["-"] DIGIT { DIGIT | "_" }
<hex>         ::= "0x" HEXDIGIT { HEXDIGIT | "_" }
<bool>        ::= "true" | "false"
<string>      ::= '"' { <escaped_char> | any_nonquote } '"'

<comment>     ::= ";" { any }   // comment to end of line

A.2 Stable pretty-printer rules (MUST):
  1) LF line endings only.
  2) Exactly two spaces indentation inside blocks.
  3) Values: print as %name:type when first defined, later as %name.
  4) Locals: sorted by name if opts.sort_locals=true.
  5) Instructions: one per line, no trailing spaces.
  6) Integers: preserve '_' grouping; otherwise print canonical decimal.
  7) Hex: uppercase A-F, preserve '_' if present else group by 4 nybbles.
  8) Strings: escape \n \r \t \\ \" only; everything else UTF-8 as-is.
  9) Spans: if opts.emit_spans, emit as comment suffix:
       "  %x:i64 = add_i64 %a,%b  ;@L12:C5:LEN3"
 10) Guard ids and block ids are stable across runs (seeded).

===============================================================================
FORMAT B: syntax.exec.meta (BINARY) + JSON mirror
===============================================================================

B.1 Binary container: "RANE_META" chunked, little-endian.

Header (fixed 32 bytes):
  u32 magic      = 0x4152454D   // 'MERA' (example) — choose one and freeze
  u16 version    = 1
  u16 header_sz  = 32
  u32 build_id   = 0xDEADBEEF
  u32 flags      = 0
  u32 chunk_cnt  = N
  u32 chunk_off  = byte offset to first chunk
  u32 reserved   = 0

Chunk header (16 bytes each, followed by payload):
  u32 tag        // 'CAPS' 'POLY' 'GARD' 'TRAC' 'IMPT' 'AMAP' 'STRS'
  u32 size       // payload bytes
  u32 crc32      // optional; 0 if unused
  u32 reserved   // 0

String table chunk 'STRS':
  u32 string_count
  u32 offsets[string_count]  // offsets from start of string blob
  u8  blob[]                 // NUL-terminated UTF-8 strings

Caps chunk 'CAPS':
  u16 required_caps_bits
  u16 reserved
  u32 reserved2

Policy chunk 'POLY':
  u8 determinism_mode   // 0 ritual, 1 relaxed
  u8 opt_level          // 0 none, 1 speed, 2 size
  u8 allow_tracepoints  // 0/1
  u8 allow_optional_invariants // 0/1
  u16 perf_floor_permille
  u16 reserved

Guards chunk 'GARD':
  u32 guard_count
  repeat guard_count:
    u32 guard_id
    u16 kind
    u8  enforcement
    u8  reserved
    span where: u32 line, u32 col, u32 len
    anchor: u32 fn_sym, u32 bb, u32 inst

Trace chunk 'TRAC':
  u32 trace_count
  repeat trace_count:
    u32 tp_id
    u16 kind
    u16 reserved
    span where (line,col,len)
    anchor (fn_sym,bb,inst)

Imports chunk 'IMPT':
  u32 import_count
  u32 sym_ids[import_count]

Address map chunk 'AMAP':
  u32 range_count
  repeat range_count:
    u32 sym_id
    u32 section
    u32 rva_start
    u32 rva_end

B.2 JSON mirror: syntax.exec.meta.json
  - MUST be semantically equivalent to binary.
  - MUST be stable for diffing.
  - MUST include "meta_version" and "build_id" and "target"/"abi".
  - MUST sort arrays by stable key:
      guards by guard_id
      traces by tp_id
      imports by sym_id
      address_map by sym_id
  - MUST represent capabilities both as:
      "required_caps": ["heap_alloc", ...]
      and "required_caps_bits": <u16>
  - Unknown fields:
      Readers MUST ignore unknown fields.
      Writers SHOULD preserve unknown fields if they were present on read
      (implementation detail: store raw JSON fragments per object).

Example JSON shape:

{
  "meta_version": 1,
  "build_id": 3735928559,
  "target": "x86_64",
  "abi": "win64",
  "perf_floor_permille": 980,

  "required_caps_bits": 255,
  "required_caps": ["heap_alloc","file_io","network_io","dynamic_eval","syscalls","threads","channels","crypto"],

  "policy": {
    "determinism_mode": "ritual",
    "opt_level": "speed",
    "allow_tracepoints": true,
    "allow_optional_invariants": true
  },

  "guards": [
    {
      "id": 0,
      "kind": "mutex_lock",
      "enforcement": "must_succeed",
      "span": {"line": 1, "col": 1, "len": 1},
      "anchor": {"fn_sym": 12, "bb": 3, "inst": 18}
    }
  ],

  "traces": [
    {
      "id": 0,
      "kind": "spawn",
      "span": {"line": 1, "col": 1, "len": 1},
      "anchor": {"fn_sym": 12, "bb": 3, "inst": 7}
    }
  ],

  "imports": [101, 102, 103],
  "address_map": [
    {"sym": 12, "section": 0, "rva_start": 4096, "rva_end": 8192}
  ]
}

===============================================================================
End formats
===============================================================================
*/

} // namespace rane::ciam
