# RANE Tutorial (Bootstrap)
**Last updated:** 2026-01-06

This tutorial teaches how to write programs in the RANE language implemented by this repository’s bootstrap compiler, including the currently implemented syntax and practical examples.

> Accuracy guarantee
>
> This tutorial documents what the compiler in this repo can *actually* tokenize/parse/typecheck/lower today.
> Tokens and keywords that exist in the lexer but are not yet implemented semantically are called out as **reserved**.

---

## 1) What you can build today

With the current bootstrap compiler, you can compile `.rane` files into:

- a Windows x64 PE executable (default path)
- or a C file via `-emit-c` (then compile it with a C compiler)

RANE currently supports:
- a v1 “node/prose” surface (`module`, `node`, `say`, `start at node`, `go to node`, `end`)
- a low-level “core/bootstrap” surface for explicit memory and control flow (`mmio`, `read32`, `write32`, `goto`, `call`, `trap`, `halt`, `mem copy/zero/fill`)

---

## 2) Building and running

### Build (Visual Studio 2026)
- Open the solution/project in Visual Studio
- Select **Release | x64**
- Run __Build Solution__

### Emit C instead
- Append `-emit-c` to the compiler flags
- Build normally
- The C file appears next to your `.rane` file
- Compile the C file with a C compiler

---

## 3) Program structure (v1 node surface)

A minimal runnable `.rane` program formats like:

````````
module example

node main:
  say "Hello, world!"
  halt

start at node main
````````

### Meaning
- `module <name>`: module metadata
- `node <name>:`: defines a node with a block-like body
- `say <expr>`: prints a string/text expression
- `halt`: terminates execution
- `end`: closes node bodies (and some other v1 constructs)
- `start at node <name>`: selects initial node

### Notes
- Semicolons are optional in many v1 locations; the parser also accepts semicolons.
- Strings are pooled into `.rdata` and referenced by address in generated code.

---

## 4) Expressions (implemented)

RANE expressions are used in v1 `say` and several core statements.

### 4.1 Literals
````````

### 4.2 Variables
````````

### 4.3 Unary operators
- `-x` (negation)
- `not x` or `!x` (logical not)
- `~x` (bitwise not)

### 4.4 Binary operators
Arithmetic:
- `+ - * / %`

Bitwise:
- `& | ^`
- shifts: `<< >>`
- word forms: `and or xor shl shr sar` are tokenized; semantics are partially shared with operator forms in expressions.

Comparisons:
- `< <= > >= == !=`
- v1 note: single `=` is treated as equality in expression parsing.

Logical:
- `and`, `or`
- `&&`, `||` (tokenized)

Ternary:
- `cond ? thenExpr : elseExpr`

### 4.5 Calls and postfix
- call: `f(a, b)`
- member: `a.b`
- index: `a[i]`
- call-on-expression: `(getFn())(1, 2)`

---

## 5) Built-in low-level helpers (implemented)

These are parsed as expressions and lowered directly.

### 5.1 Address calculation
````````

Example:
````````

### 5.2 Load/store
````````

Types accepted in `load/store`:
- `u8 u16 u32 u64`
- `i8 i16 i32 i64`
- `p64`
- `b1` / `bool`

---

## 6) Statements (implemented surface)

> The bootstrap grammar currently mixes surfaces. The clearest source of truth is `rane_parser.cpp` (`parse_stmt()`).

### 6.1 v1 statements
- `module <ident>`
- `node <ident>: ... end`
- `start at node <ident>`
- `go to node <ident>`
- `say <expr>`
- `halt`

### 6.2 Control flow (bootstrap)
Conditional jump:
````````

### 6.3 Calls (bootstrap statement form)
````````

Notes:
- This is a low-level call statement form, not the same as `expr_call(...)`.
- `slot` is a bootstrap mechanism for capturing call results.

### 6.4 MMIO
````````

### 6.5 Memory helper
````````

### 6.6 Trap
````````

---

## 7) Native imports and link hints (implemented)

Top-level directives:

- `link "<module>"`, `import "<module>"` to use other RANE modules
- `#rane` hints (e.g. `#rane noopt`, `#rane entry`)
- `"symbol" = ?` to reserve symbols for external linking
- `{.attribute. : value}` to set platform-specific attributes

Semantics:
- The PE backend emits `.idata` entries for each imported symbol (grouped by DLL).
- Callsites are patched per symbol to the symbol’s IAT entry.
- The C backend emits:
  - `__declspec(dllimport)` for imported symbols (Windows/MSVC)
  - `#pragma comment(lib, "...")` for `link` entries under MSVC

---

## 8) A practical “hello + import” example
````````

---

## 9) Grammar (implemented, simplified)

RANE does not currently ship a formal grammar file; the parser is the authoritative grammar.
This section describes a *useful approximation* of the implemented syntax.
````````

---

## 10) Reserved tokens / planned features

The lexer recognizes many keywords beyond what is currently implemented in parsing/lowering:
- Examples: `constexpr`, `namespace`, `class`, `try/catch/throw`, etc.

These should be treated as **reserved** unless you can find corresponding parsing/lowering paths.
Authoritative list: `rane_lexer.cpp` → `identifier_type()`.

---

## 11) How to extend the language (for contributors)

Practical vertical slice:
1. Add/confirm token (lexer)
2. Parse into AST (`rane_parser.cpp`)
3. Add AST node (`rane_ast.h`)
4. Typecheck (`rane_typecheck.cpp`)
5. Lower into TIR (`rane_tir.cpp`)
6. Optimize / reason about side effects (`rane_optimize.cpp`)
7. Codegen + fixups (`rane_x64.cpp`, `rane_driver.cpp`)
8. Add a focused `.rane` test in `tests/`

---

## 12) Reference: Where each feature is implemented

- Parsing: `rane_parser.cpp`
- AST definitions: `rane_ast.h`
- Type checking: `rane_typecheck.cpp`
- Lowering: `rane_tir.cpp`
- Backends:
  - x64+PE: `rane_x64.cpp`, `rane_driver.cpp`
  - C backend: `rane_c_backend.cpp`
- Optimizations: `rane_optimize.cpp`
- Tests: `tests/`