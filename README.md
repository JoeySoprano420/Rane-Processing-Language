# **RANE Processing Language**
**Updated: 1/6/2026**

RANE is a deterministic, statically typed, low‑level systems language with explicit foreign boundaries and an emphasis on clarity and analyzability.

RANE is under active development. Some concepts referenced in older documents (e.g., advanced type features, full memory‑band semantics) are **planned**, not fully implemented yet.

---

# **Project Goals (Bootstrap Phase)**

The current compiler emphasizes:

- A minimal surface language with deterministic semantics
- A **Typed Intermediate Representation (TIR)**
- A small, direct, bootstrap‑friendly **Windows x64** backend
- A minimal PE writer emitting `.text`, `.rdata`, `.idata`
- Policy-gated capabilities (e.g., heap allocation via `import sys.alloc`)
- Strong, early diagnostics (parse/typecheck/lower all produce `rane_diag_t`)

This stage is about building a solid end‑to‑end pipeline where each step is easy to reason about and debug.

---

# **Language Overview (What Works Today)**

Everything in this section compiles end‑to‑end:

**parse → typecheck → lower (TIR) → x64 → PE(EXE)**

## **Tokens**
- Identifiers: `foo`, `_tmp`, `memcpy`
- Integers: `42`, `1_000_000`, `0xCAFE_BABE`, `0b1010_0101`
- Text literals: `"hello"`
- Booleans: `true`, `false`
- Operators:
  - arithmetic `+ - * / %`
  - bitwise `& | ^ ~`
  - comparisons `< <= > >= == !=`
  - logical `and or` (also tokenizes `&&` / `||`)
  - ternary `? :`
- Punctuation: `;`, `{}`, `()`, `,`, `[ ]`, `.`

---

# **Types (Bootstrap)**

Types are currently inferred. Internally, the compiler uses:

- `u64` — default integer type
- `b1` — boolean
- `p64` — pointer-sized integer (opaque pointer in bootstrap)
- `text`, `bytes` — used for “string-like” and raw byte sequences

---

# **Expressions**

Supported expressions:

- Literals: `42`, `"hello"`, `true`
- Variables: `x`
- Arithmetic & bitwise expressions
- Comparisons (returning 0/1)
- Logical short‑circuiting (`and`, `or`)
- Ternary expressions
- Calls: `foo(1, 2)`

---

# **Memory Builtins**

These are built into the parser and lower directly into TIR:

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
  - `label;`
  - `jump label;`
  - `goto cond -> true_label, false_label;`

---

# **User-Defined Structs (v1)**

RANE supports simple struct declarations and literals:

```rane
struct Point {
  x: i32,
  y: i32,
}

let p = Point { x: 10, y: 20 };
mem copy dst, src, size;
```

### Allocation strategy (bootstrap)
Struct literals are lowered in a deterministic order:

1. **Stack allocation** (preferred bootstrap):
   - `set h: Header to Header{ ... }` allocates `Header` bytes in the function stack frame and stores `&stack_slot` into `h`.
2. **Heap allocation** (policy gated):
   - `Header{ ... }` in expression position allocates with `rane_alloc(size)` **only if** `import sys.alloc;` is present.
   - If `sys.alloc` is not imported, lowering hard-fails with a diagnostic.
3. **Global/static blobs**:
   - `text` / `bytes` literals are emitted into `.rdata` as real data (see `.rdata` emission below).

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

Parsed and treated as normal blocks in the bootstrap compiler:

```rane
zone hot {
  print("fast path");
}
```

---

# **Imports / Exports**

Imports / exports lower into TIR declarations:

```rane
import sys.alloc;
export my_symbol;
```

---

# **Diagnostics**

Parse, typecheck, and lowering all use `rane_diag_t`:

- filename-less, span-based error reporting (`line`, `col`, `length`)
- deterministic messages
- “hard error everywhere” in lowering (no silent “treat as 0” fallbacks)

---

# **Compiler Pipeline**

Implemented components:

- Lexer / parser / typechecker
- AST → TIR lowering
- TIR → x64 machine code generation
- PE writer emitting `.text`, `.rdata`, `.idata`
- Import fixups (bootstrap: `msvcrt.dll!printf` backing `rane_rt_print`)
- Branch fixups (rel32)
- Small bootstrap optimizations:
  - peephole MOV folding
  - basic dead‑code elimination

---

# **`.rdata` Emission (Bootstrap)**

The compiler emits real `.rdata` content from TIR data directives:

- `TIR_DATA_BEGIN <label>`
- `TIR_DATA_ZSTR <ptr>`
- `TIR_DATA_BYTES <ptr,len>`
- `TIR_DATA_END`

The `.exe` writer builds a `.rdata` blob from these directives and patches `TIR_ADDR_OF` callsites (which are emitted as RIP‑relative `lea`) to point at final `.rdata` virtual addresses.

This replaces the earlier bootstrap approach of patching raw `mov reg, imm64` heap pointers.

---

# **Building (Visual Studio 2026)**

RANE builds with C++ (bootstrap style, VS toolchain).

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

# **Tests**

- `.rane` fixtures live under `tests/`
- C++ unit tests exist (example: `rane_gc_tests.cpp`)

Guideline:
- For each new language feature, add at least one focused `.rane` test under `tests/`.

---

# **Current Status**

RANE is a functioning bootstrap compiler with:

- A minimal but real language subset
- Deterministic lowering and fixups
- A typed IR
- A working native backend
- A prototype memory‑band loader
- Policy‑gated heap usage
- Span-aware, hard-failing lowering diagnostics

Many advanced features described in older documents are **planned**, not yet implemented.

---

# **Vision: The Future of RANE**

RANE’s long‑term direction extends beyond the current bootstrap compiler.
The project aims to evolve into a deterministic, capability‑oriented, multi‑stage systems language with a focus on analyzability and high‑performance native execution.

## **1. A Rich, Explicit Type System**
Planned expansion includes:

- Explicit type annotations for all declarations
- User-defined types (records, enums, tagged unions)
- More robust type checking and layout computation
- Contracts and constraints (“specify” clauses)

## **2. Durations, Privileges, and Capability‑Oriented Semantics**
RANE’s capability model is intended to formalize:

- where values live (duration)
- what code is allowed to do (privileges)
- enforcement without hidden runtime checks

## **3. Memory‑Band Architecture**
The prototype loader is intended to evolve into a full multi‑band execution model:

- CORE — immutable, trusted code
- AOT — ahead‑of‑time compiled modules
- JIT — deterministic runtime specialization
- META — compile‑time execution and reflection
- HEAP — dynamic allocations
- MMAP — mapped regions and device memory

## **4. High‑Performance Native Execution**
Planned improvements include:

- stronger SSA-based optimizations
- better inlining/call reasoning
- improved register allocation
- more complete instruction selection and lowering
- deterministic JIT specialization (roadmap)

## **5. Self‑Hosting and Toolchain Maturity**
Long-term goals include:

- self-hosting
- module system expansion
- higher quality diagnostics
- a standard library suitable for real programs

---

## Onboarding

See `onboarding.md` for a detailed onboarding guide (build prerequisites, compiler pipeline, language syntax and examples, imports/link directives, testing workflow, and roadmap milestones).
