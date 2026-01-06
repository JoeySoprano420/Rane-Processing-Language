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

*****

# **RANE Processing Language**
**Updated: 1/6/2026**

RANE is a deterministic, statically typed, low‑level systems language with explicit foreign boundaries and an emphasis on clarity and analyzability.

RANE is under active development. Some concepts referenced in older documents (e.g., advanced code features, full memory‑band semantics) are **planned**, not fully implemented yet.

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

Supported optimization flags:
- `-O0 -O1 -O2 -O3`

---

## 3) Program structure and surfaces

RANE currently has two overlapping “surfaces” that compile through the same AST/TIR pipeline.

### 3.1 Core/bootstrap surface
Used in `tests/exhaustive_exprs.rane` and many internal demos:
- local variable binding (`let`)
- expressions (arith/bitwise/compare/logical/ternary)
- procedures (`proc`)
- explicit memory helpers (`addr`, `load`, `store`
- MMIO helpers (`mmio region`, `read32`, `write32`)
- printing via `print(...)` (lowered to runtime import)
- `return`

### 3.2 v1 prose/node surface
Used in `tests/hello_v1_nodes.rane` and `tests/v1_struct_basic.rane`:
- `module <name>`
- `node <name>:` ... `end`
- `say <expr>`
- `halt`
- `start at node <name>`
- (optional) `go to node <name>`
- v1 data model: `struct ... end`, `set`, `add`

Minimal v1 node program:

````````
module tiny;

say "hello, world!";

set a: i32 = 1;
set b: i32 = 2;

say "sum is " + (a + b);

start at node main;

node main:
  say "in main";
  halt;
end.
---

## 4) Lexical structure (implemented)

### 4.1 Whitespace
Whitespace is ignored (spaces, tabs, newlines).

### 4.2 Comments
- line comments: `// ...`
- block comments: `/* ... */`

### 4.3 Identifiers
- `[A-Za-z_][A-Za-z0-9_]*`

### 4.4 Integer literals
Supported:
- decimal: `42`, `1_000_000`
- hexadecimal: `0xCAFE_BABE`
- binary: `0b1010_0101`

Underscores are permitted and ignored during parsing.

### 4.5 Text literals
- `"..."` double-quoted text literal tokens
- basic backslash escapes are lexed (bootstrap escaping behavior is intentionally minimal)

### 4.6 Boolean literals
- `true`, `false`

### 4.7 Punctuation and operators
Implemented tokens include:
- punctuation: `; , . ( ) [ ] { } : ?`
- operators:
  - arithmetic: `+ - * / %`
  - bitwise: `& | ^ ~`
  - comparisons: `< <= > >= == !=`
  - assignment: `=`
  - logical: `&& || !`
  - node/control punctuation: `->`, `<-`, `=>`, `::`
  - identifier-literal prefix: `#`

---

## 5) Types and the bootstrap type system (implemented)

### 5.1 Internal type universe (bootstrap)
Primitive/internal types are defined in `rane_ast.h`:

- Unsigned integers: `u8 u16 u32 u64`
- Signed integers: `i8 i16 i32 i64`
- Pointer-sized value: `p64`
- Boolean: `b1`
- Textual blobs for v1 output: `text`, `bytes`
- Legacy: `string` (bootstrap legacy)

### 5.2 Type inference (bootstrap)
- Most variables are introduced with `let x = expr;` and inferred from `expr`.
- Numeric binary ops unify to `u64` in the bootstrap typechecker.
- Comparisons produce `b1`.
- `and` / `or` require `b1` operands (enforced in `rane_typecheck.cpp`).
- `null` is represented as `p64(0)` in bootstrap.

### 5.3 Named types (v1)
`struct` introduces named types (kept as names and resolved later in type/lowering).

---

## 6) Expressions (implemented)

### 6.1 Literals
- integer: `123`, `0x10`, `0b1010`
- boolean: `true`, `false`
- text: `"hello"`
- null: `null`
- identifier literal: `#rane_rt_print`

### 6.2 Variables
- `x` resolves through lexical scope rules.

### 6.3 Unary operators
- `-x`
- `not x` or `!x`
- `~x`

### 6.4 Binary operators (precedence-aware)
Arithmetic:
- `+ - * / %`

Bitwise:
- `& | ^`
- word-form: `xor`
- shifts: `shl`, `shr`, `sar` (tokenized keywords used as operators)

Comparisons:
- `< <= > >= == !=`

Logical:
- word-form `and`, `or`
- tokenized forms `&&`, `||`

v1 compatibility:
- `=` is treated as equality in expression parsing (same as `==`) for compatibility.

### 6.5 Ternary
- `cond ? thenExpr : elseExpr`

### 6.6 Postfix expressions
- call: `f(a, b)`
- member: `a.b`
- index: `a[i]`
- call-on-expression: `(calleeExpr)(1, 2)`

---

## 7) Built-in memory operations (implemented)

These are built-in expression forms, parsed and lowered directly.

### 7.1 `addr(base, index, scale, disp)`
Computes an address-like value (returns `p64` in typecheck).

Example:

````````

### 7.2 `load(type, address)`
Loads memory at `address` and yields `type`.

Example:

````````

Typechecking ensures `address` is pointer-like (`p64` or `u64`).

### 7.3 `store(type, address, value)`
Stores `value` to memory at `address` and returns the stored value.

Example:

````````

Typechecking ensures:
- `address` is pointer-like
- `value` matches the declared `type`

---

## 8) Control flow and statements (implemented subset)

The AST includes many statements; the confirmed implemented/tested subset includes:

### 8.1 Blocks
- `{ ... }` blocks exist and are typechecked.

### 8.2 `let`
- `let name = expr;`

### 8.3 Assignment
- `name = expr;`
- `_ = expr;` is used as an “expression statement” in some paths (discard result).

### 8.4 `if` / `while`
- exist in AST and are typechecked
- bootstrap rule: condition must be `b1` or `u64`

### 8.5 `proc` and `return`
Example (from tests):

````````
proc add(x, y) {
  return x + y;
}

let sum = add(21, 21);
```

Bootstrap typing for procedures:
- params are treated as `u64`
- return is treated as `u64` (bootstrap simplification)

### 8.6 `break` / `continue`
Tokens and AST exist; treat as implemented if present in parse paths you are using.

---

## 9) v1 data model: `struct`, `set`, `add` (implemented)

This is demonstrated by `tests/v1_struct_basic.rane`.

### 9.1 `struct` declaration
Example:

````````
struct Point {
  x: i32,
  y: i32,
}
```

### 9.2 Struct literals
Named-field form:
```rane
let p = Point { x: 10, y: 20 };
mem copy dst, src, size;
```

Position-independent bitcast form:
```rane
set p: Point = Point { x: 10, y: 20 };
```

### 9.3 `set` statement (two forms)
Declaration + initialization:
````````
set p: Point = Point { x: 1, y: 2 };
```

Separate declaration and initialization:

````````
set p: Point;
p = Point { x: 1, y: 2 };

````````

This is the code block that represents the suggested code change:

````````markdown
set m: u32 to h.magic
```

### 9.4 `add` statement
Numeric update form:
````````
set m: u32 to h.magic;
```

Pointer update form:
```rane
set ptr: *i32 = addr(base, index, scale, disp);
```

---
---

## 10) v1 prose/node surface (implemented)

### 10.1 `module`

````````

This is the code block that represents the suggested code change:

````````markdown

### 10.2 `node` blocks

````````

This is the code block that represents the suggested code change:

````````markdown

### 10.3 `start at node`

````````

This is the code block that represents the suggested code change:

````````markdown

### 10.4 `go to node`
Tokenized and AST exists; use in v1 control-flow programs where wired.

### 10.5 `say`
`say expr` prints `text` or `bytes` (typechecked).

### 10.6 `halt`
Terminates execution in v1 programs.

---

## 11) MMIO helpers (implemented)

Example (from tests):

````````

This is the code block that represents the suggested code change:

````````markdown

### Semantics (bootstrap)
- `mmio region <NAME> from <BASE> size <SIZE>` defines a region in compiler module state.
- `read32 NAME, offsetExpr into varName` lowers into a load from that region.
- `write32 NAME, offsetExpr, valueExpr` lowers into a store.

---

## 12) Bulk memory helpers (implemented subset)

### 12.1 `mem copy`
Statement:

````````

This is the code block that represents the suggested code change:

````````markdown

Other tokens exist (`mem zero`, `mem fill`) but should be treated as reserved unless verified by tests/lowering in your active grammar path.

---

## 13) Imports / exports and the foreign boundary (bootstrap)

### 13.1 Import declarations
Imports lower into TIR import metadata.

Example:

````````

This is the code block that represents the suggested code change:

````````markdown

### 13.2 Export declarations
Exports lower into TIR export metadata.

Example:

````````

This is the code block that represents the suggested code change:

````````markdown

> Note
>
> The bootstrap PE emitter currently has a minimal fixed `.idata` implementation that is primarily used for printing (`msvcrt.dll!printf`), and the call-fixup mechanism patches callsites accordingly.

---

## 14) Output and printing (bootstrap)

### 14.1 `say` (v1)
- `say "Hello"` prints a `text` value.
- Typechecking enforces `text|bytes`.

### 14.2 `print(...)` (core)
The core surface uses `print(s)` as in `tests/exhaustive_exprs.rane`.
In bootstrap, this is lowered to an import call targeting `rane_rt_print`.

### 14.3 `rane_rt_print` mapping (`.exe` emission)
The PE writer imports **`msvcrt.dll!printf`** and patches callsites named:
- `printf`
- `rane_rt_print`

to the same IAT entry.

This is a bootstrap compatibility strategy: codegen calls `rane_rt_print`, but the underlying import is `printf`.

---

## 15) Determinism and policy gating (implemented)

### 15.1 `sys.alloc` as a capability gate
The typechecker scans the AST and enables heap permission only if:

````````

This is the code block that represents the suggested code change:

````````markdown

This capability is used to reject certain operations when heap is forbidden (bootstrap determinism policy).

---

## 16) Compiler pipeline (implemented)

For `.exe` emission (`rane_driver.cpp`):

1. Read file
2. Parse (`rane_parse_source_len_ex`) producing AST (`rane_ast.h`)
3. Typecheck (`rane_typecheck_ast_ex`)
4. Lower AST → TIR (`rane_lower_ast_to_tir`)
5. SSA build (`rane_build_ssa`)
6. Register allocation (`rane_allocate_registers`)
7. Optimize (`rane_optimize_tir_with_level`)
8. AOT codegen (`rane_aot_compile_with_fixups` → machine code bytes)
9. Build PE: `.text`, `.rdata`, `.idata`
10. Apply import fixups and write `output.exe`

---

## 17) `.rdata` emission via `TIR_DATA_*` (bootstrap)

TIR supports data directives (`rane_tir.h`):
- `TIR_DATA_BEGIN`
- `TIR_DATA_ZSTR`
- `TIR_DATA_BYTES`
- `TIR_DATA_END`
- and address materialization via `TIR_ADDR_OF` (RIP-relative `lea` in x64)

Bootstrap strategy:
- literals are pooled into `.rdata`
- code uses RIP-relative addressing for labels
- the PE emitter places `.rdata` at a fixed RVA and patches what must be patched

This replaces earlier “patch raw heap pointer immediates” approaches.

---

## 18) C backend (`-emit-c`) (bootstrap portability)

The C backend (`rane_c_backend.*`) emits:
- a single C translation unit that emulates the TIR execution model
- register file `r[256]` and stack slots `s[stack_slot_count]`
- enough coverage for simple integer programs and a subset of calls/branches

Use:

````````

This is the code block that represents the suggested code change:

````````markdown

---

## 19) Tests (repo truth)

`.rane` fixtures under `tests/` are the most reliable “what works” documentation:

- `tests/hello_v1_nodes.rane`  
  v1 node/prose surface: `module`, `node`, `say`, `halt`, `start at node`.
- `tests/exhaustive_exprs.rane`  
  broad expression coverage + arithmetic/bitwise/compare/logical/ternary + memory builtins + MMIO.
- `tests/v1_struct_basic.rane`  
  v1 structs + `set` + `add` + member operations + `say` + `halt`.

Guideline (repo standard):
- For each new language feature, add at least one focused `.rane` test under `tests/`.

---

## 20) Reserved/planned keywords and features

The lexer tokenizes many additional keywords (examples):
- `try`, `catch`, `throw`
- `namespace`, `class`, visibility keywords
- `constexpr`, `consteval`, etc.
- many “semantic placeholders” like `permit`, `require`, `taint`, `sanitize`

Unless you can trace a feature through:
**parse → AST → typecheck → lowering → backend**, it should be treated as **reserved**.

Authoritative list: `rane_lexer.cpp` (`identifier_type()`).

---

## 21) Repo map (where things live)

### Language implementation
- Lexer: `rane_lexer.h`, `rane_lexer.cpp`
- Parser: `rane_parser.h`, `rane_parser.cpp`
- AST: `rane_ast.h`
- Diagnostics: `rane_diag.h`
- Typecheck: `rane_typecheck.h`, `rane_typecheck.cpp`
- TIR: `rane_tir.h`, `rane_tir.cpp`
- SSA/regalloc: `rane_ssa.*`, `rane_regalloc.*`
- Optimizations: `rane_optimize.*`

### Backends
- x64 emitter/codegen: `rane_x64.h`, `rane_x64.cpp`
- AOT wrapper: `rane_aot.h`, `rane_aot.cpp`
- PE writer / EXE emission: `rane_driver.cpp`
- C backend: `rane_c_backend.h`, `rane_c_backend.cpp`

### Runtime/infrastructure (present in repo)
The repo contains additional runtime-style components (GC, loader, VM, file/net/crypto/thread/security/perf). They are part of the broader project direction and internal demos; they are not necessarily exposed as first-class language features yet.
- GC: `rane_gc.*`
- Loader/policy scaffolding: `rane_loader.*`, `rane_security.*`
- VM scaffolding: `rane_vm.*`
- Utilities: `rane_hashmap.*`, `rane_bst.*`, `rane_heap.*`
- System helpers: `rane_file.*`, `rane_thread.*`, `rane_net.*`, `rane_crypto.*`
- Perf helpers: `rane_perf.*`

---

## Roadmap (high level)

Near-term practical work visible in the codebase direction:
- multiple-import `.idata` support
- per-symbol import patching and stable FFI declarations
- extend struct typing/layout beyond bootstrap placeholders
- broaden C backend coverage to more TIR ops
- stronger SSA optimizations and call reasoning

Long-term direction (planned):
- explicit rich type system (records/enums/tagged unions)
- capability/duration/privilege semantics
- full memory-band integration (CORE/AOT/JIT/META/HEAP/MMAP)
- deterministic multi-stage programming

---
