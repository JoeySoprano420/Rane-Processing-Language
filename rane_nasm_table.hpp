// rane_nasm_table.hpp
// EXECUTABLE 1:1 RANE(Typed-CIL op) → NASM-x64 template table (Windows x64 ABI)
// C++20 wrapper + tiny expander/emitter.
//
// Key points (matches your constraints):
// - Pick Entry by op string.
// - Substitute placeholders: {dst},{src},{imm64},{lbl}…
// - Emit NASM lines into a ".text" text buffer.
// - Regalloc provides concrete operand strings ("rax", "r10", "qword [rsp+32]", ...)
// - Backend is RSP-only by default; RBP is free unless you enable frame pointer.
// - Windows x64 ABI notes included in call patterns.
//
// This is "executable" as code/data: you can compile this and call emit_op(...) to get NASM.
//
// Drop-in usage idea:
//   Env env; env.put("dst","rax"); env.put("a","r10"); env.put("b","qword [rsp+32]");
//   AsmText out; emit_op(out, "i64.add", env);

#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rane::nasm {

// ---------------------------------------------
// 0) Table data model
// ---------------------------------------------
enum class Width : int32_t { W8 = 8, W16 = 16, W32 = 32, W64 = 64 };

enum class Kind : int32_t {
  Prolog, Epilog,
  Move, Lea, Load, Store,
  BinOp, UnOp, Cmp, Setcc,
  Branch, Call, Ret, Label,
  Trap, InlineAsm, Comment
};

struct Entry {
  std::string_view op;      // canonical op name, e.g. "i64.add", "br.if"
  Kind            kind;
  Width           width;
  std::string_view form;    // short signature / placeholder intent
  std::array<std::string_view, 8> nasm; // up to 8 lines
};

// ---------------------------------------------
// 1) Small placeholder env: {name} -> string
// ---------------------------------------------
struct Env {
  std::unordered_map<std::string, std::string> kv;

  void put(std::string key, std::string value) { kv.emplace(std::move(key), std::move(value)); }
  void set(std::string key, std::string value) { kv[std::move(key)] = std::move(value); }

  // returns nullptr if absent
  const std::string* get(std::string_view key) const {
    auto it = kv.find(std::string(key));
    if (it == kv.end()) return nullptr;
    return &it->second;
  }
};

// ---------------------------------------------
// 2) Output buffer for NASM (text .asm)
// ---------------------------------------------
struct AsmText {
  std::vector<std::string> lines;

  void line(std::string s) { lines.emplace_back(std::move(s)); }
  void blank() { lines.emplace_back(std::string{}); }

  std::string join() const {
    std::string out;
    out.reserve(lines.size() * 32);
    for (auto const& ln : lines) {
      out += ln;
      out += '\n';
    }
    return out;
  }
};

// ---------------------------------------------
// 3) Placeholder substitution
//    - replaces {name} with env[name]
//    - if missing, keeps "{name}" (debug-friendly)
// ---------------------------------------------
inline std::string subst(std::string_view tmpl, const Env& env) {
  std::string out;
  out.reserve(tmpl.size() + 16);

  for (size_t i = 0; i < tmpl.size();) {
    const char c = tmpl[i];
    if (c != '{') {
      out.push_back(c);
      ++i;
      continue;
    }

    // parse {name}
    size_t j = i + 1;
    while (j < tmpl.size() && tmpl[j] != '}') ++j;
    if (j >= tmpl.size()) {
      // unmatched '{' => keep literal
      out.push_back('{');
      ++i;
      continue;
    }

    std::string_view name = tmpl.substr(i + 1, j - (i + 1));
    if (auto* v = env.get(name)) {
      out += *v;
    } else {
      out.push_back('{');
      out.append(name.begin(), name.end());
      out.push_back('}');
    }
    i = j + 1;
  }
  return out;
}

// ---------------------------------------------
// 4) Lookup + emit
// ---------------------------------------------
inline const Entry* find_entry(std::string_view op);

// Emits all non-empty lines for the op.
inline void emit_op(AsmText& out, std::string_view op, const Env& env) {
  const Entry* e = find_entry(op);
  if (!e) {
    out.line("; ERROR: unknown op " + std::string(op));
    return;
  }
  for (auto ln : e->nasm) {
    if (ln.empty()) continue;
    out.line(subst(ln, env));
  }
}

