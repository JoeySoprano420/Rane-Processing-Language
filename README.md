# **RANE Processing Language**
**Updated: 1/5/2026**

RANE is a deterministic, statically typed, low‑level systems language with compile‑time execution, explicit foreign boundaries, and a ritualized emphasis on clarity and analyzability.

RANE is an experimental programming language and native‑code toolchain written in C++14.  
The current repository contains a **bootstrap compiler** targeting **Windows x64**, producing a minimal **PE executable**. The focus of this stage is correctness, determinism, and a clean end‑to‑end pipeline.

RANE is under active development. Some concepts referenced in older documents (e.g., advanced type features, full memory‑band semantics) are **planned**, not fully implemented yet.

---

# **Project Goals (Bootstrap Phase)**

The current compiler emphasizes:

- A **minimal surface language**  
  (`let`, expressions, `if`/`while`, `proc`, `return`, builtin `print`)
- A **Typed Intermediate Representation (TIR)**
- A small, direct **x64 backend**
- A prototype **memory‑band loader**  
  (CORE / AOT / JIT / META / HEAP / MMAP) used by the in‑process demo

This stage is about building a solid, deterministic foundation for future expansion.

---

# **Language Overview (What Works Today)**

Everything in this section is fully implemented and compiles end‑to‑end  
**(parse → typecheck → TIR → x64 → PE)**.

## **Tokens**
- Identifiers: `foo`, `_tmp`, `memcpy`
- Integers: `42`, `1_000_000`, `0xCAFE_BABE`, `0b1010_0101`
- Strings: `"hello"`
- Booleans: `true`, `false`
- Operators:  
  arithmetic `+ - * / %`  
  bitwise `& | ^ ~`  
  comparisons `< <= > >= == !=`  
  logical `and or` (also tokenizes `&&` / `||`)  
  ternary `? :`
- Punctuation: `;`, `{}`, `()`, `,`, `[ ]`, `.`

---

# **Types (Bootstrap)**
Types are currently inferred. Internally, the compiler uses:

- `u64` — default integer type  
- `b1` — boolean  
- `p64` — pointer‑sized integer  
- `text`, `bytes` — used by builtin operations like `print`

---

# **Expressions**
All of the following are fully supported:

- Literals: `42`, `"hello"`, `true`
- Variables: `x`
- Arithmetic & bitwise expressions
- Comparisons (returning 0/1)
- Logical short‑circuiting
- Ternary expressions
- Calls: `foo(1, 2)`

---

# **Memory Builtins**
These are built into the parser and lowered directly into TIR:

```rane
addr(base, index, scale, disp)
load(u64, addr(...))
store(u64, addr(...), value)
```

These allow explicit, low‑level memory access in the bootstrap compiler.

---

# **Statements**
Supported statements include:

- `let x = expr;`
- `x = expr;`
- Blocks `{ ... }`
- `if cond then stmt [else stmt]`
- `while cond do stmt`
- `return;` / `return expr;`
- Low‑level control flow:  
  `label;`  
  `jump label;`  
  `goto cond -> true_label, false_label;`

---

# **MMIO Helpers**
These are parsed and lowered:

```rane
mmio region UART from 0x1000 size 0x100;
read32 UART, 0x0 into v;
write32 UART, 0x0, 0x1;
```

---

# **Memcpy Helper**
```rane
mem copy dst, src, size;
```

---

# **Zone Blocks**
Parsed and treated as normal blocks in the bootstrap:

```rane
zone hot {
  print("fast path");
}
```

---

# **Imports / Exports**
These lower into TIR declarations:

```rane
import sys.alloc;
export my_symbol;
```

---

# **Compiler Pipeline**
The following components are implemented and working:

- Lexer / parser / typechecker
- AST → TIR lowering
- TIR → x64 machine code generation
- PE writer emitting `.text`, `.rdata`, `.idata`
- Import fixups (msvcrt.dll!printf)
- Branch fixups (rel32)
- Small optimizations:
  - peephole MOV folding
  - basic dead‑code elimination

---

# **Building**
RANE builds with **Visual Studio (C++14)**.

1. Open `Rane Processing Language.vcxproj`
2. Build **Release | x64**

---

## 6) Build the compiler (Visual Studio 2026)

1. Open the solution/project in Visual Studio.
2. Select configuration **Release | x64** (or **Debug | x64**).
3. Build using __Build Solution__.

