// syntax.rane
// Complete, exhaustive syntax coverage file for the RANE bootstrap compiler in this repo.
// Goal: exercise every implemented syntactic form (lexer → parser → AST → TIR) in one place.
//
// IMPORTANT:
// - This file targets the *current implementation* (what `rane_parser.cpp` accepts).
// - Many lexer keywords are reserved but not parseable yet; they are listed in a comment block
//   so they remain visible without breaking compilation.

///////////////////////////////////////////////////////////////////////////
// 0) Reserved / tokenized keywords (NOT necessarily parseable today)
//    Source of truth: `rane_lexer.cpp` identifier_type()
///////////////////////////////////////////////////////////////////////////
//
// Core/reserved examples (not exhaustive list here):
// let if then else while do for break continue return ret
// proc def call
// import export include exclude
// decide case default
// jump goto mark label
// guard zone hot cold deterministic repeat unroll
// not and or xor shl shr sar
// try catch throw
// define ifdef ifndef pragma namespace enum struct class public private protected
// static inline extern virtual const volatile constexpr consteval constinit
// new del cast type typealias alias mut immutable mutable null match pattern lambda
// handle target splice split difference increment decrement dedicate mutex ignore bypass
// isolate separate join declaration compile score sys admin plot peak point reg exception
// align mutate string literal linear nonlinear primitives tuples member open close
//
// NOTE: Only a subset is accepted at statement position by the current parser.

///////////////////////////////////////////////////////////////////////////
// 1) Imports (current parser: `import <sym>;` OR `import <sym> from "<dll>";` depending on branch)
//    - This file sticks to the known-working older form used in tests: `import rane_rt_print;`
///////////////////////////////////////////////////////////////////////////

import rane_rt_print;

///////////////////////////////////////////////////////////////////////////
// 2) MMIO region decl + read32/write32 (statement forms)
///////////////////////////////////////////////////////////////////////////

mmio region REG from 4096 size 256;

///////////////////////////////////////////////////////////////////////////
// 3) Proc definitions (core surface)
///////////////////////////////////////////////////////////////////////////

proc add5(a, b, c, d, e) {
  // return expr;
  return a + b + c + d + e;
}

proc identity(x) {
  return x;
}

///////////////////////////////////////////////////////////////////////////
// 4) Main proc exercises expressions + statements
///////////////////////////////////////////////////////////////////////////

