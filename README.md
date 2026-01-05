# RANE Processing Language

## Overview

RANE is an experimental programming language and native code toolchain written in C++14.

Today the repository contains a **bootstrap compiler pipeline** targeting **Windows x64** and producing a small PE executable.
The focus is on:

- A minimal language (`let`, basic expressions, `if`/`while`, builtin `print`).
- A Typed IR stage (TIR) and a small x64 emitter.
- A strict memory-band loader prototype (CORE/AOT/JIT/META/HEAP/MMAP) used by the in-process demo.

This project is in active development; many advanced features described in older docs are planned rather than fully implemented.

## Syntax Examples (end-to-end: parses + typechecks + codegen)

This section shows **concrete syntax examples** that currently compile through the pipeline (parse → typecheck → TIR → x64).

### Tokens (selected)

**Identifiers**
- `foo`
- `_tmp`
- `memcpy` (can be called as an import)

**Integer literals**
- `0`
- `42`
- `1_000_000`
- `0xCAFE_BABE`
- `0b1010_0101`

**String literals** (double quoted)
- `"hello"`

**Booleans**
- `true`
- `false`

**Operators / punctuation**
- arithmetic: `+ - * / %`
- bitwise: `& | ^ ~`
- comparisons: `< <= > >= == !=`
- logical: `and or` (also tokenizes `&&` / `||`)
- ternary: `? :`
- calls: `( )` and argument comma `,`
- indexing/member tokens exist: `[ ]` and `.` (parses; codegen is currently placeholder)
- statement terminator: `;`
- blocks: `{ }`

### Types (bootstrap)

Types are inferred in the bootstrap compiler; the following are used internally and in builtins:
- integers: `u64` (default for integer literals)
- booleans: `b1`
- pointer-sized: `p64`
- text/bytes: `text`, `bytes` (used by `say` surface; `print` accepts pointer-like values)

### Expressions

**Literals / variables**
```rane
42
true
"hello"
x
```

**Arithmetic**
```rane
(1 + 2) * 3
10 / 2
10 % 3
```

**Bitwise / shifts**
```rane
(a & b) | (c ^ d)
~x
x << 3
x >> 1
```

**Comparisons** (result is boolean-like `0/1`)
```rane
x < 10
x == 0
x != y
```

**Logical (short-circuit)**
```rane
(x != 0) and (y != 0)
(x == 0) or (y == 0)
```

**Ternary**
```rane
x != 0 ? 1 : 0
```

**Calls**
```rane
print("hello")
foo(1, 2, 3)
```

**Addressing / memory builtins** (built into the parser)
```rane
addr(base, index, scale, disp)
load(u64, addr(...))
store(u64, addr(...), value)
```

### Declarations

**Local binding**
```rane
let x = 123;
let y = x + 1;
```

**Procedure declaration**

`proc`/`def` is tokenized, and procedure lowering exists; procedure parsing is present in this codebase.
Example form used by the toolchain:
```rane
proc add(a, b) {
  return a + b;
}
```

### Statements

**Assignment**
```rane
x = 1;
x = x + 1;
```

**Block**
```rane
{
  let x = 1;
  x = x + 1;
}
```

**If / while**
```rane
if x != 0 then {
  print("nonzero");
} else {
  print("zero");
}

while x != 0 do {
  x = x - 1;
}
```

**Return** (inside `proc`)
```rane
return;
return 123;
```

**Call statement (explicit)**
```rane
call foo(1, 2, 3);
call foo(1) into slot 1;
```

**Low-level control flow**
```rane
start;
jump start;

goto x != 0 -> true_label, false_label;
```

**MMIO helpers**
```rane
mmio region UART from 0x1000 size 0x100;
read32 UART, 0x0 into v;
write32 UART, 0x0, 0x1;
```

**Memcpy helper**
```rane
mem copy dst, src, size;
```

### Adverbs / directives

The lexer tokenizes many "directive-like" keywords (e.g. `zone`, `hot`, `cold`, `deterministic`) but only some are currently semantic.

**Zone blocks** (parsed; treated as a normal block in the bootstrap lowering)
```rane
zone hot {
  print("fast path");
}
```

**Imports / exports** (lowered to TIR decls)
```rane
import sys.alloc;
export my_symbol;
```

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
- ternary: `cond ? then : else`
- function calls: `name(arg0, arg1, ...)` (parser supports up to 8 args)

**Procs / calls (bootstrap)**
- `proc name(params...) { ... }` exists and is compiled into the output.
- Calling user procs supports Win64 calling convention with up to 4 register args (`RCX/RDX/R8/R9`) and stack args for remaining parameters.

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

### Stage 1 – “Minimal complete language” (usable for small programs)

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

