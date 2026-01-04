# RANE Processing Language Breakdown

This document summarizes what is **implemented today** in this repository vs. what is **planned**.

## What RANE is (in this repo)

RANE is an experimental language toolchain implemented in C++14.
The current focus is a small end-to-end pipeline that compiles a `.rane` source file into a minimal Windows x64 PE executable.

## What works today

### Frontend
- Lexer / parser building an AST.
- Type checking (bootstrap level).

### IR + lowering
- AST ? Typed IR (TIR).
- `if` lowering always produces a valid `else` control-flow shape.
- Comparison operators (`< <= > >= == !=`) lower into a boolean `0/1` value.

### Backend
- TIR ? x64 emission.
- Rel32 label fixups for branches.
- Deterministic import call fixups: imported calls record call-site offsets so the driver can patch them reliably.

### Output
- A small PE writer emits:
  - `.text` (code)
  - `.rdata` (format/user strings)
  - `.idata` importing `msvcrt.dll!printf`

### Optimizations (small but real)
- Local peephole MOV-chain folding.
- Local dead-code elimination.

## What is planned / partially stubbed

The codebase contains placeholders for more advanced features (SSA enhancements, richer optimizations, a larger stdlib, stronger diagnostics).
These are not all complete yet.

Near-term realistic additions:
- Constant folding/propagation on TIR.
- Better CFG-aware DCE.
- Cleaner separation of “frontend IR” vs “machine IR”.

## Who it’s useful for (right now)

- Learning/experimenting with compiler plumbing (AST ? IR ? machine code).
- Testing lowering/codegen ideas in a small, hackable codebase.

## Notes

Older versions of this document referenced a much larger feature set. This file has been updated to match the current implementation.