---

# **Usage**
Compile a `.rane` file into a Windows executable:

```
Rane Processing Language.exe input.rane -o output.exe -O2
```

Supported optimization flags: `-O0 -O1 -O2 -O3`.

---

# **Current Status**
RANE is a functioning bootstrap compiler with:

- A minimal but real language
- Deterministic lowering and fixups
- A typed IR
- A working native backend
- A prototype memory‑band loader

Many advanced features described in older documents are **planned**, not yet implemented.

---

Here’s a clean, polished **Vision** section you can paste directly into your README.  
It’s written to match the tone and structure of the rest of your document, and it reflects the direction RANE is clearly growing toward.

You can drop this right after **Current Status**.

---

# **Vision: The Future of RANE**

RANE’s long‑term direction extends far beyond the current bootstrap compiler.  
The project aims to evolve into a **deterministic, capability‑oriented, multi‑stage systems language** with a focus on clarity, analyzability, and high‑performance native execution.

Future development is centered around the following pillars:

## **1. A Rich, Explicit Type System**
RANE will introduce a full suite of type‑level constructs designed for determinism and analyzability:

- **Type annotations** for all declarations  
- **User‑defined types** (records, enums, tagged unions)  
- **Capsules** as explicit, analyzable units of state + behavior  
- **Containers** with predictable memory layout  
- **Qualifiers** for purity, determinism, and side‑effect control  
- **Specify clauses** for constraints and type‑level contracts  

The goal is a type system that remains simple, explicit, and zero‑cost at runtime.

## **2. Durations, Privileges, and Capability‑Oriented Semantics**
RANE will adopt a capability‑driven execution model:

- **Durations** define where and how long values live  
- **Privileges** define what code is allowed to do  
- **Capability scopes** ensure safety without hidden checks or runtime overhead  

These features form a static, analyzable capability lattice that replaces implicit lifetimes, hidden conversions, and ad‑hoc access rules found in other systems languages.

## **3. Memory‑Band Architecture**
The prototype loader will evolve into a full **multi‑band execution model**:

- **CORE** — immutable, trusted code  
- **AOT** — ahead‑of‑time compiled modules  
- **JIT** — deterministic runtime specialization  
- **META** — compile‑time execution and reflection  
- **HEAP** — dynamic allocations  
- **MMAP** — mapped regions and device memory  

Bands provide structure for optimization, safety, and predictable performance.

## **4. High‑Performance Native Execution**
RANE aims to reach performance comparable to C, Zig, and Rust through:

- SSA‑based optimization  
- Inlining, LTO, and PGO  
- Auto‑vectorization and SIMD lowering  
- Loop optimizations and unrolling  
- A full register allocator  
- Deterministic JIT specialization for hot paths  

The long‑term goal is **native‑class throughput with deterministic behavior**.

## **5. Self‑Hosting and Toolchain Maturity**
RANE will eventually compile itself, enabling:

- Faster iteration  
- A cleaner, more expressive compiler codebase  
- A stable foundation for future language evolution  

A module system, improved diagnostics, and a standard library will support real‑world development.

## **6. Deterministic Multi‑Stage Programming**
RANE’s META and JIT bands will support:

- Compile‑time code generation  
- Runtime specialization  
- Safe, deterministic multi‑stage execution  
- Domain‑specific languages  
- High‑performance pipelines and simulation engines  

This enables patterns that are difficult or impossible in traditional AOT‑only languages.

---

## Onboarding

See `onboarding.md` for a detailed onboarding guide (build prerequisites, compiler pipeline, language syntax and examples, imports/link directives, testing workflow, and roadmap milestones).

---

## 13) Performance (current and trajectory)

### Current performance characteristics
- The compiler is a bootstrap toolchain focused on correctness and deterministic codegen.
- Generated code is native x64 and runs at machine speed for the subset used.
- Optimization passes exist but are intentionally small/limited in bootstrap:
  - peephole MOV folding
  - basic DCE
  - (where implemented) constant folding / constexpr hooks

### Expected future performance direction
Performance will improve primarily via:
- stronger SSA-based optimizations
- better inlining/call reasoning
- improved register allocation
- more complete instruction selection and lowering
- deterministic JIT specialization (roadmap)

---

## 14) Milestones / trajectory (roadmap)

This is a practical milestone plan aligned with the existing repo direction.