### Stage 2 – “General-purpose core” (data + modules)

- Modules/imports and name resolution
- User-defined data: `struct` + field access
- Arrays/slices + indexing (optionally bounds checks)
- A minimal standard library surface (string formatting, files, etc.)

Outcome: real programs can be structured and maintained.

### Stage 3 – “Toolchain maturity” (debugging + performance)

- Replace placeholder SSA/regalloc with correct implementations
- CFG-based optimizations (CSE/GVN, SCCP, CFG-aware DCE, inlining)
- Debug info (at least line tables) and a debugger story
- Benchmark suite + regression tracking

Outcome: predictable performance and debuggability.

### Stage 4 – “Professional robustness” (ecosystem + hardening)

- Formatter, linter, language server (LSP)
- CI with fuzzing for parser/typechecker/codegen
- Test runner and packaging/build tooling
- Security posture: reproducible builds, dependency hygiene
- Portability strategy (additional OS/architectures or an LLVM backend)

Outcome: strong engineering practices and an ecosystem that supports real teams.

CURRENTLY IN STAGE 1.

THIS IS WHERE WE ARE TODAY.

Rane Processing Language — short pitch
Rane is a small, Windows-first compiled language and toolchain designed for experimenting with “full stack” compilation: a compact front-end, a simple typed AST, a custom IR pipeline (TIR → SSA → regalloc → opts), and an AOT backend that can emit a standalone PE32+ x64 .exe or generate C.
Alongside the core compiler, the repo includes early scaffolding for a hardened runtime/loader model (banded address spaces, W^X/JIT policy hooks, diagnostics/crash records) and optional runtime subsystems (GC, stdlib stubs), making it a good playground for building and testing compiler/runtime ideas without pulling in a heavyweight dependency like LLVM.
---
What works end-to-end today (parse → typecheck → codegen)
These are implemented enough to run through the pipeline and produce output:
•	Tooling / pipeline
•	Source file compilation via rane_driver through:
•	Lexer → Parser → AST
•	Typechecking
•	TIR lowering
•	SSA + register allocation
•	Optimization pass framework (basic)
•	AOT x64 codegen (emits a PE32+ executable on Windows)
•	C backend (-emit-c) for generating C output
•	Core language subset (v0-ish)
•	Integer and boolean literals
•	Variables and assignment (slot-based locals)
•	Basic expressions (arithmetic / comparisons / boolean ops)
•	Control flow at IR level: labels/jumps exist and are used
•	“v1 prose/node” surface (partially end-to-end)
•	Tokens exist and are recognized for: module, node, start at node, go to node, say
•	Lowering exists for node-style constructs:
•	Nodes become labels
•	go to node X lowers to an unconditional jump
•	say lowers to a print-like call (wired to printf through the PE import patching)
•	Note: as of the current state, node body termination is buggy (parsing end in node bodies is failing), so this surface is close but not consistently runnable across the provided tests.
---
Implemented in the front-end, but not fully end-to-end yet
These parse (and may build AST), but are not guaranteed to typecheck/lower/codegen correctly yet:
•	v1 struct literals
•	Parsing added for:
•	TypeName{ field: expr ... } (named)
•	TypeName(expr, ...) (positional)
•	The back-end/typechecker/lowering side is not yet complete, so it’s front-end only at the moment.
•	v1 structured-data statements
•	Lexer tokens and some parser hooks exist for things like set, and related “structured-data” syntax.
•	Full semantic integration (typecheck + lowering + codegen) is not complete.
---
Scaffolding / experimental subsystems (not wired into compiled-program semantics)
These exist as modules but aren’t currently part of the “compile a program and run it” story:
•	Runtime/loader security model
•	rane_loader.h defines a banded virtual address layout (CORE/AOT/JIT/META/HEAP/MMAP), policy enforcement hooks, diagnostic blocks, and crash record formats.
•	This is primarily an architectural foundation; it’s not yet the default execution path for compiled programs.
•	Garbage collector
•	rane_gc.cpp provides a refcount API plus an optional mark/sweep collector with root management and a tracing callback.
•	Not currently integrated into the language runtime/codegen.
•	Stdlib and utilities
•	Networking, threading, containers, crypto, etc. exist mostly as stubs or standalone libs, not as fully integrated language-level features.
---
Practical “today” usage
•	If your goal is compiler hacking (front-end → IR → x64 codegen, PE emission, C output), the core pipeline is already in place.
•	If your goal is the v1 prose/node scripting surface, most pieces exist but node-body parsing needs finishing.
•	If your goal is managed/runtime features (GC, sandboxed loader, hardened JIT), the repo contains the building blocks, but they’re currently scaffolding rather than default behavior.