// ---------------------------------------------
// 5) RSP-only prolog/epilog helpers
//    - Default: rsp-only, rbp free
//    - Optional: rbp frame pointer (debug/interop)
// ---------------------------------------------
inline void emit_prolog(AsmText& out, const Env& env, bool use_rbp_frameptr) {
  if (use_rbp_frameptr) {
    emit_op(out, "fn.prolog.rbp", env);
  } else {
    // rsp-only: expects {frame_size_aligned}
    out.line(subst("sub rsp, {frame_size_aligned}", env));
    out.line("; (rsp-only) rbp is free for GP use unless you enable frame pointer");
  }
}
inline void emit_epilog(AsmText& out, const Env& env, bool use_rbp_frameptr) {
  if (use_rbp_frameptr) {
    emit_op(out, "fn.epilog.rbp", env);
  } else {
    out.line(subst("add rsp, {frame_size_aligned}", env));
    out.line("ret");
  }
}

// ============================================================================
// 6) The actual 1:1 TABLE (Typed-CIL op → NASM templates)
//    Scope: the constructs you demoed + Spec 0.1 core building blocks.
//    “1:1” here means from Typed-CIL op -> NASM template lines.
// ============================================================================

static constexpr std::array<Entry, 64> TABLE = {{
  // ------------------------------
  // Prolog/Epilog (optional frameptr)
  // ------------------------------
  Entry{
    "fn.prolog.rbp", Kind::Prolog, Width::W64, "frame_size_aligned, save_nonvol",
    {
      "push rbp",
      "mov rbp, rsp",
      "sub rsp, {frame_size_aligned}",
      "; save non-volatiles if used (rbx,rdi,rsi,r12-r15)",
      "", "", "", ""
    }
  },
  Entry{
    "fn.epilog.rbp", Kind::Epilog, Width::W64, "restore_nonvol",
    {
      "; restore non-volatiles if saved",
      "mov rsp, rbp",
      "pop rbp",
      "ret",
      "", "", "", ""
    }
  },

  // ------------------------------
  // Moves / constants / address
  // ------------------------------
  Entry{
    "i64.mov", Kind::Move, Width::W64, "dst, src",
    { "mov {dst}, {src}", "", "", "", "", "", "", "" }
  },
  Entry{
    "i32.mov", Kind::Move, Width::W32, "dst32, src32",
    { "mov {dst32}, {src32}", "", "", "", "", "", "", "" }
  },
  Entry{
    "i64.const", Kind::Move, Width::W64, "dst, imm64",
    { "mov {dst}, {imm64}", "", "", "", "", "", "", "" }
  },
  Entry{
    "addr.lea", Kind::Lea, Width::W64, "dst, base, index, scale, disp",
    { "lea {dst}, [{base} + {index}*{scale} + {disp}]", "", "", "", "", "", "", "" }
  },

  // ------------------------------
  // Loads / stores
  // ------------------------------
  Entry{
    "i64.load", Kind::Load, Width::W64, "dst, addr_reg, disp",
    { "mov {dst}, qword [{addr_reg} + {disp}]", "", "", "", "", "", "", "" }
  },
  Entry{
    "i32.load", Kind::Load, Width::W32, "dst32, addr_reg, disp",
    { "mov {dst32}, dword [{addr_reg} + {disp}]", "", "", "", "", "", "", "" }
  },
  Entry{
    "i64.store", Kind::Store, Width::W64, "addr_reg, disp, src",
    { "mov qword [{addr_reg} + {disp}], {src}", "", "", "", "", "", "", "" }
  },
  Entry{
    "i32.store", Kind::Store, Width::W32, "addr_reg, disp, src32",
    { "mov dword [{addr_reg} + {disp}], {src32}", "", "", "", "", "", "", "" }
  },

  // Absolute address helpers (MMIO/addr demo style)
  Entry{
    "mmio.read32.abs", Kind::Load, Width::W32, "dst32, addr64, off, tmp",
    {
      "mov {tmp}, {addr64}",
      "mov {dst32}, dword [{tmp} + {off}]",
      "", "", "", "", "", ""
    }
  },
  Entry{
    "mmio.write32.abs", Kind::Store, Width::W32, "addr64, off, src32, tmp",
    {
      "mov {tmp}, {addr64}",
      "mov dword [{tmp} + {off}], {src32}",
      "", "", "", "", "", ""
    }
  },

  // ------------------------------
  // Arithmetic / bitwise (i64)
  // ------------------------------
  Entry{
    "i64.add", Kind::BinOp, Width::W64, "dst, a, b",
    { "mov {dst}, {a}", "add {dst}, {b}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.sub", Kind::BinOp, Width::W64, "dst, a, b",
    { "mov {dst}, {a}", "sub {dst}, {b}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.mul", Kind::BinOp, Width::W64, "dst, a, b",
    { "mov {dst}, {a}", "imul {dst}, {b}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.div", Kind::BinOp, Width::W64, "dst, a, b",
    {
      "mov rax, {a}",
      "cqo",
      "idiv {b}",
      "mov {dst}, rax",
      "", "", "", ""
    }
  },
  Entry{
    "i64.mod", Kind::BinOp, Width::W64, "dst, a, b",
    {
      "mov rax, {a}",
      "cqo",
      "idiv {b}",
      "mov {dst}, rdx",
      "", "", "", ""
    }
  },
  Entry{
    "i64.and", Kind::BinOp, Width::W64, "dst, a, b",
    { "mov {dst}, {a}", "and {dst}, {b}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.or", Kind::BinOp, Width::W64, "dst, a, b",
    { "mov {dst}, {a}", "or {dst}, {b}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.xor", Kind::BinOp, Width::W64, "dst, a, b",
    { "mov {dst}, {a}", "xor {dst}, {b}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.shl", Kind::BinOp, Width::W64, "dst, a, shamt8",
    { "mov {dst}, {a}", "mov cl, {shamt8}", "shl {dst}, cl", "", "", "", "", "" }
  },
  Entry{
    "i64.shr", Kind::BinOp, Width::W64, "dst, a, shamt8",
    { "mov {dst}, {a}", "mov cl, {shamt8}", "shr {dst}, cl", "", "", "", "", "" }
  },
  Entry{
    "i64.sar", Kind::BinOp, Width::W64, "dst, a, shamt8",
    { "mov {dst}, {a}", "mov cl, {shamt8}", "sar {dst}, cl", "", "", "", "", "" }
  },

  // ------------------------------
  // Unary
  // ------------------------------
  Entry{
    "i64.neg", Kind::UnOp, Width::W64, "dst, a",
    { "mov {dst}, {a}", "neg {dst}", "", "", "", "", "", "" }
  },
  Entry{
    "i64.notbits", Kind::UnOp, Width::W64, "dst, a",
    { "mov {dst}, {a}", "not {dst}", "", "", "", "", "", "" }
  },
  Entry{
    "bool.not", Kind::UnOp, Width::W32, "dst8, a",
    { "cmp {a}, 0", "sete {dst8}", "", "", "", "", "", "" }
  },

  // ------------------------------
  // Comparisons (i64 -> bool in dst8)
  // ------------------------------
  Entry{ "i64.cmp.lt", Kind::Cmp, Width::W64, "dst8, a, b", { "cmp {a}, {b}", "setl {dst8}", "", "", "", "", "", "" } },
  Entry{ "i64.cmp.le", Kind::Cmp, Width::W64, "dst8, a, b", { "cmp {a}, {b}", "setle {dst8}", "", "", "", "", "", "" } },
  Entry{ "i64.cmp.gt", Kind::Cmp, Width::W64, "dst8, a, b", { "cmp {a}, {b}", "setg {dst8}", "", "", "", "", "", "" } },
  Entry{ "i64.cmp.ge", Kind::Cmp, Width::W64, "dst8, a, b", { "cmp {a}, {b}", "setge {dst8}", "", "", "", "", "", "" } },
  Entry{ "i64.cmp.eq", Kind::Cmp, Width::W64, "dst8, a, b", { "cmp {a}, {b}", "sete {dst8}", "", "", "", "", "", "" } },
  Entry{ "i64.cmp.ne", Kind::Cmp, Width::W64, "dst8, a, b", { "cmp {a}, {b}", "setne {dst8}", "", "", "", "", "", "" } },

  // ------------------------------
  // Labels / branching / goto
  // ------------------------------
  Entry{
    "label", Kind::Label, Width::W64, "lbl",
    { "{lbl}:", "", "", "", "", "", "", "" }
  },
  Entry{
    "br", Kind::Branch, Width::W64, "lbl",
    { "jmp {lbl}", "", "", "", "", "", "", "" }
  },
  Entry{
    "br.if", Kind::Branch, Width::W32, "cond, lbl_true, lbl_false",
    { "cmp {cond}, 0", "jne {lbl_true}", "jmp {lbl_false}", "", "", "", "", "" }
  },
  Entry{
    "goto.cond", Kind::Branch, Width::W32, "cond, lbl_true, lbl_false",
    { "cmp {cond}, 0", "jne {lbl_true}", "jmp {lbl_false}", "", "", "", "", "" }
  },

  // ------------------------------
  // Calls / returns (Windows x64 ABI)
  // ------------------------------
  // call.sym:
  // - caller must ensure:
  //   - RCX,RDX,R8,R9 args set
  //   - stack args placed above shadow
  //   - 32-byte shadow space reserved
  //   - RSP 16-byte aligned at CALL
  //
  // {shadow_and_align} typically:
  //   0x20 (shadow) + optional extra 0x8 to fix alignment depending on current depth.
  Entry{
    "call.sym", Kind::Call, Width::W64, "sym, shadow_and_align",
    { "sub rsp, {shadow_and_align}", "call {sym}", "add rsp, {shadow_and_align}", "", "", "", "", "" }
  },
  // ret: (value already in rax/eax by convention)
  Entry{
    "ret", Kind::Ret, Width::W64, "void",
    { "ret", "", "", "", "", "", "", "" }
  },

  // ------------------------------
  // Trap / Halt
  // ------------------------------
  Entry{
    "trap", Kind::Trap, Width::W64, "opt(code)",
    { "int3", "", "", "", "", "", "", "" }
  },
  Entry{
    "halt", Kind::Trap, Width::W64, "void",
    { "ud2", "", "", "", "", "", "", "" }
  },

  // ------------------------------
  // Casts / extends (needed often in your demo: (u32 as i64), etc.)
  // ------------------------------
  // i32 -> i64 sign-extend: movsxd r64, r/m32
  Entry{
    "i64.sext.i32", Kind::Move, Width::W64, "dst, src32",
    { "movsxd {dst}, {src32}", "", "", "", "", "", "", "" }
  },
  // u32 -> u64 zero-extend: writing to r32 already zero-extends; but if src is memory, load into dst32.
  Entry{
    "u64.zext.u32", Kind::Move, Width::W64, "dst, src32_or_mem",
    { "mov {dst32}, {src32_or_mem}", "; (zext) upper 32 are now zero", "", "", "", "", "", "" }
  },

  // ------------------------------
  // Minimal boolean normalize (dst8 = (a != 0))
  // ------------------------------
  Entry{
    "bool.from.i64", Kind::UnOp, Width::W32, "dst8, a",
    { "cmp {a}, 0", "setne {dst8}", "", "", "", "", "", "" }
  },
}};

// Table lookup implementation
inline const Entry* find_entry(std::string_view op) {
  for (auto const& e : TABLE) {
    if (e.op == op) return &e;
  }
  return nullptr;
}

} // namespace rane::nasm

// ============================================================================
// Optional: small demo main (compile by defining RANE_NASM_TABLE_DEMO)
// ============================================================================

#ifdef RANE_NASM_TABLE_DEMO
#include <iostream>

int main() {
  using namespace rane::nasm;

  AsmText out;
  Env env;

  out.line("section .text");
  out.line("global main");
  out.line("main:");

  // rsp-only frame
  env.set("frame_size_aligned", "0x60");
  emit_prolog(out, env, /*use_rbp_frameptr=*/false);

  // i64.const r10, 42
  env.set("dst", "r10");
  env.set("imm64", "42");
  emit_op(out, "i64.const", env);

  // i64.add rax = r10 + qword [rsp+32]
  env.set("dst", "rax");
  env.set("a", "r10");
  env.set("b", "qword [rsp+32]");
  emit_op(out, "i64.add", env);

  // i64.cmp.lt al = (rax < 100)
  env.set("dst8", "al");
  env.set("a", "rax");
  env.set("b", "100");
  emit_op(out, "i64.cmp.lt", env);

  // br.if al -> L_true/L_false
  env.set("cond", "al");
  env.set("lbl_true", "L_true");
  env.set("lbl_false", "L_false");
  emit_op(out, "br.if", env);

  env.set("lbl", "L_true");
  emit_op(out, "label", env);
  emit_op(out, "trap", env);
  env.set("lbl", "L_end");
  emit_op(out, "br", env);

  env.set("lbl", "L_false");
  emit_op(out, "label", env);
  emit_op(out, "halt", env);

  env.set("lbl", "L_end");
  emit_op(out, "label", env);

  emit_epilog(out, env, /*use_rbp_frameptr=*/false);

  std::cout << out.join();
  return 0;
}
#endif
