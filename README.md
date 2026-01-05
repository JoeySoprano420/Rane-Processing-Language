# RANE Processing Language

## Overview

RANE is an experimental programming language and native code toolchain written in C++14.

Today the repository contains a **bootstrap compiler pipeline** targeting **Windows x64** and producing a small PE executable.
The focus is on:

- A minimal language (`let`, basic expressions, `if`/`while`, builtin `print`).
- A Typed IR stage (TIR) and a small x64 emitter.
- A strict memory-band loader prototype (CORE/AOT/JIT/META/HEAP/MMAP) used by the in-process demo.

This project is in active development; many advanced features described in older docs are planned rather than fully implemented.
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
- AST → TIR lowering.
- TIR → x64 machine code generation.
- PE writer that emits a minimal `.text + .rdata + .idata` executable importing `msvcrt.dll!printf`.

### Fixups / correctness
- Imported calls are patched deterministically via **call-site fixups** recorded during codegen.
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

*****

1/5/2026

# RANE Processing Language

## Overview

RANE is an experimental programming language and native code toolchain written in C++14.

Today the repository contains a **bootstrap compiler pipeline** targeting **Windows x64** and producing a small PE executable.
The focus is on:

- A minimal language (`let`, basic expressions, `if`/`while`, builtin `print`).
- A Typed IR stage (TIR) and a small x64 emitter.
- A strict memory-band loader prototype (CORE/AOT/JIT/META/HEAP/MMAP) used by the in-process demo.

This project is in active development; many advanced features described in older docs are planned rather than fully implemented.
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
- AST → TIR lowering.
- TIR → x64 machine code generation.
- PE writer that emits a minimal `.text + .rdata + .idata` executable importing `msvcrt.dll!printf`.

### Fixups / correctness
- Imported calls are patched deterministically via **call-site fixups** recorded during codegen.
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
