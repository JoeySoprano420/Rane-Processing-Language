# RANE Processing Language

## Overview

RANE is an experimental programming language and native code toolchain written in C++14.

Today the repository contains a **bootstrap compiler pipeline** targeting **Windows x64** and producing a small PE executable.
The focus is on:

- A minimal language (`let`, basic expressions, `if`/`while`, builtin `print`).
- A Typed IR stage (TIR) and a small x64 emitter.
- A strict memory-band loader prototype (CORE/AOT/JIT/META/HEAP/MMAP) used by the in-process demo.

This project is in active development and many advanced features described in older docs are *planned* rather than fully implemented.

## Current Status (what works)

### Language (bootstrap)
- String literals and `let` bindings (used for `print`).
- Builtin `print(x)` lowering to an imported `printf` call.
- `if` lowering that always generates a valid `else` flow.
- `while` loops.
- Comparison operators (`< <= > >= == !=`) lowered to a boolean `0/1` value.
- Bitwise operators: `& | ^`, plus word-form `xor`.
- Unary boolean `not`.
- Shift operators (word-form): `shl`, `shr`, `sar`.

### Toolchain
- Lexer / parser / type checker.
- AST ? TIR lowering.
- TIR ? x64 machine code generation.
- PE writer that emits a minimal `.text + .rdata + .idata` executable importing `msvcrt.dll!printf`.

### Fixups / correctness
- Imported calls are patched deterministically via **call-site fixups** recorded during codegen (no more “patch the first call” heuristic).
- Branch/label fixups are applied using rel32 patching.

### Optimizations (implemented, small)
- Local peephole: fold simple MOV chains.
- Local dead code elimination pass (basic liveness sweep).

## Building

This repo is intended to build with **Visual Studio** using **C++14**.

- Open `Rane Processing Language.vcxproj`
- Build `Release|x64`

## Usage

### Compile a `.rane` file to a Windows EXE

From the built executable:

- `Rane Processing Language.exe input.rane -o output.exe -O2`

Optimization flags currently parsed: `-O0 -O1 -O2 -O3`.