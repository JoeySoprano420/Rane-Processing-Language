# **RANE Processing Language**
**Updated: 1/5/2026**

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
