# RANE Processing Language

## Overview

RANE is an experimental programming language and native code toolchain written in C++14.

Today the repository contains a **bootstrap compiler pipeline** targeting **Windows x64** and producing a small PE executable.
The focus is on:

- A minimal language (`let`, basic expressions, `if`/`while`, builtin `print`).
- A Typed IR stage (TIR) and a small x64 emitter.
- A strict memory-band loader prototype (CORE/AOT/JIT/META/HEAP/MMAP) used by the in-process demo.

This project is in active development; many advanced features described in older docs are planned rather than fully implemented.

## Current Status

### What works today (language)

RANE currently supports a small but real subset of a language, wired end-to-end (parse → typecheck → lower → emit → run):

**Statements**
- `let name = expr;`
- `name = expr;`
- blocks: `{ ... }`
- `if cond then stmt [else stmt]`
- `while cond do stmt`
- `return;` and `return expr;` (inside `proc`)
- marker/jump style control flow: `label;` and `jump label;` (low-level but supported)

**Expressions**
- integer literals (currently treated as `u64`)
- boolean literals `true` / `false`
- string literals (implemented as pooled C-strings in `.rdata` for printing)
- variables
- arithmetic: `+ - * / %`
- comparisons: `< <= > >= == !=` (materialize as `0/1`)
- logical, short-circuiting: `and` / `or`
- unary: `-x`, `not x`, `~x`
- bitwise: `& | ^` and word-form `xor`
- shifts (word-form): `shl`, `shr`, `sar`
- function calls: `name(arg0, arg1, ...)` (parser supports up to 8 args; codegen is currently more limited)

**Procs / calls (bootstrap)**
- `proc name(params...) { ... }` exists and is compiled into the output.
- Calling user procs works, but **argument passing is currently only implemented for the first two arguments** (Win64: `RCX`/`RDX`).

### What works today (toolchain)
- Lexer / parser / type checker.
- AST → TIR lowering.
- TIR → x64 machine code generation.
- PE writer that emits a minimal `.text + .rdata + .idata` executable importing `msvcrt.dll!printf`.

### Fixups / correctness
- Imported calls are patched deterministically via **call-site fixups** recorded during codegen.
- Branch/label fixups are applied using rel32 patching.

### Optimizations (implemented, small)
- Local peephole: fold simple MOV chains.
- Local dead code elimination (basic liveness sweep).

## What it can be used for right now

Practical uses today:
- A compiler/codegen sandbox for iterating on parsing, lowering, control flow, fixups, and x64 emission.
- Small demo programs that compute integer results, branch/loop, and print strings.
- Learning: a minimal end-to-end pipeline that produces a real runnable PE.

Not realistic today:
- General-purpose application development.
- Cross-platform use (currently Windows x64 PE only).
- Data-structure-heavy programs (no real strings, arrays, structs, modules, etc.).
- Reliable multi-argument function calls and a stable ABI beyond the current subset.

## Building

This repo is intended to build with **Visual Studio** using **C++14**.

- Open `Rane Processing Language.vcxproj`
- Build `Release|x64`

## Usage

### Compile a `.rane` file to a Windows EXE

From the built executable:

- `Rane Processing Language.exe input.rane -o output.exe -O2`

Optimization flags currently parsed: `-O0 -O1 -O2 -O3`.

## Roadmap (practical stages)

The goal of this section is to outline concrete steps from the current bootstrap compiler to:
1) a **minimal complete language**, and eventually
2) a **professionally robust** toolchain.

### Stage 1 — “Minimal complete language” (usable for small programs)

Core work needed to make the language coherent and predictable:

- **Function calling convention (Win64) done correctly**
  - Support 4 register args (`RCX/RDX/R8/R9`) + stack args
  - Shadow space, stack alignment, and correct prologues/epilogues
  - Clear rules for return values

- **Type system definition (even if small)**
  - Decide/implement: signed vs unsigned, overflow behavior, casts
  - Distinguish `bool` from integers (or define booleanization rules)

- **A minimal “real string” story**
  - Define string type semantics (length, ownership, printing)

- **Diagnostics / errors improvements**
  - Better parse/type errors with spans and messages

Outcome: small but reliable programs with functions, control flow, and basic types.

### Stage 2 — “General-purpose core” (data + modules)

- Modules/imports and name resolution
- User-defined data: `struct` + field access
- Arrays/slices + indexing (optionally bounds checks)
- A minimal standard library surface (string formatting, files, etc.)

Outcome: real programs can be structured and maintained.

### Stage 3 — “Toolchain maturity” (debugging + performance)

- Replace placeholder SSA/regalloc with correct implementations
- CFG-based optimizations (CSE/GVN, SCCP, CFG-aware DCE, inlining)
- Debug info (at least line tables) and a debugger story
- Benchmark suite + regression tracking

Outcome: predictable performance and debuggability.

### Stage 4 — “Professional robustness” (ecosystem + hardening)

- Formatter, linter, language server (LSP)
- CI with fuzzing for parser/typechecker/codegen
- Test runner and packaging/build tooling
- Security posture: reproducible builds, dependency hygiene
- Portability strategy (additional OS/architectures or an LLVM backend)

Outcome: strong engineering practices and an ecosystem that supports real teams.