proc main() {
  ///////////////////////////////////////////////////////////////////////////
  // 4.1) let bindings
  ///////////////////////////////////////////////////////////////////////////
  let a = 1;
  let b = 2;

  ///////////////////////////////////////////////////////////////////////////
  // 4.2) literals
  ///////////////////////////////////////////////////////////////////////////
  let i_dec = 123;
  let i_underscore = 1_000_000;
  let i_hex = 0xCAFE_BABE;
  let i_bin = 0b1010_0101;

  let t = true;
  let f = false;

  let s0 = "hello";
  let s1 = "with \\n escape";
  let n = null;

  ///////////////////////////////////////////////////////////////////////////
  // 4.3) unary
  ///////////////////////////////////////////////////////////////////////////
  let u0 = -i_dec;
  let u1 = not f;
  let u2 = !f;
  let u3 = ~i_dec;

  ///////////////////////////////////////////////////////////////////////////
  // 4.4) binary arithmetic / bitwise / shifts
  ///////////////////////////////////////////////////////////////////////////
  let ar0 = a + b;
  let ar1 = a - b;
  let ar2 = a * b;
  let ar3 = 100 / b;
  let ar4 = 100 % b;

  let bw0 = a & b;
  let bw1 = a | b;
  let bw2 = a ^ b;

  // word-form bitwise
  let bw3 = a xor b;

  // shifts: symbol and word-forms both tokenize; current parser uses token precedence table
  let sh0 = i_dec shl 2;
  let sh1 = i_dec shr 1;
  let sh2 = i_dec sar 1;
  let sh3 = i_dec << 1;
  let sh4 = i_dec >> 1;

  ///////////////////////////////////////////////////////////////////////////
  // 4.5) comparisons (return boolean in bootstrap: 0/1)
  ///////////////////////////////////////////////////////////////////////////
  let c0 = a < b;
  let c1 = a <= b;
  let c2 = a > b;
  let c3 = a >= b;
  let c4 = a == b;
  let c5 = a != b;

  // v1 compatibility: single '=' is treated as equality in expression parsing
  let c6 = a = b;

  ///////////////////////////////////////////////////////////////////////////
  // 4.6) logical ops (short-circuit)
  ///////////////////////////////////////////////////////////////////////////
  let l0 = c0 and c5;
  let l1 = c0 or c4;
  let l2 = c0 && c5;
  let l3 = c0 || c4;

  ///////////////////////////////////////////////////////////////////////////
  // 4.7) ternary
  ///////////////////////////////////////////////////////////////////////////
  let te0 = c0 ? a : b;
  let te1 = (a < b) ? (a + 1) : (b + 1);

  ///////////////////////////////////////////////////////////////////////////
  // 4.8) choose max/min
  ///////////////////////////////////////////////////////////////////////////
  let ch0 = choose max(a, b);
  let ch1 = choose min(a, b);

  ///////////////////////////////////////////////////////////////////////////
  // 4.9) addr / load / store expression forms
  ///////////////////////////////////////////////////////////////////////////
  let p0 = addr(4096, 4, 8, 16);
  let y0 = load(u32, addr(4096, 0, 1, 0));
  let z0 = store(u32, addr(4096, 0, 1, 8), 7);

  ///////////////////////////////////////////////////////////////////////////
  // 4.10) mmio sugar (read32/write32 are statements that build EXPR_MMIO_ADDR + EXPR_LOAD/STORE)
  ///////////////////////////////////////////////////////////////////////////
  let x = 0;
  read32 REG, 0 into x;
  write32 REG, 4, 123;

  ///////////////////////////////////////////////////////////////////////////
  // 4.11) ident literal
  ///////////////////////////////////////////////////////////////////////////
  let sym0 = #rane_rt_print;
  let sym1 = #REG;

  ///////////////////////////////////////////////////////////////////////////
  // 4.12) calls (expression calls)
  ///////////////////////////////////////////////////////////////////////////
  let sum = add5(1, 2, 3, 4, 5);
  let idv = identity(sum);

  // print is a builtin lowered from EXPR_CALL named "print" => import "rane_rt_print" at lowering time
  print(s0);
  print(sum);
  print(idv);

  ///////////////////////////////////////////////////////////////////////////
  // 4.13) statement-form call + goto (bootstrap control flow)
  ///////////////////////////////////////////////////////////////////////////
  call identity(123) into slot 1;

  // conditional goto: goto <expr> -> <true_label>, <false_label>;
  goto (a < b) -> L_true, L_false;

label L_false;
  // trap with optional argument
  trap 7;
  goto 1 -> L_end, L_end;

label L_true;
  trap;

label L_end;
  halt;

  // return is present but unreachable after halt; keep for AST coverage
  return 0;
}

///////////////////////////////////////////////////////////////////////////
// 5) v1 node/prose surface + v1 struct surface (parse-only in this file)
//
// NOTE: The lowering path supports STMT_MODULE/STMT_NODE/STMT_START_AT/STMT_SAY/STMT_GO_NODE,
// and v1 struct decl / set / add are parsed. In the current repo, these are used in tests.
//
// CAUTION: Mixing `proc main()` (core surface) and v1 `start at node` in the same file
// may or may not be meaningful for runtime behavior, but it is still useful for syntax coverage.
///////////////////////////////////////////////////////////////////////////

module demo_struct

struct Header:
  magic: u32
  version: u16
  flags: u16
  size: u64
end

node start:
  // v1: set declaration form
  set h: Header to Header{
    magic: 0x52414E45
    version: 1
    flags: 0
    size: 4096
  }

  // v1: member read into typed var
  set m: u32 to h.magic

  // v1: set assignment into member
  set h.version to 2

  // v1: add numeric update
  add h.size by 512

  // v1: say
  say "ok"

  // v1: go to node
  go to node end_node

  // v1: halt keyword (tokenized as KW_HALT; parser also has a legacy identifier check)
  halt
end

node end_node:
  say "goodbye"
  halt
end

start at node start