### Milestone 0 — Bootstrap compiler (current)
- Working lexer/parser/typecheck/TIR/codegen/PE path
- `.text/.rdata/.idata` emission
- basic optimizations
- basic tests in `tests/`

### Milestone 1 — Imports, link hints, and stable FFI
- Multiple-import `.idata` support
- Per-symbol call fixups
- C backend emits dllimport + pragma comment(lib)

### Milestone 2 — Language usability expansion
- More consistent statement grammar across “core” and “v1 node” surfaces
- Better diagnostics
- More predictable syntax for functions/procs and returns

### Milestone 3 — Stronger type system groundwork
- Explicit type annotations in source
- User-defined types with deterministic layout
- More robust type checking

### Milestone 4 — Multi-stage + memory-band integration
- Expand CORE/AOT/JIT/META/HEAP/MMAP semantics
- Raise determinism guarantees and enforcement tools

---

## 15) Learning prerequisites and learning curve

### Prerequisites
- Comfortable reading C/C++ (for understanding the compiler implementation)
- Basic familiarity with compilers/IR is helpful but not required
- Basic Windows/native concepts help (PE, DLL imports)

### Learning curve guidance
Recommended path:
1. Start with the v1 node surface examples in `tests/`
2. Move to expressions and arithmetic
3. Learn the `mmio` and memory helper surface (if needed)
4. Finally learn native imports (`import … from "…"`) and linking hints (`link "…"`)

---

## 16) What can RANE make today?

### Today
- Small Windows x64 executables (single binary output)
- Programs that:
  - print simple strings (`say`)
  - execute arithmetic and control flow
  - perform limited memory operations via builtins
  - call imported symbols from DLLs (bootstrap FFI path)

### Later
- richer systems programs with explicit types, safe/capability constrained operations,
  and multi-stage compilation (AOT/JIT/META) with deterministic specialization.

---

## 17) Tests

- `.rane` fixtures live under `tests/`
- C++ unit tests exist (example: `rane_gc_tests.cpp`)

Guideline:
- For each new language feature, add at least one focused `.rane` test under `tests/`.

---

## 18) Quick troubleshooting

### Parse errors
- Check `rane_parser.cpp` `parse_stmt()` for what is currently accepted.
- Use the error spans printed by the driver (`rane_driver.cpp`) for line/column.

### Link/import issues
- On the AOT/PE path, imports are from DLL+symbol only; `.lib` is not used.
- On the C backend path, `link "foo.lib";` emits MSVC `#pragma comment(lib, ...)`.

---

### What these mean
- `import <sym> from "<dll>";`
  - Adds an import entry for `<dll>!<sym>` in the emitted PE `.idata`
  - Call sites to `<sym>` are patched to use the corresponding IAT slot
- `link "<lib>";`
  - The C backend will emit `#pragma comment(lib, "<lib>")` when compiling under MSVC
  - The AOT/PE path does not directly use `.lib` hints (PE import is driven by DLL + symbol)

### When to use them
- Use `import` when you want to call an external function by name from a DLL at runtime.
- Use `link` only when using the **C backend** and compiling the emitted C with MSVC.

---

## 11) Keywords and tokens: source of truth

### Current implemented semantics
Not every token in the lexer has meaning (many are reserved for future).

- For what is *tokenized*, see:
  - `rane_lexer.cpp` → `identifier_type()`
- For what is *parsed into AST*, see:
  - `rane_parser.cpp` → `parse_stmt()` and expression parsing
- For what is *lowered into TIR*, see:
  - `rane_tir.cpp`

Recommended workflow:
- Treat the lexer keyword table as **reserved words**.
- Treat `rane_parser.cpp` as the authoritative “what the language accepts today”。

---

## 12) Compiler pipeline (how it works)

---

# **Native imports and MSVC link hints (CURRENT)**

RANE supports native imports and link hints as top-level directives:

````````markdown
import <sym> from "<dll>";
say "hello, world";
module my_module
node main:
  say "Entering main node"
  halt
end
````````

Semantics:
- `module <name>` declares module context (bootstrap metadata)
- `node <name>:` begins a node body, terminated by `end`
- `say <expr>` prints a string/text in the bootstrap model
- `halt` terminates execution
- `start at node <name>` defines the entry node

> Note: In this bootstrap, `say` currently prints via a runtime path wired into the backend(s). The exact imported function used depends on the current driver/backend configuration.

---

**********

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
