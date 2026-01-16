# 🌐 THE RANE LANGUAGE — COMPLETE SYSTEM OVERVIEW

**Reliable Adaptive Natural Efficient (RANE Processing Language)**
**P.I.E. = Processing Ideas → Instructions → Execution**

RANE is designed as a *systems whisperer*: we write clear, human-shaped instructional prose, and the toolchain deterministically turns it into machine-operable structure and then into real Windows x86-64 PE executable code.

RANE’s identity is not “a syntax that compiles.” It’s an **execution civilization**: language + staged transformations + capability security + deterministic concurrency + inspectable IR + an emitter that can produce `.exe` output.


## Table of Contents

- [0) The spine: what RANE *is* in one sentence](#0-the-spine-what-rane-is-in-one-sentence)
- [1) P.I.E. — the mission statement (Processing Ideas → Instructions → Execution)](#1-pie-the-mission-statement-processing-ideas-instructions-execution)
- [2) The pipeline (the full “spine” with what each stage *means*)](#2-the-pipeline-the-full-spine-with-what-each-stage-means)
- [3) CIAMs (Contextual Inference Abstraction Macros) — the “everywhere” mechanism](#3-ciams-contextual-inference-abstraction-macros-the-everywhere-mechanism)
- [4) RANE’s philosophy statements — explained line by line (no handwaving)](#4-ranes-philosophy-statements-explained-line-by-line-no-handwaving)
- [5) Core language surface (what our syntax demonstrates)](#5-core-language-surface-what-our-syntax-demonstrates)
- [6) Modules, imports, namespaces (the program’s global topology)](#6-modules-imports-namespaces-the-programs-global-topology)
- [7) Procedures (procs), visibility, qualifiers](#7-procedures-procs-visibility-qualifiers)
- [8) Types: primitives, aliases, user-defined aggregates](#8-types-primitives-aliases-user-defined-aggregates)
- [9) Constants: `const`, `constexpr`, `constinit`, `consteval`](#9-constants-const-constexpr-constinit-consteval)
- [10) Structs, enums, variants, unions — data modeling explained fully](#10-structs-enums-variants-unions-data-modeling-explained-fully)
- [11) Low-level hardware features: `mmio region`, `addr`, `load`, `store`](#11-low-level-hardware-features-mmio-region-addr-load-store)
- [12) Capabilities: the compile-time security model](#12-capabilities-the-compile-time-security-model)
- [13) Contracts and assertions: correctness rules as part of code](#13-contracts-and-assertions-correctness-rules-as-part-of-code)
- [14) Macros and templates: two different powers](#14-macros-and-templates-two-different-powers)
- [15) Concurrency primitives: async/await, threads, mutex, channels](#15-concurrency-primitives-asyncawait-threads-mutex-channels)
- [16) Resource management: `with`, `defer`, ownership ops](#16-resource-management-with-defer-ownership-ops)
- [17) Control flow: if/else, loops, match/switch/decide, goto/label](#17-control-flow-ifelse-loops-matchswitchdecide-gotolabel)
- [18) Inline `asm:` blocks](#18-inline-asm-blocks)
- [19) Exceptions: `try/catch/finally`, `throw`](#19-exceptions-trycatchfinally-throw)
- [20) `eval` and dynamic features](#20-eval-and-dynamic-features)
- [21) Operators: our full operator set, with rules](#21-operators-our-full-operator-set-with-rules)
- [22) Symbol literals: `#rane_rt_print`, `#REG`](#22-symbol-literals-rane-rt-print-reg)
- [23) Node-graph programming: `module demo_struct`, `node start: ...`](#23-node-graph-programming-module-demo-struct-node-start)
- [24) Typed CIL — what it is, why it exists, what it must guarantee](#24-typed-cil-what-it-is-why-it-exists-what-it-must-guarantee)
- [25) OSW — Optimized Structure Web (why it’s a “web” not a “list”)](#25-osw-optimized-structure-web-why-its-a-web-not-a-list)
- [26) Frame Planner — the ABI truth layer (Windows x64 specifics)](#26-frame-planner-the-abi-truth-layer-windows-x64-specifics)
- [27) Codegen: from Typed CIL/OSW into x64 machine code / PE](#27-codegen-from-typed-cilosw-into-x64-machine-code-pe)
- [28) Tooling and “built for creation”](#28-tooling-and-built-for-creation)
- [29) “Optimizations baked into how code is written”](#29-optimizations-baked-into-how-code-is-written)
- [30) “Polynomial-fibonacci ciphers replace obfuscation” (explained safely)](#30-polynomial-fibonacci-ciphers-replace-obfuscation-explained-safely)
- [31) Putting it all together: what our provided program demonstrates](#31-putting-it-all-together-what-our-provided-program-demonstrates)
- [32) The “civilization” view: what RANE ultimately becomes](#32-the-civilization-view-what-rane-ultimately-becomes)
- [33) The complete mental model in one final “execution story”](#33-the-complete-mental-model-in-one-final-execution-story)
- [1.1 Tokens](#11-tokens)
- [1.2 Indentation & Block Structure](#12-indentation-block-structure)
- [Diagnostics](#diagnostics)
- [2.1 `import`](#21-import)
- [2.2 `module`](#22-module)
- [2.3 `namespace`](#23-namespace)
- [3.1 Visibility keywords: `public`, `protected`, `private`, `admin`](#31-visibility-keywords-public-protected-private-admin)
- [3.2 Qualifiers: `inline`, `async`, `dedicate`, `linear`, `nonlinear`](#32-qualifiers-inline-async-dedicate-linear-nonlinear)
- [4.1 `type` (primitive or prelude registration)](#41-type-primitive-or-prelude-registration)
- [4.2 `typealias` and `alias`](#42-typealias-and-alias)
- [5.1 `const`, `constexpr`, `constinit`](#51-const-constexpr-constinit)
- [5.2 `consteval proc`](#52-consteval-proc)
- [6.1 `@derive`](#61-derive)
- [7.1 `struct`](#71-struct)
- [7.2 `enum`](#72-enum)
- [7.3 `variant`](#73-variant)
- [7.4 `union`](#74-union)
- [8.1 `proc` declaration (core form)](#81-proc-declaration-core-form)
- [8.2 `async` / `await`](#82-async-await)
- [8.3 `dedicate` + `spawn` + `join`](#83-dedicate-spawn-join)
- [9.1 `let`](#91-let)
- [9.2 `set` / `add ... by` (node-style mutation)](#92-set-add-by-node-style-mutation)
- [9.3 `return`](#93-return)
- [9.4 `if` / `else`](#94-if-else)
- [9.5 `while`](#95-while)
- [9.6 `for`](#96-for)
- [9.7 `match` / `switch` / `decide`](#97-match-switch-decide)
- [9.8 `try` / `catch` / `finally` / `throw`](#98-try-catch-finally-throw)
- [9.9 `with` / `defer`](#99-with-defer)
- [9.10 `asm`](#910-asm)
- [9.11 `assert`](#911-assert)
- [10.1 Literals / variables / field / index](#101-literals-variables-field-index)
- [10.2 Unary operators: `- not ! ~`](#102-unary-operators-not)
- [10.3 Binary arithmetic: `+ - * / %`](#103-binary-arithmetic)
- [10.4 Bitwise: `& | ^ xor`](#104-bitwise-xor)
- [10.5 Shifts: `shl shr sar << >>`](#105-shifts-shl-shr-sar)
- [10.6 Comparisons: `< <= > >= == !=`](#106-comparisons)
- [10.7 Logical: `and or && ||`](#107-logical-and-or)
- [10.8 Ternary: `cond ? a : b`](#108-ternary-cond-a-b)
- [10.9 Cast: `as`](#109-cast-as)
- [11.1 Arrays: `[5]i64 = [1 2 3 4 5]`](#111-arrays-5i64-1-2-3-4-5)
- [11.2 `vector`, `map`, tuples `(1 "hi" true)`](#112-vector-map-tuples-1-hi-true)
- [12.1 `capability <name>`](#121-capability-name)
- [14.1 `macro`](#141-macro)
- [14.2 Generic procs (`proc identity<T>`)](#142-generic-procs-proc-identityt)
- [17.1 `mmio region`](#171-mmio-region)
- [17.2 `addr/load/store`](#172-addrloadstore)
- [17.3 `asm`](#173-asm)
- [24.1 Call-site template (must obey)](#241-call-site-template-must-obey)
- [Appendix A — Keyword Index (A→Z)](#appendix-a-keyword-index-az)
- [Appendix B — Operator Precedence Table](#appendix-b-operator-precedence-table)
- [Appendix C — Typed CIL Opcode Catalog (Current Feature Set)](#appendix-c-typed-cil-opcode-catalog-current-feature-set)
- [Appendix D — x64 Emission Templates (Windows x86-64, bytes-level forms)](#appendix-d-x64-emission-templates-windows-x86-64-bytes-level-forms)
- [(1) Surface → AST Mapping Table (every keyword / construct)](#1-surface-ast-mapping-table-every-keyword-construct)
- [(2) Typed CIL Opcode → Exact Emission Template(s) Table](#2-typed-cil-opcode-exact-emission-templates-table)
- [“Backend mechanically specifiable” closing constraints (the strict rules)](#backend-mechanically-specifiable-closing-constraints-the-strict-rules)
- [0) Global ABI + Emitter Conventions (mandatory)](#0-global-abi-emitter-conventions-mandatory)
- [1) Opcode Contracts (one entry per opcode)](#1-opcode-contracts-one-entry-per-opcode)
- [1A) Structural / CFG](#1a-structural-cfg)
- [1B) Locals / Moves / Constants](#1b-locals-moves-constants)
- [1C) Addressing + Loads/Stores (non-MMIO)](#1c-addressing-loadsstores-non-mmio)
- [1D) Integer Arithmetic / Bitwise / Shifts (i64)](#1d-integer-arithmetic-bitwise-shifts-i64)
- [1E) Compare + SetCC + Boolean Ops](#1e-compare-setcc-boolean-ops)
- [1F) Select / Choose (branchless)](#1f-select-choose-branchless)
- [1G) Calls (internal + IAT) + Shadow/Align](#1g-calls-internal-iat-shadowalign)
- [1H) Assert / Diagnostics at Runtime Boundary](#1h-assert-diagnostics-at-runtime-boundary)
- [1I) MMIO (volatile semantics)](#1i-mmio-volatile-semantics)
- [1J) Concurrency / Channels / Mutex (runtime-call ops)](#1j-concurrency-channels-mutex-runtime-call-ops)
- [1K) “Structured” constructs that must be lowered to these opcodes (no direct emission)](#1k-structured-constructs-that-must-be-lowered-to-these-opcodes-no-direct-emission)
- [0) Global backend invariants (apply to all opcodes)](#0-global-backend-invariants-apply-to-all-opcodes)
- [1.1 Boolean materialization and branch](#11-boolean-materialization-and-branch)
- [1.4.1 Lawet rules (must be fixed to emit correct bytes)](#141-lawet-rules-must-be-fixed-to-emit-correct-bytes)
- [1.4.2 Field opcodes](#142-field-opcodes)
- [Canonical runtime imports (example ABI)](#canonical-runtime-imports-example-abi)
- [Rule (recommended): `=` is **statement-only assignment**, never an expression](#rule-recommended-is-statement-only-assignment-never-an-expression)
- [C.0 Canonical backend conventions (so templates are deterministic)](#c0-canonical-backend-conventions-so-templates-are-deterministic)
- [C.1.1 Prolog / Epilog / Stack](#c11-prolog-epilog-stack)
- [C.1.2 Addressing / Moves / Constants](#c12-addressing-moves-constants)
- [C.1.3 Integer Arithmetic (i64/u64)](#c13-integer-arithmetic-i64u64)
- [C.1.4 Bitwise ops + Shifts (i64/u64)](#c14-bitwise-ops-shifts-i64u64)
- [C.1.5 Comparisons + Bool materialization](#c15-comparisons-bool-materialization)
- [C.1.6 Control flow: Labels + Branches + Trap/Halt](#c16-control-flow-labels-branches-traphalt)
- [C.1.7 Calls (imports + internal)](#c17-calls-imports-internal)
- [C.1.8 Loads/Stores for scalar locals (common patterns)](#c18-loadsstores-for-scalar-locals-common-patterns)
- [C.1.9 MMIO (volatile) read/write](#c19-mmio-volatile-readwrite)
- [C.1.10 Variants (merged from our last step)](#c110-variants-merged-from-our-last-step)
- [C.2.1 Consolidated emission table (all opcodes)](#c21-consolidated-emission-table-all-opcodes)


---

## 0) The spine: what RANE *is* in one sentence

**RANE is a deterministic, capability-gated systems language whose compilation pipeline is a sequence of audited structural transformations (CIAMs) from human-friendly instruction prose into a Typed IR web that lowers into Windows x64 machine code.**

---

## 1) P.I.E. — the mission statement (Processing Ideas → Instructions → Execution)

### 1.1 “Processing ideas”

This is the phase where we’re still thinking like a human: *What do I want done?*
RANE supports this by letting our code read like:

* “with open path as f”
* “defer close f”
* “start at node start”
* “go to node end_node”
* “choose max a b”
* “requires network_io”

RANE is intentionally shaped so our **intent** remains visible even when the compiler becomes aggressive.

### 1.2 “Instructions”

RANE programs are *instructional directions* that map 1:1 into operable internal forms. That means:

* Every construct becomes an explicit node shape.
* Every effect is explicitly gated.
* Every operation has a defined evaluation order.
* “Sugar” is not magic—CIAMs rewrite it into canonical instruction structure.

### 1.3 “Execution”

Execution is produced as **real machine code** (Windows `.exe` x86-64 PE) by:

* lowering Typed CIL into an optimization graph (OSW),
* planning frames and ABI compliance,
* selecting instructions and emitting bytes / or NASM-compatible output,
* producing a final PE image with imports and sections.

---

## 2) The pipeline (the full “spine” with what each stage *means*)

### 2.1 Pipeline diagram (Official Pipeline)

```
syntax.rane
  ↓  (CIAMs: interpret surface intent)
Lexer / Tokenizer
  ↓  (CIAMs: contextual token shaping)
Parser
  ↓  (CIAMs: grammar rewriting + sugar)
AST (SMD-shape or AST proper)
  ↓  (CIAMs: structural normalization)
Resolver (names, scopes, capabilities, ownership facts)
  ↓  (CIAMs: semantic insertion + inference)
Typed CIL (Typed Common Intermediary Language)
  ↓  (CIAMs: typed rewrites + lowering decisions)
OSW (Optimized Structure Web)
  ↓  (CIAMs: optimization + backend shaping)
Frame Planner
  ↓  (CIAMs: ABI, stack, shadow, alignment)
Codegen (.exe x64 PE) / NASM Emitters
```

### 2.2 What each stage outputs (not vibes—actual deliverables)

#### Stage A — `syntax.rane` (surface source)

* Indentation-based blocks
* Minimal punctuation
* Human-meaningful symbols (`>`, `->`, `and`, `or`, `not`)
* “Instruction prose” flavor

Output: raw source text + file identity + module identity.

#### Stage B — Lexer / Tokenizer

* Turns characters into tokens: identifiers, keywords, literals, operators, indentation/dedent markers.
* “Static yet flexible spacing”: spacing is mostly ignorable except where it separates tokens and where indentation forms blocks.

Output: token stream with spans (`line, col, length`) and structural markers (INDENT/DEDENT/NEWLINE).

#### Stage C — Parser

* Converts tokens into a structured tree.
* Establishes block structure and precedence.
* Creates distinct node kinds: `ProcDecl`, `StructDecl`, `IfStmt`, `ForStmt`, `MatchStmt`, etc.

Output: AST / SMD-style tree (source-shaped structure).

#### Stage D — AST / SMD normalization

* Unifies equivalent surface forms.
* Example: `match`, `switch`, `decide` remain distinct nodes *or* normalize into a shared “Decision” super-node with a tag that preserves intent.

Output: canonical structural tree (same meaning, fewer syntax variants).

#### Stage E — Resolver

This is where “meaning” locks in:

* Name binding (`math::square`)
* Module scope and namespace structure
* Type name resolution (`Maybe<i64>`)
* Capability constraints (`requires network_io`)
* Contract attachment
* Ownership/borrowing legality (if enforced early)
* Macro/template expansion boundaries (what is expanded now vs later)

Output: resolved tree + symbol tables + type references + capability environment + diagnostics.

#### Stage F — Typed CIL

Typed CIL is the “machine-ready truth”:

* Every expression has a type.
* Every call has an effect/capability signature.
* Every memory operation becomes explicit.
* “String typed” becomes concrete: string is a known runtime ABI type.
* Control flow becomes explicit blocks with conditions and branches.

Output: typed IR in a stable, inspectable format.

#### Stage G — OSW (Optimized Structure Web)

OSW is not “a list of passes.” It’s a **web**:

* Nodes = transformations + invariants
* Edges = allowed ordering + prerequisites
* CIAMs can inject patterns here to guide optimization and lowering.

Output: optimized IR + metadata for backend selection.

#### Stage H — Frame Planner

This stage computes:

* stack frame size
* local slots
* spill slots
* alignment padding
* Windows x64 shadow space requirements
* call-site alignment rules
* debug/unwind metadata hooks (if we support it)

Output: frame map + per-proc lawet + calling convention compliance plan.

#### Stage I — Codegen / Emitter

* Instruction selection (IR → x64 instruction forms)
* Register assignment (or uses a deterministic “virtual register → physical register” plan)
* Emits:

  * machine code bytes
  * PE sections
  * import tables and thunks
  * relocation info if needed
  * entrypoint and symbol metadata

Output: `.exe` (and optionally `.asm` / `.obj`).

---

## 3) CIAMs (Contextual Inference Abstraction Macros) — the “everywhere” mechanism

### 3.1 What a CIAM is

A **CIAM** is a deterministic, auditable rewrite unit that can run at any stage.
It is not “a macro system.” It is a **multi-stage semantic transformer**.

A CIAM has:

* **Match pattern** (syntax shape / AST shape / typed IR shape / CFG shape)
* **Context requirements** (types known? symbols resolved? capabilities present?)
* **Rewrite output** (new structure)
* **Invariants** (what must remain true)
* **Audit footprint** (hashable transformation record)

### 3.2 The core promise

CIAMs make RANE feel “natural” without becoming “mystical.”

They enable:

* minimal punctuation while still compiling to precise machine intent
* “prose-like” constructs (`with`, `defer`, `choose`, `start at node`)
* context-aware rewrites (type-directed expansions)
* optimization hints baked into writing style

### 3.3 CIAM lifecycle (fully explicit)

1. **Register** CIAM with stages + required invariants.
2. **Build Context Object** at boundary:

   * stage id
   * symbol snapshot
   * type environment
   * capability environment
   * target ABI info
   * ownership/linearity state
3. **Match** deterministically (ordered rule set).
4. **Rewrite** structure:

   * insert nodes
   * normalize blocks
   * lower sugar
   * attach metadata
5. **Validate invariants** (type/capability/ownership/control-flow rules).
6. **Commit** output as canonical representation.
7. **Log** the transformation record:

   * CIAM name
   * input hash
   * output hash
   * invariant results

### 3.4 Examples of CIAM behavior (concrete)

#### Example A: `with open path as f: ... end`

CIAM rewrite:

* transforms into a `try/finally` structure with guaranteed close
* ensures `file_io` capability is required
* enforces that `f` does not escape scope (or inserts safe boxing if allowed)

#### Example B: `defer close f`

CIAM rewrite:

* pushes a cleanup action onto a scope cleanup stack
* later lowered into finally blocks in Typed CIL or OSW

#### Example C: `choose max a b`

CIAM rewrite:

* replaces with a call to a known intrinsic or runtime function
* may inline for primitives
* ensures determinism and explicit evaluation order

---

## 4) RANE’s philosophy statements — explained line by line (no handwaving)

### 4.1 “No smoke and mirrors; direct and precise”

Meaning:

* No hidden allocations.
* No implicit conversions that change meaning silently.
* No unspecified evaluation order.
* Effects are gated explicitly (capabilities).
* If the compiler does something clever, it can explain it via logs/IR dumps.

### 4.2 “Syntax is heavily machine natural; yet extremely human-friendly”

Meaning:

* Human-friendly: prose-like blocks, minimal punctuation, readable flow.
* Machine-natural: every construct maps to a finite set of canonical IR forms.
* We don’t write “compiler-bait”; we write “instruction truth.”

### 4.3 “Grammar is cohesive and human-oriented”

Meaning:

* Similar concepts have similar forms:

  * `proc name args -> ret: ... end`
  * `struct Name: fields end`
  * `namespace name: ... end`
* The language avoids chaotic punctuation dialects.

### 4.4 “Semantics are basic and beginner welcoming”

Meaning:

* No “gotcha” defaults.
* Direct mental model:

  * variables have types
  * expressions evaluate left-to-right
  * control flow is explicit
  * side effects require capability
* Advanced power exists, but it doesn’t infect the beginner model.

### 4.5 “Programs are a network of separate command-nodes”

Meaning:

* RANE supports both:

  * structured procedural programming (`proc`)
  * explicit node graphs (`node start: ... end`)
* Internally, everything can be represented as:

  * basic blocks + edges
  * or nodes + control transitions
* This supports:

  * deterministic compilation
  * explicit scheduling
  * clear lowering into jump logic

### 4.6 “Memory is layered-stacks with representative virtual registers”

Meaning:

* User-facing model can feel “register-like” (explicit, stable values).
* Implementation uses:

  * stack slots for locals/temps
  * virtual registers in IR
  * physical registers assigned late
* We get predictable performance while keeping readability.

### 4.7 “Dynamic AOT compilation”

Meaning:

* “Ahead-of-time” output is real native code.
* “Dynamic” means the pipeline can:

  * choose different lowering strategies based on context
  * apply CIAMs and optimizations based on target and code patterns
* But determinism is preserved: the same inputs + same configuration → same output.

### 4.8 “Completely runtime-free by default”

Meaning:

* If we don’t import runtime services, we don’t pay for them.
* We can compile a “freestanding-ish” subset:

  * no heap
  * no IO
  * no threads
* When we *do* want services, we explicitly import and require capabilities.

### 4.9 “Intrinsic instruction set by design”

Meaning:
RANE includes builtin operations that are not “library functions,” but **language-level primitives**, like:

* `addr`, `load`, `store`
* `read32`, `write32` on `mmio region`
* `allocate`, `free`, `borrow`, `mutate`
* `trap`, `halt`
* inline `asm`

Each intrinsic has:

* typing rules
* capability rules
* lowering rules into IR + final instructions

### 4.10 “Explicit states”

Meaning:

* No invisible state machines.
* If async exists, its lowering is inspectable (state structs + switch dispatch).
* If ownership exists, it is modeled explicitly (owned vs borrowed).

### 4.11 “Immutable classes; mutable objects”

Interpretation in RANE terms:

* **Types** (struct definitions, enum definitions) are immutable definitions.
* **Values** can be mutable if declared/operated on with explicit mutation forms (`mutate`, `set`, `add ... by`).
* Mutation is never accidental; it is written.

### 4.12 “Safety baked into syntax; security compile-time centric”

Meaning:

* Security is not “runtime checks everywhere.”
* It’s:

  * capability gating
  * contracts
  * deterministic behavior
  * ownership/borrowing rules (where enabled)
  * compile-time denial of forbidden effects

---

## 5) Core language surface (what our syntax demonstrates)

I’m going to treat our provided syntax as **canonical supported forms** and explain each category:

---

## 6) Modules, imports, namespaces (the program’s global topology)

### 6.1 `import rane_rt_print`

* Declares a dependency on a runtime/service module.
* Introduces symbols into import table and/or resolver environment.
* May be used for:

  * direct calls (e.g., `print`)
  * capability association (e.g., `file_io` maps to `rane_rt_fs`)

**Compilation meaning:**
Imports become:

* resolver-visible module references
* and later: PE imports (LoadLibrary/GetProcAddress or static import table depending on design)

### 6.2 `module demo_root`

* Establishes module identity.
* Determines:

  * symbol prefixes / name mangling domain
  * export namespace
  * compilation unit boundaries

### 6.3 `namespace math: ... end`

* Logical grouping.
* Influences symbol paths: `math::square`.

---

## 7) Procedures (procs), visibility, qualifiers

### 7.1 Proc form

Surface:

```rane
export inline proc square x i64 -> i64:
  return x * x
end
```

Meaning breakdown:

* `export`: visible outside the module (emitted in symbol table / export list)
* `inline`: optimization directive, but also a semantic hint (“safe to duplicate body”)
* `proc`: function/procedure declaration
* `square`: name
* `x i64`: parameter list, each param has a name and type
* `-> i64`: return type
* block body
* `end`: closes proc

Typed CIL form:

```rane
export inline proc square(x: i64) -> i64 { return x * x; }
```

### 7.2 Visibility keywords

* `public`: callable by other modules (subject to export policy)
* `protected`: callable within module + friend scopes (depending on our rules)
* `private`: callable only inside namespace/module scope
* `admin`: elevated scope (often for privileged capabilities or internal runtime glue)

**Compiler meaning:**
Visibility controls:

* symbol table emission
* linker/export metadata
* resolver access rules
* optional LTO boundaries

---

## 8) Types: primitives, aliases, user-defined aggregates

### 8.1 Primitive declarations

We list:

* signed ints: `i8 ... i512`
* unsigned ints: `u8 ... u512`
* floats: `f32 f64 f128`
* `bool void int string`

**Interpretation:**
These `type` lines can mean either:

1. “declare builtins into the module type environment,” or
2. “alias builtin types into the current compilation unit,” or
3. “expose them as part of the language prelude.”

Regardless, by the time Typed CIL exists, these are concrete types with known:

* size
* alignment
* ABI behavior
* allowed operations

### 8.2 `typealias` and `alias`

* `typealias word = u32`: introduces a named alias that preserves type identity rules we define (could be “strong alias”)
* `alias int32 = i32`: often a “weak alias” (pure synonym)

**Important distinction (recommended):**

* `alias` = synonym (no new type identity)
* `typealias` = named type wrapper (distinct identity, same representation)
  This supports safer APIs without runtime cost.

---

## 9) Constants: `const`, `constexpr`, `constinit`, `consteval`

### 9.1 `const PI f64 = ...`

* compile-time constant value
* usable wherever constant expressions are allowed
* must be immutable

### 9.2 `constexpr E f64 = ...`

* stronger guarantee: must be evaluable at compile-time
* can be used in compile-time contexts (array sizes, enum values, etc.)

### 9.3 `constinit ZERO i64 = 0`

* guarantees initialization occurs at program init, not lazily
* used for static storage objects that must be initialized deterministically

### 9.4 `consteval proc const_fn -> i64: return 42 end`

* proc executed at compile time whenever referenced
* cannot depend on runtime data
* output is embedded into IR as a literal

---

## 10) Structs, enums, variants, unions — data modeling explained fully

### 10.1 Attributes: `@derive Eq Ord Debug`

* compile-time generated implementations
* implemented via CIAM or derive system

Meaning:

* `Eq`: generate equality comparison
* `Ord`: generate ordering
* `Debug`: generate debug formatting

**Lowering meaning:**

* Derive generates:

  * procs or methods in Typed CIL/OSW
  * or runtime-vtable-like hooks if we ever choose that (but our “runtime-free by default” suggests avoid vtables)

### 10.2 `struct Person: name string age u8 end`

* field list
* deterministic lawet (C-like)
* no hidden padding decisions: lawet is defined and inspectable

### 10.3 `enum Flags u8: ... end`

* enum with explicit representation type (`u8`)
* supports bitwise composition:

  * `ReadWrite = Read | Write`

**Lowering meaning:**

* stored as the repr type (`u8`)
* operations are integer ops

### 10.4 `variant Maybe<T>: Some T None end`

* sum type with cases
* generic parameter `T`

**Lowering meaning (typical):**

* tag + payload union
* tag repr chosen (e.g., `u8` if few cases)
* payload contains `T` for `Some`, nothing for `None`

### 10.5 `union IntOrFloat: i i32 f f32 end`

* overlapping storage; manual interpretation
* unsafe potential, but still type-known

**Lowering meaning:**

* size = max(field sizes), align = max(field alignments)
* access is explicit

---

## 11) Low-level hardware features: `mmio region`, `addr`, `load`, `store`

### 11.1 `mmio region REG from 4096 size 256`

Defines a named memory-mapped IO region:

* base address: 4096
* size: 256 bytes
* the symbol `REG` becomes a region handle

### 11.2 `read32 REG 0 into x`

* volatile load from REG base + offset 0
* ensures proper width and ordering

### 11.3 `write32 REG 4 123`

* volatile store into offset 4

### 11.4 `addr/load/store` forms

Our examples:

* `let p0 = addr 4096 4 8 16`
* `load u32 addr 4096 0 1 0`
* `store u32 addr 4096 0 1 8 7`

These read like an address-construction DSL:

* base
* scale
* index
* displacement
* value

**Lowering meaning:**

* compile-time folding if constants
* emits LEA + MOV patterns
* enforces alignment / access width rules

---

## 12) Capabilities: the compile-time security model

### 12.1 Capability declarations

We define:

* `capability heap_alloc`
* `capability file_io`
* `capability network_io`
* `capability dynamic_eval`
* `capability syscalls`
* `capability threads`
* `capability channels`
* `capability crypto`

### 12.2 Capability use: `requires`

Example:

```rane
async proc async_fetch -> i64 requires network_io:
  ...
end
```

Meaning:

* This proc *cannot compile* unless it is explicitly marked as requiring `network_io`.
* A caller must also be in a capability context that includes `network_io` (or be marked similarly).
* This enforces effect safety at compile time.

### 12.3 Why this matters

This is our “security is compile time centric” line made real:

* We can’t accidentally do IO.
* We can’t “accidentally” spawn threads.
* We can’t hide `eval` inside some helper; it contaminates the call chain unless explicitly permitted.

---

## 13) Contracts and assertions: correctness rules as part of code

### 13.1 Contracts

```rane
contract positive x i64:
  ensures x > 0
end
```

Meaning:

* a named logical guarantee
* attachable to values/procs
* can be enforced:

  * purely compile-time (when provable)
  * optionally runtime (if we choose), but our model leans compile-time-first

### 13.2 Assertions

```rane
assert x != 0 "x must be non-zero"
```

Meaning:

* explicit condition with message
* lowering choices:

  * compile-time eliminate if provable
  * otherwise emit a conditional trap / error path

---

## 14) Macros and templates: two different powers

### 14.1 Macro (text/structure rewrite)

```rane
macro SQUARE x = x * x
```

Meaning:

* expands before typing (or during early typing, depending on design)
* should preserve determinism
* must not introduce hidden effects unless explicitly allowed

### 14.2 Template / Generics (type-driven)

```rane
template T
proc generic_id x T -> T:
  return x
end
```

Meaning:

* monomorphization: `generic_id<i64>` becomes a concrete proc
* type constraints can be added later (if we want)
* expands into Typed CIL specialized copies

---

## 15) Concurrency primitives: async/await, threads, mutex, channels

### 15.1 `async` + `await`

* `async proc` returns a value but may suspend.
* `await` pauses until result is ready.

**Lowering meaning:**

* state machine:

  * a state struct (locals + state index)
  * a resume function
  * a dispatch switch on state index
* determinism:

  * scheduling must be explicit or stable per our `pragma "scheduling" "fair"`

### 15.2 `dedicate proc` + `spawn/join`

Our intent:

* `dedicate` signals “this proc is meant to be run as a worker”
* `spawn` creates a thread task handle
* `join` waits and retrieves result

### 15.3 `mutex` and `lock`

```rane
mutex m1
lock m1:
  print "locked"
end
```

Meaning:

* `lock` is a structured region
* must lower into:

  * lock acquire
  * try/finally unlock (so unlock is guaranteed)

### 15.4 `channel<int> ch` + `send/recv`

* typed message passing
* blocking semantics defined by runtime
* compile-time ensures message types match

---

## 16) Resource management: `with`, `defer`, ownership ops

### 16.1 `with open path as f: ...`

* structured resource acquisition
* deterministic cleanup

### 16.2 `defer close f`

* schedules cleanup at scope end
* CIAM lowers into finally blocks

### 16.3 Ownership operations

```rane
let p = allocate i32 4
mutate p[0] to 10
let q = borrow p
free p
```

Meaning:

* `allocate` returns owned memory (requires `heap_alloc`)
* `borrow` returns a non-owning reference
* `free` consumes ownership

**Compile-time safety options (depending on how strict we want):**

* forbid `free p` while `q` is live
* or require `q` be dropped/ended before free
* or allow but mark as unsafe (we didn’t include `unsafe`, so best is to enforce structurally)

---

## 17) Control flow: if/else, loops, match/switch/decide, goto/label

### 17.1 Deterministic evaluation

* Expressions evaluate left-to-right.
* Conditions are explicit booleans.
* No “truthy” ambiguity unless we define it (we show `if (f & Flags.Write):`, so we likely define nonzero → true only for `bool` or provide an explicit rule for flags).

### 17.2 Loops

* `while` and `for` forms are canonical
* `#pragma unroll 4` is a lowering directive

### 17.3 Pattern matching

`match` supports destructuring variants:

```rane
match m1:
  case Some x: print x
  case None:   print "none"
end
```

Lowering:

* inspect tag
* jump to case blocks
* bind payload if present

### 17.4 `switch` and `decide`

We keep distinct keywords because they can carry different semantic intent:

* `switch` = standard branching
* `decide` = “decision table” semantics, can map to jump tables more aggressively
* both can lower into the same backend structures but preserve author intent for optimization.

### 17.5 Labels / goto / trap / halt

These are explicit low-level control tools:

* `label L_true:`
* `goto (cond) -> L_true L_false`
* `trap` raises an unrecoverable fault (optionally with code)
* `halt` terminates execution path

**Compiler meaning:**

* these map directly into basic blocks and jumps
* `trap` can map to `int3`, `ud2`, or a runtime abort stub
* `halt` can map to an exit syscall or a controlled termination path

---

## 18) Inline `asm:` blocks

```rane
asm:
  mov rax 1
  add rax 2
  mov out rax
end
```

Meaning:

* allows direct target instructions
* must be constrained to preserve:

  * clobber rules
  * type safety around inputs/outputs
  * calling convention constraints

A strict design:

* only allows named output binds (`mov out rax`)
* requires capability `syscalls` or `unsafe_asm` (we used `syscalls`)

---

## 19) Exceptions: `try/catch/finally`, `throw`

RANE includes:

* recoverable flow (`throw`, `catch`)
* deterministic cleanup (`finally`)
* structural lowering into explicit control flow (not mystical stack unwinding unless we implement real SEH)

**Possible lowering strategies:**

1. **Zero-cost SEH-style** (harder)
2. **Explicit error-return + handler blocks** (simpler, deterministic, auditable)
3. **Trap-based** for “no exceptions mode”

Given our “runtime-free by default,” the simplest consistent approach is:

* lower `throw` into a structured error return or a jump to a handler block
* `try/catch` becomes a decision region
* `finally` always becomes guaranteed cleanup blocks

---

## 20) `eval` and dynamic features

```rane
proc eval_example x string -> i64 requires dynamic_eval:
  let res i64 = eval "10 + " + x
  ...
end
```

Meaning:

* dynamic evaluation is not “for free”
* it is capability-gated (`dynamic_eval`)
* the compiler can:

  * forbid it in production builds
  * sandbox it
  * or require explicit trust policies

This is exactly how we keep “professional language” + “security compile-time centric.”

---

## 21) Operators: our full operator set, with rules

We demonstrate:

* unary: `-`, `not`, `!`, `~`
* arithmetic: `+ - * / %`
* bitwise: `& | ^ xor`
* shifts: `shl shr sar << >>`
* comparisons: `< <= > >= == !=`
* assignment: `=`
* logic: `and or && ||`
* ternary: `? :`

A coherent rule system (the “symbols mean what they mean” promise):

* `>` is strictly numeric/ordered comparison (not “generic weirdness”)
* `->` is “into/then/transition” and used in:

  * return types
  * mapping pairs (`"a" -> 1`)
  * goto conditional targets (`goto cond -> L_true L_false`)
* `and/or/not` are readable aliases to boolean logic, while `&&/||/!` exist for familiarity.
* `xor`, `shl`, `shr`, `sar` provide word-meaning clarity.

---

## 22) Symbol literals: `#rane_rt_print`, `#REG`

```rane
let sym0 = #rane_rt_print
let sym1 = #REG
```

Meaning:

* compile-time symbol handles
* can represent:

  * imported module symbol IDs
  * mmio region IDs
  * reflection handles
  * debug identifiers

Lowering:

* into immediate IDs (integers) or pointers (if we build a symbol table blob)

---

## 23) Node-graph programming: `module demo_struct`, `node start: ...`

This is a major identity feature: **programs as command-node networks**.

Example:

```rane
node start:
  set h Header to Header:
    magic 0x52414E45
    ...
  end
  say "ok"
  go to node end_node
end
```

Meaning:

* nodes are explicit basic blocks with named identity
* `go to node` is a jump
* `start at node start` defines entrypoint for this module’s node graph

This gives we:

* visualizable CFG
* deterministic scheduling
* easy conversion to “structure web” graphs
* a natural bridge to mission/script logic systems

---

## 24) Typed CIL — what it is, why it exists, what it must guarantee

### 24.1 Definition

**Typed CIL is the canonical typed IR that is close enough to hardware to compile easily, but structured enough to optimize safely.**

### 24.2 Guarantees Typed CIL must provide

By the time something is Typed CIL:

* Every identifier has a resolved symbol binding.
* Every expression has a known type.
* Every call has a known capability/effect signature.
* Control flow is explicit in blocks.
* Memory operations are explicit and typed.
* No ambiguous syntax remains.

### 24.3 Why Typed CIL exists (the practical reason)

If we skip a stable typed IR, we end up with:

* scattered lowering logic
* duplicated rules
* “optimization spaghetti”
* fragile codegen

Typed CIL is the “single truth” the rest of the compiler can trust.

---

## 25) OSW — Optimized Structure Web (why it’s a “web” not a “list”)

### 25.1 Why “web”

Because optimization is not one-dimensional. We have:

* prerequisites (type facts, alias facts, capability facts)
* mutual exclusions (can’t inline after some transforms if we want debug fidelity)
* multiple routes to the same end (jump table vs if-chain)
* target-driven decisions (size vs speed)

So OSW is:

* transformations as nodes
* dependencies as edges
* CIAMs can inject edges (“prefer pattern X if pragma says hot”)

### 25.2 What OSW *does* to our constructs

* `match/switch/decide` → decision tree + jump table selection
* `async/await` → state machine lowering
* `with/defer/lock` → guaranteed cleanup regions
* loops → unroll/strength-reduce if permitted
* addr/load/store → fold constants, choose LEA patterns
* constant propagation across blocks
* remove unreachable code created by consteval folding

---

## 26) Frame Planner — the ABI truth layer (Windows x64 specifics)

### 26.1 Windows x64 must-haves

* first 4 integer args in: `RCX, RDX, R8, R9`
* return in: `RAX`
* stack must be **16-byte aligned at call sites**
* caller reserves **32 bytes shadow space** for callee
* callee-saved registers preserved as required

### 26.2 What a frame planner computes

For each proc:

* local slot sizes + offsets
* temp/spill slot sizes + offsets
* saved register slots
* outgoing call arg area (if needed)
* shadow space
* alignment padding
* final frame size

Output is a **map** from IR locals/temps → `[rsp+offset]` (or `[rbp-offset]` if we use frame pointers).

---

## 27) Codegen: from Typed CIL/OSW into x64 machine code / PE

### 27.1 Instruction selection

Map each IR op into a small set of patterns:

* arithmetic ops → `add/sub/imul/idiv`
* comparisons → `cmp` + `setcc` or branches
* branches → `jcc/jmp`
* loads/stores → `mov` with proper widths
* calls → `call` with ABI setup

### 27.2 Register allocation (deterministic)

RANE emphasizes determinism, so register allocation should be:

* stable given identical input
* reproducible across machines

We can choose:

* linear scan (deterministic, simpler)
* graph coloring (more optimal, more complex)

### 27.3 PE emission

The emitter must build:

* DOS header + NT headers
* `.text` section (code)
* `.rdata` (constants, strings)
* `.idata` (imports) or custom resolver blob
* entrypoint + import resolution strategy

---

## 28) Tooling and “built for creation”

### 28.1 Intuitive build system

Our philosophy implies:

* builds are declarative and deterministic
* “package manager baked into sequential render and export logic”
  Meaning:
* modules declare imports and exports
* build graph is derived from module dependency graph
* “render” = compile + link + emit
* “export” = package artifacts + symbol manifests + CIAM logs

### 28.2 Dynamic tooling

Because CIAMs operate everywhere, tooling can:

* show “what rewrites happened”
* show IR at every boundary
* show capability flow (why a call is forbidden)
* show ownership flow (why borrow is invalid)
* show frame lawet (why alignment changed)

---

## 29) “Optimizations baked into how code is written”

This is not marketing—it means the surface language is shaped so the compiler can reliably recognize patterns.

Examples:

* `inline proc hot_add` + `#pragma profile "hot"`:

  * encourages inlining and fast-path decisions
* `decide`:

  * signals a jump-table friendly branch set
* `dedicate proc`:

  * signals thread-worker style with fewer captured closures
* explicit `requires`:

  * lets optimizer reason about allowed side effects and reorder safely

---

## 30) “Polynomial-fibonacci ciphers replace obfuscation” (explained safely)

Interpreting this as *a design philosophy for optional code transformation*:

* Obfuscation is messy and unpredictable.
* A deterministic cipher-like transformation layer could:

  * encode literals or metadata
  * produce reproducible transformations
  * be used for watermarking or anti-tamper *in controlled contexts*

If used, it must remain:

* opt-in
* deterministic
* transparent in build logs
* not a substitute for real security (capabilities and sandboxing are real security)

---

## 31) Putting it all together: what our provided program demonstrates

Our program is effectively a **total coverage file**:

* imports, modules, namespaces
* procs of all visibilities
* primitives and user-defined types
* constants and compile-time evaluation
* traits/derives
* enums, variants, unions
* mmio and address operations
* capabilities and requires
* contracts/assert
* macros and templates
* async/await and threads/channels/mutex
* with/defer
* inline asm
* try/catch/finally/throw
* eval
* arrays/vectors/maps/tuples
* linear/nonlinear tags
* match/switch/decide
* loops + unroll
* tail recursion
* pragmas/defines
* symbol literals
* node-graph module + entry node
* gotos/labels/trap/halt
* final `main` requiring many capabilities

It’s not just code—it’s a *language manifesto as an executable testbed*.

---

## 32) The “civilization” view: what RANE ultimately becomes

RANE is positioned to be:

1. **A language** (human-friendly but machine-truth)
2. **A security model** (capabilities as compile-time effects)
3. **A compiler architecture** (CIAMs across every boundary)
4. **An IR ecosystem** (Typed CIL + OSW as stable tool targets)
5. **A production emitter** (real `.exe` output; deterministic builds)
6. **A creative platform** (node networks, structured prose, creation-first standard library)

---

## 33) The complete mental model in one final “execution story”

We write:

* a set of instructional declarations and command nodes

The compiler:

* tokenizes and parses deterministically
* applies CIAMs to preserve intent while normalizing structure
* resolves meaning (names, scopes, types, capabilities, ownership facts)
* produces Typed CIL (the typed truth)
* feeds OSW (the optimization web that is still auditable)
* plans frames (ABI truth, alignment, shadow, slots)
* emits x64 machine code + PE structure

The result:

* a real Windows x86-64 executable whose behavior is:

  * deterministic
  * capability-governed
  * structurally inspectable at every stage

---

**********

# RANE Reference Manual

**Reliable Adaptive Natural Efficient (RANE Processing Language)**
**Full Chaptered Manual — Every Keyword + Construct**
**For each construct:**
✅ Grammar (Surface + Typed CIL) · ✅ Typing Rules · ✅ Capability Rules · ✅ Lowering (CIAM stage mapping) · ✅ OSW expectations · ✅ x64 codegen patterns · ✅ Diagnostics

> **Notation**

* **Surface grammar** shown in a readable EBNF-ish style (indent blocks implied).
* **Typed CIL** shown in our punctuated canonical form.
* Types: `i8..i512`, `u8..u512`, `f32..f128`, `bool`, `void`, `int`, `string`, plus user types.
* Capabilities are compile-time effects: `requires(cap)` is mandatory for using guarded operations.
* Diagnostics are described as: **CODE** — message — typical location.

---

# Table of Contents

1. **Lexical Structure**
2. **Modules, Imports, Namespaces**
3. **Visibility & Qualifiers**
4. **Types & Type Declarations**
5. **Constants & Compile-Time Evaluation**
6. **Attributes & Derives**
7. **Data Types: struct / enum / variant / union**
8. **Procedures (proc): parameters, return, generics**
9. **Statements**
10. **Expressions & Operators**
11. **Collections & Aggregates**
12. **Capabilities & Effects**
13. **Contracts & Assertions**
14. **Macros & Templates**
15. **Concurrency: async/await, threads, mutex, channels**
16. **Resource Management: with / defer**
17. **Low-Level & Systems: mmio, addr/load/store, asm, syscalls**
18. **Exceptions: try/catch/finally/throw**
19. **Dynamic Evaluation: eval**
20. **Symbols: #sym literals**
21. **Node Graph Programming: node/start/say/go/halt**
22. **Control Transfer: goto/label/trap/halt**
23. **Pragmas & Defines**
24. **Backend: Frame Planner & Windows x64 ABI**
25. **Diagnostics Model (rane_diag_t)**

---

# 1) Lexical Structure

## 1.1 Tokens

### Surface Forms

* **Identifiers:** `Name`, `snake_case`, `_private`, `T`, `Maybe`
* **Qualified paths:** `math::square`
* **Literals:**

  * Integer: `123`, `1_000_000`, `0xCAFE_BABE`, `0b1010_0101`
  * Float: `3.14`, `2.718281828459045`
  * String: `"hello"`, `"with \\n escape"`
  * Bool: `true`, `false`
  * Null: `null`

### Typed CIL Forms

Same values, but punctuation and separators are explicit: `, ; ( ) { }`.

## 1.2 Indentation & Block Structure

### Surface Grammar

* A block begins after `:` and is terminated by `end`.
* Indentation is *informal* for style but **structure is still block-based**.

### Typed CIL

Blocks are `{ ... }`.

## Diagnostics

* **RANE_DIAG_PARSE_ERROR** — “unexpected token …” / “missing `end`” / “bad indentation or block terminator”
* **RANE_DIAG_INTERNAL_ERROR** — “lexer/parser invariant broken” (compiler bug)

---

# 2) Modules, Imports, Namespaces

## 2.1 `import`

### Surface Grammar

```
import <module_name>
import <namespace>::<symbol>
```

### Typed CIL

```
import <module_name>;
import <namespace>::<symbol>;
```

### Typing Rules

* `import module` introduces a module handle and its exported symbol set.
* `import ns::sym` introduces `sym` into current scope with a qualified binding.

### Capability Rules

* Importing does **not** grant capabilities.
* Capabilities are granted only by:

  * `capability name` declaration + allowed policy,
  * and used by `requires`.

### Lowering (CIAM mapping)

* **CIAM@syntax/lexer:** recognize module tokens; normalize separators.
* **CIAM@resolver:** build import graph, attach symbol IDs.
* **CIAM@codegen:** convert imports to PE import table entries (or resolver thunks).

### OSW Expectations

* Imports guide inlining decisions and LTO boundaries.
* Dead import elimination if no symbol referenced.

### x64 Codegen Patterns

* Imports become:

  * IAT references (`mov rax, [__imp_Function] ; call rax`)
  * or thunk calls (`call __rane_thunk_Function`)

### Diagnostics

* **RANE_DIAG_UNDEFINED_NAME** — “imported symbol not found: …”
* **RANE_DIAG_REDECLARED_NAME** — “import conflicts with existing name: …”

---

## 2.2 `module`

### Surface Grammar

```
module <ident>
```

### Typed CIL

```
module <ident>;
```

### Typing Rules

* Exactly one `module` per compilation unit (unless we implement multi-module files).
* Defines a symbol namespace root.

### Capability Rules

None.

### Lowering

* **CIAM@resolver:** establishes root symbol scope.
* **CIAM@codegen:** sets PE metadata: entrypoint mapping and export namespace.

### OSW Expectations

* Module boundaries define default optimization visibility unless LTO enabled.

### x64 Patterns

None directly; affects symbol naming/mangling.

### Diagnostics

* **RANE_DIAG_PARSE_ERROR** — “duplicate module declaration”
* **RANE_DIAG_REDECLARED_NAME** — “module name collides with import/namespace”

---

## 2.3 `namespace`

### Surface Grammar

```
namespace <ident>:
  <decls...>
end
```

### Typed CIL

```
namespace <ident> { <decls...> }
```

### Typing Rules

* Creates a nested scope. Names resolve as `ns::name`.

### Capability Rules

None directly; capabilities declared inside are still global symbols.

### Lowering

* **CIAM@resolver:** scopes + symbol prefixing.
* **CIAM@typed-CIL:** canonicalizes paths.

### OSW Expectations

* Namespace does not change semantics; can group exports.

### x64 Patterns

Affects mangled names only.

### Diagnostics

* **RANE_DIAG_REDECLARED_NAME** — “namespace already defined”
* **RANE_DIAG_PARSE_ERROR** — “namespace missing `end`”

---

# 3) Visibility & Qualifiers

## 3.1 Visibility keywords: `public`, `protected`, `private`, `admin`

### Surface Grammar

```
<vis> proc ...
<vis> <decl> ...
```

### Typed CIL

```
<vis> proc ... { ... }
```

### Typing Rules

* Visibility affects name resolution legality across modules/scopes.

### Capability Rules

* `admin` may be allowed to require/permit higher-risk capabilities by policy (compiler config).

### Lowering

* **CIAM@resolver:** enforces access.
* **CIAM@codegen:** exports only public/exported symbols.

### OSW Expectations

* Private procs are more aggressively inlined and DCE’d.

### x64 Patterns

* Exported functions: present in export metadata (if we implement PE exports).

### Diagnostics

* **RANE_DIAG_SECURITY_VIOLATION** — “access denied to private/protected symbol”
* **RANE_DIAG_UNDEFINED_NAME** — “symbol not visible in this scope”

---

## 3.2 Qualifiers: `inline`, `async`, `dedicate`, `linear`, `nonlinear`

Each is a keyword modifier on `proc`.

We’ll define them where procs are defined (Chapter 8), but globally:

* `inline`: optimization and linkage hint.
* `async`: changes return shape/lowering into a state machine.
* `dedicate`: thread-worker intent (spawn-friendly calling convention constraints).
* `linear/nonlinear`: type/effect discipline tags (linearity rules).

Diagnostics typically:

* **RANE_DIAG_TYPE_MISMATCH** when mixing linear resources wrong.
* **RANE_DIAG_SECURITY_VIOLATION** if async implies forbidden capability pattern by policy.

---

# 4) Types & Type Declarations

## 4.1 `type` (primitive or prelude registration)

### Surface Grammar

```
type <ident>
```

### Typed CIL

```
type <ident>;
```

### Typing Rules

* For builtins: registers them into the compilation environment.
* If user-defined `type` is allowed: it becomes an opaque nominal type (we didn’t show that use, so treat as builtin registration).

### Capability Rules

None.

### Lowering

* **CIAM@resolver:** binds `type` names to builtin types.
* **CIAM@typed-CIL:** resolves sizes/alignments.

### OSW Expectations

* Enables constant folding and instruction selection per type width.

### x64 Patterns

* Arithmetic instruction width follows type:

  * `i64/u64` → 64-bit ops
  * `i32/u32` → 32-bit ops (zero-extend semantics on x64)
  * `i8/u8` → byte ops (often promoted)

### Diagnostics

* **RANE_DIAG_UNDEFINED_NAME** — “unknown type: …”
* **RANE_DIAG_REDECLARED_NAME** — “type already defined: …”

---

## 4.2 `typealias` and `alias`

### Surface Grammar

```
typealias <Name> = <Type>
alias <Name> = <Type>
```

### Typed CIL

```
typealias <Name> = <Type>;
alias <Name> = <Type>;
```

### Typing Rules (recommended canonical)

* `alias`: pure synonym; `Name` and `Type` identical for type checking.
* `typealias`: distinct nominal identity but same representation; implicit conversion rules must be defined.

  * Safe rule: no implicit conversion; require `as` cast.
  * Or allow implicit widening if we want.

### Capability Rules

None.

### Lowering

* **CIAM@resolver:** binds alias mapping.
* **CIAM@typed-CIL:** either erases aliases (synonyms) or preserves nominal tags (typealias).

### OSW Expectations

* alias erasure helps optimization; typealias preserved for API safety but erased before codegen.

### x64 Patterns

Representation identical to underlying type.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — “cannot use typealias value as underlying type without cast”
* **RANE_DIAG_REDECLARED_NAME** — alias name conflicts

---

# 5) Constants & Compile-Time Evaluation

## 5.1 `const`, `constexpr`, `constinit`

### Surface Grammar

```
const <Name> <Type> = <Expr>
constexpr <Name> <Type> = <Expr>
constinit <Name> <Type> = <Expr>
```

### Typed CIL

```
const <Name>: <Type> = <Expr>;
constexpr <Name>: <Type> = <Expr>;
constinit <Name>: <Type> = <Expr>;
```

### Typing Rules

* `<Expr>` must be convertible to `<Type>`.
* `constexpr` must be compile-time evaluable.
* `constinit` must be valid for static initialization (no runtime calls).

### Capability Rules

* constants cannot require capabilities unless the value is purely compile-time computed.
* `constexpr/consteval` must not depend on capability-gated runtime operations.

### Lowering

* **CIAM@typed-CIL:** constant folding, embedding literal data into `.rdata`.
* **CIAM@OSW:** propagation + DCE of unreachable branches.

### OSW Expectations

* aggressive propagation, branch pruning:

  * `if (constexpr_cond)` → remove dead branch

### x64 Patterns

* literal embedding:

  * small ints in immediates: `mov rax, imm64`
  * large constants / strings in `.rdata` with RIP-relative addressing:

    * `lea rcx, [rip+str_const]`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — bad initializer type
* **RANE_DIAG_PARSE_ERROR** — malformed constant decl
* **RANE_DIAG_SECURITY_VIOLATION** — constexpr uses forbidden effect (e.g., IO)

---

## 5.2 `consteval proc`

### Surface Grammar

```
consteval proc <name> <params...> -> <ret>:
  <body>
end
```

### Typed CIL

```
consteval proc <name>(...) -> <ret> { ... }
```

### Typing Rules

* Body must typecheck normally.
* All operations inside must be compile-time evaluable.

### Capability Rules

* Must not call capability-gated procs unless those procs are also consteval and pure.

### Lowering

* **CIAM@resolver:** mark proc as compile-time only.
* **CIAM@typed-CIL:** evaluate when referenced; replace call with literal.

### OSW Expectations

* Replace calls with constants; eliminate unused consteval bodies if not referenced.

### x64 Patterns

* No runtime call emitted; result is literal.

### Diagnostics

* **RANE_DIAG_SECURITY_VIOLATION** — consteval depends on runtime
* **RANE_DIAG_INTERNAL_ERROR** — evaluator failure (compiler bug)
* **RANE_DIAG_TYPE_MISMATCH** — return type mismatch

---

# 6) Attributes & Derives

## 6.1 `@derive`

### Surface Grammar

```
@derive <Ident> <Ident> ...
struct ...
```

### Typed CIL

```
@derive(Eq, Ord, Debug)
struct ...
```

### Typing Rules

* Each derive must be known.
* Derive may require trait-like constraints on fields (e.g., `Eq` needs field types to be comparable).

### Capability Rules

* Derive-generated code must not introduce capability requirements unless explicitly declared (prefer: derive is pure).

### Lowering

* **CIAM@AST or resolver:** expand derive into generated procs/impls.
* **CIAM@typed-CIL:** typecheck generated code.

### OSW Expectations

* Generated helpers inline well; DCE removes unused derived functions.

### x64 Patterns

* Eq: sequence of field comparisons + short-circuit branches.
* Ord: lexicographic compare.
* Debug: calls print/format functions (would then require capability if printing is capability-gated—so prefer Debug to generate formatting, not IO).

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — field type doesn’t support required ops
* **RANE_DIAG_UNDEFINED_NAME** — unknown derive
* **RANE_DIAG_SECURITY_VIOLATION** — derive tries to emit effectful code without permission

---

# 7) Data Types

## 7.1 `struct`

### Surface Grammar

```
struct <Name>:
  <field_name> <Type>
  ...
end
```

### Typed CIL

```
struct <Name>:
  <field>: <Type>
end
```

### Typing Rules

* Fields must have defined types.
* No duplicate field names.

### Capability Rules

None.

### Lowering

* **CIAM@resolver:** bind struct type symbol.
* **CIAM@typed-CIL:** compute lawet (size, align, offsets).

### OSW Expectations

* Field access can be strength-reduced to constant offsets.
* Scalar replacement (SROA) may break structs into registers if safe.

### x64 Patterns

* Field load: `mov rax, [base + offset]`
* Field store: `mov [base + offset], rax`
* If stack-allocated struct: address is `rsp + slot + offset`.

### Diagnostics

* **RANE_DIAG_REDECLARED_NAME** — duplicate field
* **RANE_DIAG_UNDEFINED_NAME** — unknown field type

---

## 7.2 `enum`

### Surface Grammar

```
enum <Name> <ReprType>:
  <Case> = <ConstExpr>
  ...
end
```

### Typed CIL

```
enum <Name> : <ReprType> { ... }
```

### Typing Rules

* Values must be representable in repr type.
* If no value given, we may define auto-increment rules (not shown; assume explicit or simple defaulting).

### Capability Rules

None.

### Lowering

* **CIAM@typed-CIL:** map enum to integer representation.

### OSW Expectations

* constant folding; switch lowering into jump tables.

### x64 Patterns

* Comparisons compile as integer comparisons.
* Bitflags compile as `and/or/test`.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — enum value out of range
* **RANE_DIAG_PARSE_ERROR** — malformed enum entry

---

## 7.3 `variant`

### Surface Grammar

```
variant <Name><<T...>>:
  <Case> <Type?>
  ...
end
```

### Typed CIL

```
variant <Name><T> = Case(T) | None
```

(or our struct-style variant form)

### Typing Rules

* Each case payload type must be valid.
* Pattern matching must cover cases or provide default (depending on exhaustiveness rules).

### Capability Rules

None.

### Lowering

* **CIAM@typed-CIL:** lower to tagged union:

  * `tag` + `payload`
* **CIAM@OSW:** match lowering to decision tree.

### OSW Expectations

* Tag tests are optimized; payload loads only in relevant branches.
* If variant is known at compile time, branch pruning happens.

### x64 Patterns

* `tag` is loaded and compared:

  * `mov al, [obj + tag_off]`
  * `cmp al, imm`
  * `je case_label`
* payload read uses fixed offsets.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — wrong payload type for case
* **RANE_DIAG_PARSE_ERROR** — invalid pattern in match
* **RANE_DIAG_UNDEFINED_NAME** — unknown case name

---

## 7.4 `union`

### Surface Grammar

```
union <Name>:
  <field> <Type>
  ...
end
```

### Typed CIL

```
union <Name> { ... }
```

### Typing Rules

* All fields share storage; reading a different field than last written may be undefined unless we define rules.
* We can require explicit casts or “active field” tracking (not shown; simplest is “unsafe by convention”).

### Capability Rules

None unless we create an `unsafe` capability later.

### Lowering

* union size = max(field sizes), alignment = max(field align).
* access is offset 0 for all fields.

### OSW Expectations

* if active field known, optimize.
* otherwise, conservatively avoid invalid assumptions.

### x64 Patterns

* load/store at base offset 0 with width of chosen field.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — storing incompatible type into union slot (if we enforce)
* **RANE_DIAG_SECURITY_VIOLATION** — union read requires unsafe mode (if enforced)

---

# 8) Procedures (`proc`)

## 8.1 `proc` declaration (core form)

### Surface Grammar

```
<vis?> <export?> <inline?> <async?> <dedicate?> <linear|nonlinear?> proc <Name> <params...> -> <Ret> <requires?>:
  <stmts...>
end
```

Parameters (surface):

```
proc f a i64 b i64 -> i64:
```

### Typed CIL

```
<vis?> <export?> <inline?> <async?> proc <Name>(a: i64, b: i64) -> i64 <requires(...)?> { ... }
```

### Typing Rules

* Parameter types must exist.
* Return statements must match return type.
* If return type is `void`, `return` may omit expression.
* Generic procs require type arguments or inference rules (we show explicit `<i64>`).

### Capability Rules

* A proc has an **effect set**:

  * declared by `requires cap1 cap2 ...`
  * plus any inferred requirements (if we allow inference—our model prefers explicit, but CIAMs may assist).
* Calls are legal only if caller’s effect set ⊇ callee’s effect set.

### Lowering

* **CIAM@parser/AST:** normalize param syntax into `(name:type)` list.
* **CIAM@resolver:** bind symbol IDs, compute proc signature and effect set.
* **CIAM@typed-CIL:** lower body into basic blocks + typed operations.
* **CIAM@frame-planner:** assign stack slots and shadow space plan.
* **CIAM@codegen:** ABI-compliant prologue/epilogue + instruction emission.

### OSW Expectations

* Inlining guided by:

  * `inline`
  * `#pragma profile "hot"`
  * size heuristics
* DCE removes unused private procs.
* Tail recursion elimination (if safe).

### x64 Codegen Patterns (Windows x64)

**Prologue (typical minimal):**

* allocate frame:

  * `sub rsp, FRAME_SIZE`
* preserve nonvolatile regs if used (`rbx, rbp, rsi, rdi, r12-r15`)
* ensure stack alignment

**Args:**

* RCX, RDX, R8, R9 first four
* spill to stack slots if needed

**Return:**

* value in RAX

**Epilogue:**

* restore regs
* `add rsp, FRAME_SIZE`
* `ret`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — bad return type, bad arg types
* **RANE_DIAG_UNDEFINED_NAME** — calling unknown proc
* **RANE_DIAG_SECURITY_VIOLATION** — missing required capability
* **RANE_DIAG_PARSE_ERROR** — malformed proc syntax

---

## 8.2 `async` / `await`

### Surface Grammar

```
async proc f ... requires <caps?>:
  let x = await <expr>
end

await <expr>
```

### Typed CIL

```
async proc f(...) -> T requires(...) {
  let x: U = await g(...);
}
```

### Typing Rules

* `await e` requires `e` to be an awaitable type (e.g., `Task<T>` or compiler-known async result).
* `async proc` returns `T` at surface, but internally becomes `Task<T>` or state machine handle (our surface hides it).

### Capability Rules

* async operations that perform IO must require relevant capabilities.

### Lowering

* **CIAM@typed-CIL:** transform into state machine:

  * state struct storing locals + resume point
  * resume proc dispatch
* **CIAM@OSW:** optimize states, elide stores if no suspension points.

### OSW Expectations

* hoist non-suspending computations out of states
* DCE unused state fields

### x64 Patterns

* state machine call uses:

  * pointers to state struct
  * indirect jump tables or switch dispatch
* awaited call typically becomes:

  * call into runtime scheduler
  * store continuation
  * return to caller until resumed

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — await non-awaitable
* **RANE_DIAG_SECURITY_VIOLATION** — await uses capability not declared
* **RANE_DIAG_PARSE_ERROR** — await used outside async context (if disallowed)

---

## 8.3 `dedicate` + `spawn` + `join`

### Surface Grammar

```
dedicate proc worker iter i64 -> i64 requires threads:
  ...
end

let th = spawn worker 100
let r = join th
```

### Typed CIL

```
dedicate proc worker(iter: i64) -> i64 requires(threads) { ... }

let th = spawn(worker, 100);
let r: i64 = join_i64(th);
```

### Typing Rules

* `spawn f args...` requires `f` be spawnable:

  * no illegal captures (if closures exist later)
  * signature known
* `join` result type must match spawned proc return.

### Capability Rules

* `spawn` requires `threads`.
* `join` requires `threads` (or uses thread handle type requiring it).

### Lowering

* spawn becomes runtime call:

  * `rane_rt_threads.spawn_proc(fptr, args...)`
* join becomes runtime call:

  * `join_i64(handle)` etc.

### OSW Expectations

* inline small worker bodies only if not truly spawned (if we allow “static threads” off)
* otherwise treat spawn/join as effectful barriers

### x64 Patterns

* `spawn` call:

  * set up args in RCX/RDX/R8/R9, reserve shadow space
  * `call [spawn_proc]`
* handle returned in RAX (pointer or integer)

### Diagnostics

* **RANE_DIAG_SECURITY_VIOLATION** — missing threads capability
* **RANE_DIAG_TYPE_MISMATCH** — join type mismatch
* **RANE_DIAG_UNDEFINED_NAME** — unknown worker proc

---

# 9) Statements (Complete Set in Our Syntax)

Below are **every statement keyword/construct we used**, each fully specified.

---

## 9.1 `let`

### Surface Grammar

```
let <name> <Type?> = <expr?>
let <name> = <expr>
let <name> <Type>
```

### Typed CIL

```
let <name>: <Type> = <expr>;
let <name> = <expr>;   // type inferred (if allowed)
```

### Typing Rules

* If type is present, initializer must convert to it.
* If type omitted, inference must succeed (or error).

### Capability Rules

None.

### Lowering

* becomes local slot or virtual register in CIL.
* initializer lowered to expression evaluation + store.

### OSW Expectations

* constant propagation, copy propagation, DCE of unused lets.

### x64 Patterns

* stack slot store:

  * `mov [rsp+off], reg`
* register virtual:

  * keep in physical register if possible

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — init mismatch
* **RANE_DIAG_UNDEFINED_NAME** — init references unknown symbol
* **RANE_DIAG_PARSE_ERROR** — malformed let

---

## 9.2 `set` / `add ... by` (node-style mutation)

### Surface Grammar

```
set <target> <Type?> to <expr>
add <target> by <expr>
```

### Typed CIL

```
<target> = <expr>;
<target> = <target> + <expr>;
```

### Typing Rules

* target must be assignable (lvalue): var, field, index, union field, etc.
* addition requires numeric type or overloaded rule (not shown).

### Capability Rules

None.

### Lowering

* becomes assign operations; field/index become address calculations + store.

### OSW Expectations

* strength reduction and store combining.

### x64 Patterns

* load target → add → store back.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — target/expr type mismatch
* **RANE_DIAG_SECURITY_VIOLATION** — trying to mutate immutable binding (if we enforce immutability)
* **RANE_DIAG_PARSE_ERROR** — invalid target

---

## 9.3 `return`

### Surface Grammar

```
return
return <expr>
```

### Typed CIL

```
return;
return <expr>;
```

### Typing Rules

* Expression type must match proc return.
* If proc return is non-void, `return` must include expr.

### Capability Rules

None.

### Lowering

* terminator instruction in IR; creates exit block.

### OSW Expectations

* tail-call optimization opportunities.
* remove dead code after return.

### x64 Patterns

* move value to RAX (if any), jump to epilogue or emit epilogue directly.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — bad return value type
* **RANE_DIAG_PARSE_ERROR** — return outside proc

---

## 9.4 `if` / `else`

### Surface Grammar

```
if <cond>:
  <stmts>
else:
  <stmts>
end
```

### Typed CIL

```
if (<cond>) { ... } else { ... }
```

### Typing Rules

* `<cond>` must be `bool` (or we must define explicit coercions; safest is bool-only).

### Capability Rules

None.

### Lowering

* produces basic blocks: cond → then/else → merge.

### OSW Expectations

* constant fold conditions, prune branches.
* merge common code.

### x64 Patterns

* compare + conditional jump:

  * `cmp reg, 0`
  * `je else_label`
  * `... then ...`
  * `jmp end_label`
  * `else_label: ...`
  * `end_label:`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — non-bool condition
* **RANE_DIAG_PARSE_ERROR** — missing end/else placement

---

## 9.5 `while`

### Surface Grammar

```
while <cond>:
  <stmts>
end
```

### Typed CIL

```
while (<cond>) { ... }
```

### Typing Rules

* cond must be bool.

### Capability Rules

None.

### Lowering

* loop header block, body block, backedge.

### OSW Expectations

* invariant hoisting, unrolling if instructed, induction variable simplification.

### x64 Patterns

* loop labels + conditional branch back.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — non-bool condition
* **RANE_DIAG_PARSE_ERROR** — malformed while block

---

## 9.6 `for`

### Surface Grammar

```
for let i i64 = <init>; <cond>; i = <step>:
  <stmts>
end
```

### Typed CIL

```
for (let i: i64 = init; cond; i = step) { ... }
```

### Typing Rules

* init must match type.
* cond bool.
* step must assign to loop variable type.

### Capability Rules

None.

### Lowering

* normalize to:

  * init
  * while-loop with step at end

### OSW Expectations

* unrolling, strength reduction, vectorization hints (if we ever add).

### x64 Patterns

* same as while with explicit init/step.

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — init/step mismatch
* **RANE_DIAG_PARSE_ERROR** — malformed for header

---

## 9.7 `match` / `switch` / `decide`

### Surface Grammar

```
match <expr>:
  case <pattern>: <stmts>
  default: <stmts>
end
```

`switch`/`decide` similar but patterns are constants.

### Typed CIL

```
match x { case ...: ... default: ... }
switch x { case 0: ... default: ... }
decide x { case 1: ... default: ... }
```

### Typing Rules

* match patterns must typecheck against scrutinee.
* switch/decide case constants must be same type as scrutinee.

### Capability Rules

None.

### Lowering

* **CIAM@typed-CIL:** build decision IR:

  * for variants: tag tests + payload binds
  * for integers: jump table or compare chain

### OSW Expectations

* choose best lowering:

  * dense integer cases → jump table
  * sparse → compare chain
* constant-fold if scrutinee known.

### x64 Patterns

**Jump table (dense):**

* bounds check + indirect jump:

  * `cmp eax, max`
  * `ja default`
  * `jmp [rip + table + rax*8]`

**Compare chain (sparse):**

* repeated `cmp/jcc`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — wrong case type / invalid pattern
* **RANE_DIAG_PARSE_ERROR** — malformed case/default
* **RANE_DIAG_UNDEFINED_NAME** — unknown variant case

---

## 9.8 `try` / `catch` / `finally` / `throw`

### Surface Grammar

```
try:
  <stmts>
catch e:
  <stmts>
finally:
  <stmts>
end
```

```
throw <expr?>
```

### Typed CIL

```
try { ... } catch (e) { ... } finally { ... }
throw <expr>;
```

### Typing Rules

* thrown value type must match catch variable type (or be `any`/`int` if we define it).
* if no catch, finally still runs.

### Capability Rules

* none inherently, unless throwing uses runtime.

### Lowering

Recommended deterministic lowering:

* represent as explicit error-control edges:

  * try block has exceptional edge to catch
  * finally runs on both normal and exceptional exits

### OSW Expectations

* inline finally if small
* DCE dead catches if never thrown

### x64 Patterns

* If error-return model:

  * return code in register; branch to handler
* If trap model:

  * `ud2` or `int3` for throw without catch (depending)

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — incompatible thrown value/catch type
* **RANE_DIAG_PARSE_ERROR** — missing blocks
* **RANE_DIAG_INTERNAL_ERROR** — lowering inconsistency

---

## 9.9 `with` / `defer`

### Surface Grammar

```
with <expr> as <name>:
  <stmts>
end

defer <expr>
```

### Typed CIL

```
let <name> = <expr>;
try { ... } finally { <cleanup> }
```

### Typing Rules

* `with` resource must be a type that supports cleanup (file handle, etc.).
* `defer expr` must be callable/valid at scope end.

### Capability Rules

* resource acquisition/cleanup often requires `file_io`, etc.

### Lowering

* **CIAM@AST/resolver:** rewrite with → try/finally
* **CIAM@typed-CIL:** insert cleanup call
* **CIAM@OSW:** ensure cleanup always executed on all exits

### OSW Expectations

* cleanup sinking/merging across exits while preserving correctness.

### x64 Patterns

* cleanup block label + jumps from returns to cleanup if needed.

### Diagnostics

* **RANE_DIAG_SECURITY_VIOLATION** — missing capability for open/close
* **RANE_DIAG_TYPE_MISMATCH** — defer expression invalid
* **RANE_DIAG_PARSE_ERROR** — malformed with/defer

---

## 9.10 `asm`

### Surface Grammar

```
asm:
  <asm lines>
end
```

### Typed CIL

```
asm { ... }
```

### Typing Rules

* output bindings must map to declared locals with compatible type widths.

### Capability Rules

* requires `syscalls` in our example; policy may require `unsafe_asm`.

### Lowering

* preserved as inline assembly node until codegen.
* clobbers must be tracked; otherwise, conservatively spill.

### OSW Expectations

* treat asm as barrier: don’t reorder across; don’t assume register values survive unless declared.

### x64 Patterns

* emitted verbatim with operand substitution rules.

### Diagnostics

* **RANE_DIAG_SECURITY_VIOLATION** — asm without permission
* **RANE_DIAG_PARSE_ERROR** — invalid asm syntax
* **RANE_DIAG_INTERNAL_ERROR** — clobber/constraint mismatch

---

## 9.11 `assert`

### Surface Grammar

```
assert <cond> "<msg>"
```

### Typed CIL

```
assert(<cond>, "<msg>");
```

### Typing Rules

* cond must be bool; msg must be string literal (or string).

### Capability Rules

* none if assert traps.
* if assert prints/logs, then requires print capability—prefer trap-only by default.

### Lowering

* cond check → if false trap with code/message id.

### OSW Expectations

* remove asserts in release mode if configured.
* constant-fold asserts.

### x64 Patterns

* `cmp` + `jne ok` + `trap`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — non-bool assert
* **RANE_DIAG_PARSE_ERROR** — missing message

---

# 10) Expressions & Operators (complete)

## 10.1 Literals / variables / field / index

### Surface

* `123`, `0xCAFE_BABE`, `"hello"`, `true`, `null`
* `x`
* `p.x`
* `arr[0]`

### Typed CIL

* same but punctuation explicit.

### Typing Rules

* indexing requires index integer type and array/vector-like type.
* field requires struct type.

### Lowering

* variable → SSA value or stack slot
* field → base + constant offset
* index:

  * arrays: base + index*elem_size
  * bounds checking: optional policy

### x64 Patterns

* array index:

  * `lea rax, [base + idx*scale]`
  * `mov rdx, [rax]`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH** — invalid indexing/field
* **RANE_DIAG_UNDEFINED_NAME** — unknown var

---

## 10.2 Unary operators: `- not ! ~`

### Typing Rules

* `-` numeric
* `not`/`!` bool
* `~` integer

### Lowering

* map to IR unary ops.

### x64 Patterns

* neg: `neg rax`
* not bool: `xor al, 1` (or `cmp/setcc`)
* bitwise not: `not rax`

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH**

---

## 10.3 Binary arithmetic: `+ - * / %`

### Typing Rules

* numeric types; define integer vs float behavior.
* division by zero: either runtime trap or undefined; our assert model suggests trap in safe mode.

### Lowering

* integer div uses `idiv`; float uses `divss/divsd` etc (if we implement SSE path).

### x64 Patterns (integer i64)

* add/sub: `add/sub rax, rbx`
* mul: `imul rax, rbx`
* div/mod:

  * `cqo`
  * `idiv rbx`
  * quotient in RAX, remainder in RDX

### Diagnostics

* **RANE_DIAG_TYPE_MISMATCH**
* **RANE_DIAG_SECURITY_VIOLATION** — if division requires safe-check policy and it’s disabled

---

## 10.4 Bitwise: `& | ^ xor`

### Typing Rules

* integer only.

### x64

* `and/or/xor`

Diagnostics: **RANE_DIAG_TYPE_MISMATCH**

---

## 10.5 Shifts: `shl shr sar << >>`

### Typing Rules

* integer only
* shift amount integer (often masked by width)

### x64

* `shl/shr/sar rax, cl` (shift count in CL) or immediate shift

Diagnostics: **RANE_DIAG_TYPE_MISMATCH**

---

## 10.6 Comparisons: `< <= > >= == !=`

### Typing Rules

* numeric or comparable types
* result `bool`

### x64 Patterns

* `cmp` + `setcc` or branch

Diagnostics: **RANE_DIAG_TYPE_MISMATCH**

---

## 10.7 Logical: `and or && ||`

### Typing Rules

* bool only
* `&&`/`||` short-circuit; `and/or` can be short-circuit too (recommended: yes)

### Lowering

* short-circuit becomes control flow blocks.

### x64 Patterns

* conditional branches controlling evaluation.

Diagnostics: **RANE_DIAG_TYPE_MISMATCH**

---

## 10.8 Ternary: `cond ? a : b`

### Typing Rules

* cond bool
* `a` and `b` must unify to a common type

### Lowering

* branch blocks + phi/select.

### x64 Patterns

* either branches + move, or `cmov` when safe.

Diagnostics:

* **RANE_DIAG_TYPE_MISMATCH**

---

## 10.9 Cast: `as`

### Surface Grammar

```
(<expr> as <Type>)
```

### Typed CIL

```
cast(<expr>, <Type>)
```

### Typing Rules

* define allowed cast lattice:

  * int widening/narrowing
  * float conversions
  * enum repr conversions
  * pointer casts (if we add)
* narrowing may require explicit `as` (we already do explicit).

### x64 Patterns

* widening:

  * `movsx/movzx`
* narrowing:

  * low bits truncation (register move)
* int↔float:

  * `cvtsi2sd`, `cvttsd2si` etc.

Diagnostics:

* **RANE_DIAG_TYPE_MISMATCH** — illegal cast

---

# 11) Collections & Aggregates

## 11.1 Arrays: `[5]i64 = [1 2 3 4 5]`

### Surface Grammar

```
let arr [N]T = [v1 v2 ...]
```

### Typed CIL

```
let arr: [N]T = [v1, v2, ...];
```

Typing:

* exactly N elements convertible to T.

Lowering:

* static initialization to stack or .rdata depending on storage.

x64:

* emit stores into stack slots or copy from constant blob.

Diagnostics:

* **RANE_DIAG_TYPE_MISMATCH** — wrong element count/type

---

## 11.2 `vector`, `map`, tuples `(1 "hi" true)`

These are runtime-backed (require heap in our example).

Capability:

* `vector`/`map` typically require `heap_alloc`.

Lowering:

* calls into runtime constructors; tuple is structural (like anonymous struct).

x64:

* runtime calls through IAT.

Diagnostics:

* **RANE_DIAG_SECURITY_VIOLATION** — missing heap_alloc
* **RANE_DIAG_TYPE_MISMATCH**

---

# 12) Capabilities & Effects (formal rules)

## 12.1 `capability <name>`

Grammar:

* surface: `capability heap_alloc`
* CIL: `capability(heap_alloc);`

Typing:

* introduces a symbol of kind Capability.

Rules:

* A proc has effect set `E`.
* Call is allowed iff `Ecaller ⊇ Ecallee`.

Lowering:

* resolver computes `E` from `requires(...)`
* typed-CIL attaches `E` to proc type.

OSW:

* effect sets limit reordering across effectful ops.

Diagnostics:

* **RANE_DIAG_SECURITY_VIOLATION** — missing capability on proc or at callsite

---

# 13) Contracts & Assertions (complete)

Covered earlier; contracts are declarative predicates, asserts are executable checks.

Diagnostics:

* **RANE_DIAG_TYPE_MISMATCH**, **RANE_DIAG_SECURITY_VIOLATION**, **RANE_DIAG_PARSE_ERROR**

---

# 14) Macros & Templates (complete)

## 14.1 `macro`

* pre-typed rewrite
* CIAM-driven expansion

Diagnostics:

* **RANE_DIAG_PARSE_ERROR** — bad macro form
* **RANE_DIAG_INTERNAL_ERROR** — macro expansion cycle

## 14.2 Generic procs (`proc identity<T>`)

* monomorphization at typed stage

Diagnostics:

* **RANE_DIAG_TYPE_MISMATCH** — cannot infer or invalid instantiation

---

# 15) Concurrency (complete constructs)

* `mutex`
* `channel<T>`
* `send`
* `recv`
* `spawn`
* `join`
* `lock`
* `async/await`

All covered above; the universal rule:

* missing capability → **RANE_DIAG_SECURITY_VIOLATION**

---

# 16) Resource Management (complete)

* `with`
* `defer`
* `open`, `close`, `write`, `read` (runtime symbols)
* `finally` lowering guarantees cleanup

---

# 17) Systems & Low-Level (complete)

## 17.1 `mmio region`

Covered.

## 17.2 `addr/load/store`

Covered.

## 17.3 `asm`

Covered.

---

# 18) Exceptions (complete)

Covered.

---

# 19) `eval` (complete)

Grammar:

* surface: `eval "10 + " + x`
* CIL: `eval("10 + " + x)`

Typing:

* returns declared type (e.g., `i64`) only if runtime guarantees; otherwise must return `any` and cast.
* Our syntax assumes typed eval returning i64; that implies eval API is typed or validated.

Capability:

* requires `dynamic_eval`.

Lowering:

* runtime call.

OSW:

* treat as effectful barrier, no reordering.

x64:

* IAT call.

Diagnostics:

* **RANE_DIAG_SECURITY_VIOLATION** — missing dynamic_eval
* **RANE_DIAG_TYPE_MISMATCH** — eval result not convertible

---

# 20) Symbols: `#name`

Grammar:

* `#IDENT`

Typing:

* type `symbol` or `u64` (implementation-defined)
* must refer to known symbol.

Lowering:

* resolved into symbol ID integer.

x64:

* immediate constant.

Diagnostics:

* **RANE_DIAG_UNDEFINED_NAME** — unknown symbol literal

---

# 21) Node Graph Programming (complete)

Keywords:

* `node`
* `start at node`
* `go to node`
* `say`
* `halt`

Typing:

* nodes are CFG blocks
* `say` requires print capability or is intrinsic mapped to print runtime

Lowering:

* node graph becomes basic blocks in CIL/OSW.

x64:

* labels + jumps; `halt` terminator.

Diagnostics:

* **RANE_DIAG_PARSE_ERROR** — missing node end
* **RANE_DIAG_UNDEFINED_NAME** — jumping to unknown node
* **RANE_DIAG_SECURITY_VIOLATION** — say/print without capability

---

# 22) Control Transfer (complete)

Keywords:

* `goto`
* `label`
* `trap`
* `halt`

Typing:

* goto condition must be bool (if conditional form).
* label names unique.

Lowering:

* direct CFG edges.

x64:

* `jmp`, `jcc`
* `trap` → `ud2` / `int3` / call abort stub
* `halt` → exit path or infinite stop (policy)

Diagnostics:

* **RANE_DIAG_PARSE_ERROR**
* **RANE_DIAG_TYPE_MISMATCH**
* **RANE_DIAG_UNDEFINED_NAME**

---

# 23) Pragmas & Defines (complete)

Keywords:

* `#pragma unroll 4`
* `#pragma profile "hot"`
* `pragma "optimize" "speed"`
* `pragma "lto" "on"`
* `pragma "scheduling" "fair"`
* `define BUILD_ID 0xDEADBEEF`

Typing:

* pragmas affect compilation, not runtime typing.
* define introduces constant macro (compile-time).

Lowering:

* CIAM@OSW consumes optimization pragmas and tags blocks.

Diagnostics:

* **RANE_DIAG_PARSE_ERROR** — invalid pragma args
* **RANE_DIAG_SECURITY_VIOLATION** — forbidden pragma by policy

---

# 24) Backend Reference: Frame Planner & Windows x64 ABI (required patterns)

## 24.1 Call-site template (must obey)

At any call:

* allocate shadow space (32 bytes)
* align stack to 16
* place args in regs then spill remainder

**Pattern (conceptual):**

* `sub rsp, shadow+align+temps`
* set RCX/RDX/R8/R9
* `call target`
* `add rsp, shadow+align+temps`

Diagnostics:

* **RANE_DIAG_INTERNAL_ERROR** — if planner fails alignment
* backend verification failures should map to **RANE_DIAG_INTERNAL_ERROR** with details

---

# 25) Diagnostics Model (how errors are reported)

We already have a `rane_diag_t` style system; the manual defines required codes:

* **RANE_DIAG_PARSE_ERROR**
* **RANE_DIAG_UNDEFINED_NAME**
* **RANE_DIAG_REDECLARED_NAME**
* **RANE_DIAG_TYPE_MISMATCH**
* **RANE_DIAG_SECURITY_VIOLATION**
* **RANE_DIAG_INTERNAL_ERROR**

Each diagnostic must include:

* span (line/col/len)
* message (human)
* optionally: notes (expected tokens, candidate overloads, missing capability chain)

---

**********

## Appendix A — Keyword Index (A→Z)

> **Format per entry**

* **Surface token:** exact token as written in `.rane`
* **Typed CIL token:** exact canonical CIL token/spelling
* **AST node kind:** canonical node name (recommended)
* **Typed CIL op kind:** canonical IR opcode or directive
* **Effects / capabilities:** how it interacts with `requires(...)`
* **OSW hooks:** which optimizations typically touch it
* **x64 lowering snippet:** representative Windows x64 emission shape (pseudo-asm)
* **Canonical errors:** typical diagnostics (codes + meaning)

---

### `@derive`

* **Surface token:** `@derive`
* **Typed CIL token:** `@derive(...)`
* **AST node kind:** `AttrDerive`
* **Typed CIL op kind:** `CIL_ATTR_DERIVE` → expands into generated decls
* **Effects / capabilities:** must not introduce new effects unless explicit policy allows
* **OSW hooks:** inliner/DCE remove unused generated helpers
* **x64 lowering snippet:** none directly (affects generated code only)
* **Canonical errors:**

  * `RANE_DIAG_UNDEFINED_NAME` unknown derive
  * `RANE_DIAG_TYPE_MISMATCH` derive constraints not satisfied
  * `RANE_DIAG_SECURITY_VIOLATION` derive attempted effectful generation

---

### `add`

* **Surface token:** `add` (as in `add h.size by 512`)
* **Typed CIL token:** none special (lowers to assignment)
* **AST node kind:** `AddAssignStmt(target, expr)`
* **Typed CIL op kind:** `ADD` + `STORE` (or `ADD_ASSIGN`)
* **Effects / capabilities:** none
* **OSW hooks:** strength reduction, store-combining, constant fold
* **x64 lowering snippet:**

  * `mov rax, [addr]`
  * `add rax, imm/reg`
  * `mov [addr], rax`
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` non-numeric add / width mismatch
  * `RANE_DIAG_SECURITY_VIOLATION` target immutable (if enforced)
  * `RANE_DIAG_PARSE_ERROR` invalid target grammar

---

### `addr`

* **Surface token:** `addr` (as in `let p0 = addr 4096 4 8 16`)
* **Typed CIL token:** `addr(...)`
* **AST node kind:** `AddrExpr(base, a, b, c)` (our 4-arg shape)
* **Typed CIL op kind:** `ADDR_CALC`
* **Effects / capabilities:** none (pure address arithmetic) unless policy gates raw pointers
* **OSW hooks:** constant-fold, CSE, fold into load/store addressing modes
* **x64 lowering snippet:**

  * `mov rax, base_imm`
  * `add rax, off_imm` (folded as `lea` when possible)
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` non-integer components
  * `RANE_DIAG_SECURITY_VIOLATION` raw address ops disallowed by policy (optional)

---

### `admin`

* **Surface token:** `admin`
* **Typed CIL token:** `admin`
* **AST node kind:** `Visibility(Admin)`
* **Typed CIL op kind:** `CIL_VIS_ADMIN`
* **Effects / capabilities:** may be allowed to require restricted capabilities (policy)
* **OSW hooks:** none semantic; may affect export/linkage rules
* **x64 lowering snippet:** none (symbol visibility)
* **Canonical errors:**

  * `RANE_DIAG_SECURITY_VIOLATION` admin-only symbol accessed
  * `RANE_DIAG_PARSE_ERROR` invalid placement

---

### `alias`

* **Surface token:** `alias`
* **Typed CIL token:** `alias`
* **AST node kind:** `AliasDecl(name, type)`
* **Typed CIL op kind:** `CIL_ALIAS`
* **Effects / capabilities:** none
* **OSW hooks:** type alias erasure before codegen
* **x64 lowering snippet:** none (representation-only)
* **Canonical errors:**

  * `RANE_DIAG_REDECLARED_NAME` alias conflicts
  * `RANE_DIAG_UNDEFINED_NAME` target type missing

---

### `allocate`

* **Surface token:** `allocate`
* **Typed CIL token:** `allocate(T, n)`
* **AST node kind:** `AllocateExpr(type, count)`
* **Typed CIL op kind:** `HEAP_ALLOC`
* **Effects / capabilities:** requires `heap_alloc`
* **OSW hooks:** escape analysis (stack vs heap if permitted), DCE if unused
* **x64 lowering snippet:**

  * args → `RCX=size`, `RDX=align` (example)
  * `sub rsp, 40h` (shadow+align)
  * `call [__imp_rane_rt_alloc]`
* **Canonical errors:**

  * `RANE_DIAG_SECURITY_VIOLATION` missing `heap_alloc`
  * `RANE_DIAG_TYPE_MISMATCH` invalid count type
  * `RANE_DIAG_INTERNAL_ERROR` allocator ABI mismatch

---

### `and`

* **Surface token:** `and` (logical)
* **Typed CIL token:** `and`
* **AST node kind:** `LogicalAndExpr(lhs, rhs)` (short-circuit recommended)
* **Typed CIL op kind:** `AND_SC` (short-circuit) or `AND_BOOL`
* **Effects / capabilities:** if short-circuit, RHS effects are conditional
* **OSW hooks:** boolean simplification, branch pruning
* **x64 lowering snippet (short-circuit):**

  * `cmp al, 0`
  * `je false_label`
  * evaluate rhs → result
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` operands not bool

---

### `asm`

* **Surface token:** `asm: ... end`
* **Typed CIL token:** `asm { ... }`
* **AST node kind:** `AsmStmt(lines, outputs?, clobbers?)`
* **Typed CIL op kind:** `INLINE_ASM`
* **Effects / capabilities:** requires `syscalls` in our usage (policy may require `unsafe_asm`)
* **OSW hooks:** barrier (no reordering across), must model clobbers
* **x64 lowering snippet:** emitted verbatim (with operand substitution)
* **Canonical errors:**

  * `RANE_DIAG_SECURITY_VIOLATION` asm not permitted
  * `RANE_DIAG_PARSE_ERROR` invalid asm line
  * `RANE_DIAG_INTERNAL_ERROR` clobber bookkeeping failure

---

### `assert`

* **Surface token:** `assert`
* **Typed CIL token:** `assert(cond, msg)`
* **AST node kind:** `AssertStmt(cond, msg)`
* **Typed CIL op kind:** `ASSERT`
* **Effects / capabilities:** none if assert → `trap`; if logging, would require print capability
* **OSW hooks:** remove in release mode; constant-fold asserts
* **x64 lowering snippet:**

  * `test al, al`
  * `jne ok`
  * `ud2` (or call trap stub)
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` cond not bool
  * `RANE_DIAG_PARSE_ERROR` missing message

---

### `async`

* **Surface token:** `async`
* **Typed CIL token:** `async`
* **AST node kind:** `ProcQualifier(Async)`
* **Typed CIL op kind:** `CIL_PROC_ASYNC` + `AWAIT` ops in body
* **Effects / capabilities:** async itself doesn’t add effects; awaited ops may
* **OSW hooks:** async state machine optimization, field DCE, resume-path simplification
* **x64 lowering snippet:** state struct + dispatcher:

  * `mov rcx, state_ptr`
  * `call resume_proc`
* **Canonical errors:**

  * `RANE_DIAG_PARSE_ERROR` `await` outside async (if enforced)
  * `RANE_DIAG_TYPE_MISMATCH` await non-awaitable

---

### `await`

* **Surface token:** `await`
* **Typed CIL token:** `await`
* **AST node kind:** `AwaitExpr(expr)`
* **Typed CIL op kind:** `AWAIT`
* **Effects / capabilities:** effects come from awaited call; may require `network_io`, etc.
* **OSW hooks:** suspension-point analysis, spilling minimization
* **x64 lowering snippet:** typically:

  * call scheduler/runtime to register continuation
  * store resume state
  * return/yield
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` non-awaitable
  * `RANE_DIAG_SECURITY_VIOLATION` missing capability for awaited operation

---

### `borrow`

* **Surface token:** `borrow`
* **Typed CIL token:** `borrow(x)`
* **AST node kind:** `BorrowExpr(value)`
* **Typed CIL op kind:** `BORROW_REF`
* **Effects / capabilities:** none; interacts with ownership/linearity rules
* **OSW hooks:** borrow elision, load forwarding
* **x64 lowering snippet:** often no-op (same pointer) with lifetime tracking only
* **Canonical errors:**

  * `RANE_DIAG_SECURITY_VIOLATION` borrow after free / violates linearity
  * `RANE_DIAG_TYPE_MISMATCH` borrow non-addressable value

---

### `capability`

* **Surface token:** `capability`
* **Typed CIL token:** `capability(name);`
* **AST node kind:** `CapabilityDecl(name)`
* **Typed CIL op kind:** `CIL_CAP_DECL`
* **Effects / capabilities:** defines a capability symbol
* **OSW hooks:** none
* **x64 lowering snippet:** none
* **Canonical errors:**

  * `RANE_DIAG_REDECLARED_NAME` duplicate capability

---

### `case`

* **Surface token:** `case`
* **Typed CIL token:** `case`
* **AST node kind:** `CaseArm(pattern/const, block)`
* **Typed CIL op kind:** `SWITCH_CASE` / `MATCH_CASE`
* **Effects / capabilities:** conditional execution; capability checks apply within arm bodies
* **OSW hooks:** case clustering, jump-table selection, dead-arm elimination
* **x64 lowering snippet:** `cmp/jcc` chain or jump table
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` case type mismatch
  * `RANE_DIAG_PARSE_ERROR` duplicate/default misuse

---

### `catch`

* **Surface token:** `catch`
* **Typed CIL token:** `catch (e)`
* **AST node kind:** `CatchClause(name, block)`
* **Typed CIL op kind:** `TRY_CATCH`
* **Effects / capabilities:** none inherent
* **OSW hooks:** DCE unused catch; simplify try regions
* **x64 lowering snippet:** error-edge branch to handler label
* **Canonical errors:**

  * `RANE_DIAG_PARSE_ERROR` catch without try
  * `RANE_DIAG_TYPE_MISMATCH` invalid catch binder type (if typed)

---

### `channel`

* **Surface token:** `channel<int>` / `channel<int> ch`
* **Typed CIL token:** `channel<int> ch;`
* **AST node kind:** `ChannelDecl(T, name)`
* **Typed CIL op kind:** `CIL_CHAN_DECL`
* **Effects / capabilities:** requires `channels` for send/recv ops
* **OSW hooks:** treat send/recv as synchronization barriers
* **x64 lowering snippet:** channel ops call runtime:

  * `call [__imp_rane_rt_channels_send]`
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` channel type param invalid
  * `RANE_DIAG_SECURITY_VIOLATION` use without `channels`

---

### `choose`

* **Surface token:** `choose` (as in `choose max a b`)
* **Typed CIL token:** typically lowered to `call rane_rt_math.max_i64(...)` or intrinsic `SELECT_MAX`
* **AST node kind:** `ChooseExpr(kind, a, b)`
* **Typed CIL op kind:** `SELECT` (or `CALL_INTRINSIC`)
* **Effects / capabilities:** none (pure)
* **OSW hooks:** constant fold, strength reduce, convert to `cmov` when safe
* **x64 lowering snippet (cmov form):**

  * `mov rax, a`
  * `cmp a, b`
  * `cmovl rax, b` (for max/min variant)
* **Canonical errors:**

  * `RANE_DIAG_TYPE_MISMATCH` non-comparable operands
  * `RANE_DIAG_PARSE_ERROR` unknown choose mode

---

### `const`

* **Surface token:** `const`
* **Typed CIL token:** `const`
* **AST node kind:** `ConstDecl(name, type, expr)`
* **Typed CIL op kind:** `CIL_CONST`
* **Effects / capabilities:** initializer must be effect-free or compile-time resolvable
* **OSW hooks:** const propagation, DCE
* **x64 lowering snippet:** embed immediate or `.rdata`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`, `RANE_DIAG_SECURITY_VIOLATION`

---

### `constexpr`

* **Surface token:** `constexpr`
* **Typed CIL token:** `constexpr`
* **AST node kind:** `ConstexprDecl(...)`
* **Typed CIL op kind:** `CIL_CONSTEXPR`
* **Effects / capabilities:** must be compile-time evaluable; no effectful calls
* **OSW hooks:** fold to literal
* **x64 lowering snippet:** none (literal inlined)
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` constexpr uses effect/runtime

---

### `consteval`

* **Surface token:** `consteval`
* **Typed CIL token:** `consteval`
* **AST node kind:** `ProcQualifier(Consteval)`
* **Typed CIL op kind:** `CIL_PROC_CONSTEVAL`
* **Effects / capabilities:** must be pure/compile-time only
* **OSW hooks:** replace calls with literals, prune unused consteval bodies
* **x64 lowering snippet:** none for evaluated calls
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION`, `RANE_DIAG_INTERNAL_ERROR`

---

### `constinit`

* **Surface token:** `constinit`
* **Typed CIL token:** `constinit`
* **AST node kind:** `ConstinitDecl(...)`
* **Typed CIL op kind:** `CIL_CONSTINIT`
* **Effects / capabilities:** no runtime effects allowed
* **OSW hooks:** treat as static init data
* **x64 lowering snippet:** `.data/.rdata` placement
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` initializer not static-safe

---

### `contract`

* **Surface token:** `contract`
* **Typed CIL token:** `contract`
* **AST node kind:** `ContractDecl(name, params, ensures)`
* **Typed CIL op kind:** `CIL_CONTRACT`
* **Effects / capabilities:** must not introduce effects; predicates must be pure
* **OSW hooks:** can be used as optimization assumptions if enabled
* **x64 lowering snippet:** optional runtime checks or erased
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` ensures not bool / references unknowns

---

### `decide`

* **Surface token:** `decide`
* **Typed CIL token:** `decide`
* **AST node kind:** `DecideStmt(expr, arms, default)`
* **Typed CIL op kind:** `DECIDE` (lowers like switch)
* **Effects / capabilities:** none
* **OSW hooks:** jump-table selection, arm pruning
* **x64 lowering snippet:** same as switch (jump table / cmp chain)
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`, `RANE_DIAG_PARSE_ERROR`

---

### `dedicate`

* **Surface token:** `dedicate`
* **Typed CIL token:** `dedicate`
* **AST node kind:** `ProcQualifier(Dedicate)`
* **Typed CIL op kind:** `CIL_PROC_DEDICATE`
* **Effects / capabilities:** commonly paired with `requires threads`
* **OSW hooks:** treat as spawn target; reduce capture/escape assumptions
* **x64 lowering snippet:** normal proc; spawn path calls runtime
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` spawned without threads

---

### `defer`

* **Surface token:** `defer`
* **Typed CIL token:** lowered (usually no direct token) → `try/finally`
* **AST node kind:** `DeferStmt(expr)`
* **Typed CIL op kind:** `DEFER` (then lowered to `FINALLY`)
* **Effects / capabilities:** expr may require capabilities; must be valid at scope exit
* **OSW hooks:** cleanup merging; ensure runs on all exits
* **x64 lowering snippet:** jumps to cleanup label on return paths
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`, `RANE_DIAG_SECURITY_VIOLATION`

---

### `default`

* **Surface token:** `default`
* **Typed CIL token:** `default`
* **AST node kind:** `DefaultArm(block)`
* **Typed CIL op kind:** `SWITCH_DEFAULT`
* **Effects / capabilities:** none
* **OSW hooks:** ensure total coverage; eliminate unreachable default
* **x64 lowering snippet:** `jmp default_label`
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` duplicate default

---

### `else`

* **Surface token:** `else`
* **Typed CIL token:** `else`
* **AST node kind:** `ElseClause(block)`
* **Typed CIL op kind:** part of `IF`
* **Effects / capabilities:** none
* **OSW hooks:** branch pruning/merge
* **x64 lowering snippet:** `je else_label`
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` else without if

---

### `end`

* **Surface token:** `end`
* **Typed CIL token:** `}` (conceptual)
* **AST node kind:** block terminator (not a node)
* **Typed CIL op kind:** none
* **Effects / capabilities:** none
* **OSW hooks:** none
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` missing/extra `end`

---

### `enum`

* **Surface token:** `enum`
* **Typed CIL token:** `enum`
* **AST node kind:** `EnumDecl(name, repr, cases)`
* **Typed CIL op kind:** `CIL_ENUM`
* **Effects / capabilities:** none
* **OSW hooks:** switch lowering improvements
* **x64 lowering snippet:** integer constants
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` value out of range

---

### `ensures`

* **Surface token:** `ensures`
* **Typed CIL token:** `ensures(...)`
* **AST node kind:** `ContractEnsures(expr)`
* **Typed CIL op kind:** `CIL_ENSURES`
* **Effects / capabilities:** predicate must be pure
* **OSW hooks:** optional assumption-based optimization
* **x64 lowering snippet:** optional checks or erased
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` ensures non-bool

---

### `eval`

* **Surface token:** `eval`
* **Typed CIL token:** `eval(...)`
* **AST node kind:** `EvalExpr(stringExpr)`
* **Typed CIL op kind:** `DYNAMIC_EVAL`
* **Effects / capabilities:** requires `dynamic_eval`
* **OSW hooks:** barrier; no reordering; no constant fold unless literal and evaluator enabled
* **x64 lowering snippet:** runtime call via IAT
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing `dynamic_eval`

---

### `export`

* **Surface token:** `export`
* **Typed CIL token:** `export`
* **AST node kind:** `ExportQualifier`
* **Typed CIL op kind:** `CIL_EXPORT`
* **Effects / capabilities:** none
* **OSW hooks:** affects visibility/inlining across modules
* **x64 lowering snippet:** affects export tables/mangling
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` invalid placement

---

### `f32` `f64` `f128`

* **Surface token:** `f32`, `f64`, `f128`
* **Typed CIL token:** same
* **AST node kind:** `TypeName("f64")`
* **Typed CIL op kind:** `TY_F64` etc.
* **Effects / capabilities:** none
* **OSW hooks:** constant fold, choose SSE/FP lowering
* **x64 lowering snippet:** FP ops via XMM (`addsd`, `mulsd`, etc.) when implemented
* **Canonical errors:** `RANE_DIAG_UNDEFINED_NAME` if type not registered

---

### `false`

* **Surface token:** `false`
* **Typed CIL token:** `false`
* **AST node kind:** `BoolLit(false)`
* **Typed CIL op kind:** `CONST_BOOL false`
* **Effects / capabilities:** none
* **OSW hooks:** branch pruning
* **x64 lowering snippet:** `xor eax, eax`
* **Canonical errors:** none

---

### `finally`

* **Surface token:** `finally`
* **Typed CIL token:** `finally`
* **AST node kind:** `FinallyClause(block)`
* **Typed CIL op kind:** `TRY_FINALLY`
* **Effects / capabilities:** executes on all exits; capability checks apply inside
* **OSW hooks:** cleanup merge, eliminate redundant finally
* **x64 lowering snippet:** dedicated cleanup label, jumps from exits
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` finally without try

---

### `for`

* **Surface token:** `for`
* **Typed CIL token:** `for (...) { ... }`
* **AST node kind:** `ForStmt(init, cond, step, body)`
* **Typed CIL op kind:** `FOR` lowered to CFG blocks (`BR`, `JMP`)
* **Effects / capabilities:** none
* **OSW hooks:** loop opts, unroll, IV simplification
* **x64 lowering snippet:** loop header/body/backedge with `cmp/jcc`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` cond not bool

---

### `free`

* **Surface token:** `free`
* **Typed CIL token:** `free(ptr)`
* **AST node kind:** `FreeStmt(expr)`
* **Typed CIL op kind:** `HEAP_FREE`
* **Effects / capabilities:** requires `heap_alloc`
* **OSW hooks:** DCE redundant frees (only if provably safe), lifetime shortening
* **x64 lowering snippet:** runtime call to free
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing heap_alloc, double-free detection (if enforced)

---

### `from`

* **Surface token:** `from` (in `mmio region REG from 4096 size 256`)
* **Typed CIL token:** `from`
* **AST node kind:** part of `MmioRegionDecl`
* **Typed CIL op kind:** `CIL_MMIO_REGION`
* **Effects / capabilities:** none to declare; access may be gated by `syscalls` or policy
* **OSW hooks:** constant-fold addresses
* **x64 lowering snippet:** declaration none
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` invalid region spec

---

### `generic` (implied by `template T` / `proc identity<T>`)

* **Surface token:** `template` / `<T>`
* **Typed CIL token:** `template <T>`
* **AST node kind:** `TypeParamList([T])`
* **Typed CIL op kind:** `GENERIC_PROC` (monomorphization)
* **Effects / capabilities:** effect set per instantiation
* **OSW hooks:** specialize/inlining per instantiation
* **x64 lowering snippet:** per-monomorphized proc normal lowering
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` cannot infer `T`

---

### `go`

* **Surface token:** `go` (in `go to node end_node`)
* **Typed CIL token:** typically lowered to `goto end_node;`
* **AST node kind:** `GotoNodeStmt(targetNode)`
* **Typed CIL op kind:** `JMP`
* **Effects / capabilities:** none
* **OSW hooks:** CFG simplification
* **x64 lowering snippet:** `jmp end_node`
* **Canonical errors:** `RANE_DIAG_UNDEFINED_NAME` unknown node

---

### `goto`

* **Surface token:** `goto`
* **Typed CIL token:** `goto`
* **AST node kind:** `GotoStmt(cond, tLabel, fLabel)` (our ternary goto form)
* **Typed CIL op kind:** `BR_COND` or `JMP`
* **Effects / capabilities:** none
* **OSW hooks:** branch simplification, jump threading
* **x64 lowering snippet:**

  * evaluate cond
  * `cmp al, 0`
  * `jne L_true`
  * `jmp L_false`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` cond not bool, labels undefined

---

### `halt`

* **Surface token:** `halt`
* **Typed CIL token:** `halt;`
* **AST node kind:** `HaltStmt`
* **Typed CIL op kind:** `HALT` (terminator)
* **Effects / capabilities:** none
* **OSW hooks:** unreachable pruning, DCE after halt
* **x64 lowering snippet:** policy-defined:

  * `ud2` OR `jmp $` OR call `ExitProcess`
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` halt outside valid context (if enforced)

---

### `if`

* **Surface token:** `if`
* **Typed CIL token:** `if (...)`
* **AST node kind:** `IfStmt(cond, then, else?)`
* **Typed CIL op kind:** `IF` lowered to `BR_COND`
* **Effects / capabilities:** conditional effects; capability checks per branch
* **OSW hooks:** constant fold, branch prune
* **x64 lowering snippet:** `cmp/test` + `jcc`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` cond not bool

---

### `import`

* **Surface token:** `import`
* **Typed CIL token:** `import ...;`
* **AST node kind:** `ImportDecl(path, symbols?)`
* **Typed CIL op kind:** `CIL_IMPORT`
* **Effects / capabilities:** none
* **OSW hooks:** unused import elimination
* **x64 lowering snippet:** import table / IAT entries
* **Canonical errors:** `RANE_DIAG_UNDEFINED_NAME` import target missing

---

### `inline`

* **Surface token:** `inline`
* **Typed CIL token:** `inline`
* **AST node kind:** `ProcQualifier(Inline)`
* **Typed CIL op kind:** `CIL_PROC_INLINE`
* **Effects / capabilities:** none
* **OSW hooks:** inliner uses as strong hint
* **x64 lowering snippet:** none directly
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` invalid placement

---

### `int` / `i8/i16/i32/i64/i128/i512` / `u8..u512` / `bool` / `void` / `string`

* **Surface token:** type keywords as shown
* **Typed CIL token:** same
* **AST node kind:** `TypeName(...)`
* **Typed CIL op kind:** `TY_*`
* **Effects / capabilities:** none
* **OSW hooks:** width-driven instruction selection, constant folding
* **x64 lowering snippet:** integer ops use GP regs; bool uses `al`/flags; string uses pointer+len (recommended ABI)
* **Canonical errors:** `RANE_DIAG_UNDEFINED_NAME` if type not registered

---

### `label`

* **Surface token:** `label`
* **Typed CIL token:** `label`
* **AST node kind:** `LabelStmt(name)`
* **Typed CIL op kind:** `LABEL`
* **Effects / capabilities:** none
* **OSW hooks:** jump threading, unreachable elimination
* **x64 lowering snippet:** `L_name:`
* **Canonical errors:** `RANE_DIAG_REDECLARED_NAME` label duplicate

---

### `let`

* **Surface token:** `let`
* **Typed CIL token:** `let`
* **AST node kind:** `LetStmt(name, type?, init?)`
* **Typed CIL op kind:** `LOCAL` + `STORE` (init) / `ALLOC_LOCAL`
* **Effects / capabilities:** none
* **OSW hooks:** const/copy prop, DCE, regalloc friendliness
* **x64 lowering snippet:** allocate stack slot or keep in reg
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`, `RANE_DIAG_UNDEFINED_NAME`

---

### `linear`

* **Surface token:** `linear`
* **Typed CIL token:** `linear`
* **AST node kind:** `ProcQualifier(Linear)`
* **Typed CIL op kind:** `CIL_PROC_LINEAR`
* **Effects / capabilities:** enforces linear resource usage rules in body
* **OSW hooks:** prevents unsafe duplication; enables lifetime tightening
* **x64 lowering snippet:** none (static discipline)
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` linearity violated (use-after-move, double-free)

---

### `load`

* **Surface token:** `load` (as in `load u32 addr ...`)
* **Typed CIL token:** `load(u32, addr(...))`
* **AST node kind:** `LoadExpr(type, address)`
* **Typed CIL op kind:** `LOAD`
* **Effects / capabilities:** typically requires policy allowing raw memory; mmio loads treated volatile
* **OSW hooks:** load CSE, load forwarding, alias analysis gating
* **x64 lowering snippet:**

  * compute address → `rax`
  * `mov eax, [rax]`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` bad address type, `RANE_DIAG_SECURITY_VIOLATION` disallowed raw load

---

### `lock`

* **Surface token:** `lock` (block form)
* **Typed CIL token:** typically lowered to `try/finally` with `mutex_lock/unlock`
* **AST node kind:** `LockStmt(mutexExpr, body)`
* **Typed CIL op kind:** `MUTEX_LOCK` / `MUTEX_UNLOCK`
* **Effects / capabilities:** requires `threads`
* **OSW hooks:** treat lock/unlock as barriers; cannot reorder across
* **x64 lowering snippet:** runtime calls:

  * `call [mutex_lock]` … `call [mutex_unlock]`
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing threads, invalid mutex type

---

### `macro`

* **Surface token:** `macro`
* **Typed CIL token:** `macro`
* **AST node kind:** `MacroDecl(name, params, body)`
* **Typed CIL op kind:** `CIL_MACRO` (expanded before CIL ops)
* **Effects / capabilities:** expansion must preserve capability rules; cannot conjure effects silently
* **OSW hooks:** none (macros gone by OSW)
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR`, `RANE_DIAG_INTERNAL_ERROR` expansion cycle

---

### `map`

* **Surface token:** `map`
* **Typed CIL token:** `map(...)`
* **AST node kind:** `MapLiteralExpr(entries)`
* **Typed CIL op kind:** `RT_MAP_NEW` + `RT_MAP_PUT`
* **Effects / capabilities:** requires `heap_alloc` (and possibly `crypto` if hashed maps are policy-gated)
* **OSW hooks:** constant-fold literal maps only if immutable + compile-time supported
* **x64 lowering snippet:** runtime constructor calls
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing heap_alloc

---

### `match`

* **Surface token:** `match`
* **Typed CIL token:** `match`
* **AST node kind:** `MatchStmt(expr, cases, default?)`
* **Typed CIL op kind:** `MATCH` → lowered to `SWITCH`/decision tree
* **Effects / capabilities:** none
* **OSW hooks:** decision tree optimize, arm pruning
* **x64 lowering snippet:** tag test + branches
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` invalid pattern

---

### `mmio`

* **Surface token:** `mmio` (region + ops `read32` / `write32`)
* **Typed CIL token:** `mmio region ...;` and `read32`, `write32`
* **AST node kind:** `MmioRegionDecl`, `MmioReadExpr`, `MmioWriteStmt`
* **Typed CIL op kind:** `MMIO_REGION`, `MMIO_READ32`, `MMIO_WRITE32`
* **Effects / capabilities:** typically requires `syscalls` (or a dedicated `mmio` capability)
* **OSW hooks:** treat as volatile; forbid reordering; insert barriers if required
* **x64 lowering snippet:**

  * `mov eax, dword ptr [abs_addr]` (or `mov eax, [rax]`) with volatility constraints
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` mmio not permitted; `RANE_DIAG_PARSE_ERROR` invalid region spec

---

### `module`

* **Surface token:** `module`
* **Typed CIL token:** `module ...;`
* **AST node kind:** `ModuleDecl`
* **Typed CIL op kind:** `CIL_MODULE`
* **Effects / capabilities:** none
* **OSW hooks:** LTO boundary
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` duplicate module

---

### `mutex`

* **Surface token:** `mutex`
* **Typed CIL token:** `mutex m1;`
* **AST node kind:** `MutexDecl(name)`
* **Typed CIL op kind:** `CIL_MUTEX_DECL`
* **Effects / capabilities:** operations require `threads`
* **OSW hooks:** lock barrier modeling
* **x64 lowering snippet:** runtime handle setup or static storage
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` bad decl

---

### `mutate`

* **Surface token:** `mutate` (as in `mutate p[0] to 10`)
* **Typed CIL token:** assignment form
* **AST node kind:** `MutateStmt(target, expr)`
* **Typed CIL op kind:** `STORE`
* **Effects / capabilities:** interacts with ownership; mutation may be disallowed for immutable bindings
* **OSW hooks:** store combine; alias analysis
* **x64 lowering snippet:** compute address → store
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` mutate immutable; `RANE_DIAG_TYPE_MISMATCH`

---

### `namespace`

* **Surface token:** `namespace`
* **Typed CIL token:** `namespace {...}`
* **AST node kind:** `NamespaceDecl`
* **Typed CIL op kind:** `CIL_NAMESPACE`
* **Effects / capabilities:** none
* **OSW hooks:** none
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_REDECLARED_NAME`

---

### `node`

* **Surface token:** `node`
* **Typed CIL token:** `node` (or lowered to labels + CFG)
* **AST node kind:** `NodeDecl(name, body)`
* **Typed CIL op kind:** `CFG_BLOCK`
* **Effects / capabilities:** depends on body
* **OSW hooks:** CFG simplify, DCE
* **x64 lowering snippet:** label + code + terminators
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` malformed node, `RANE_DIAG_UNDEFINED_NAME` bad transitions

---

### `nonlinear`

* **Surface token:** `nonlinear`
* **Typed CIL token:** `nonlinear`
* **AST node kind:** `ProcQualifier(Nonlinear)`
* **Typed CIL op kind:** `CIL_PROC_NONLINEAR`
* **Effects / capabilities:** relaxes linearity constraints (if our system treats this as opt-out)
* **OSW hooks:** fewer restrictions; still must be safe
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` if nonlinear prohibited in policy zones

---

### `not`

* **Surface token:** `not`
* **Typed CIL token:** `not`
* **AST node kind:** `UnaryNotExpr`
* **Typed CIL op kind:** `NOT_BOOL`
* **Effects / capabilities:** none
* **OSW hooks:** boolean simplification
* **x64 lowering snippet:** `xor al, 1` (after normalize to 0/1)
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`

---

### `null`

* **Surface token:** `null`
* **Typed CIL token:** `null`
* **AST node kind:** `NullLit`
* **Typed CIL op kind:** `CONST_NULL`
* **Effects / capabilities:** none
* **OSW hooks:** null-prop, branch pruning
* **x64 lowering snippet:** `xor rax, rax`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` if assigned to non-nullable type (if we enforce)

---

### `open`

* **Surface token:** `open`
* **Typed CIL token:** `open(path)`
* **AST node kind:** `CallExpr(name="open", args)`
* **Typed CIL op kind:** `CALL_RT_OPEN`
* **Effects / capabilities:** requires `file_io`
* **OSW hooks:** barrier (IO), cannot reorder across
* **x64 lowering snippet:** runtime call
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing file_io

---

### `or`

* **Surface token:** `or`
* **Typed CIL token:** `or`
* **AST node kind:** `LogicalOrExpr` (short-circuit recommended)
* **Typed CIL op kind:** `OR_SC` / `OR_BOOL`
* **Effects / capabilities:** RHS effects conditional
* **OSW hooks:** boolean simplify, prune
* **x64 lowering snippet:** `cmp` + `jne true_label` then eval rhs
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`

---

### `private` / `protected` / `public`

* **Surface token:** `private`, `protected`, `public`
* **Typed CIL token:** same
* **AST node kind:** `Visibility(...)`
* **Typed CIL op kind:** `CIL_VIS_*`
* **Effects / capabilities:** none (except admin policy)
* **OSW hooks:** private enables aggressive inlining/DCE
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` illegal access

---

### `proc`

* **Surface token:** `proc`
* **Typed CIL token:** `proc`
* **AST node kind:** `ProcDecl`
* **Typed CIL op kind:** `CIL_PROC` + body ops
* **Effects / capabilities:** `requires(...)` defines effect set; calls must satisfy superset rule
* **OSW hooks:** inlining, DCE, TCO, CFG opts
* **x64 lowering snippet:** prologue/epilogue per Windows x64 ABI
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`, `RANE_DIAG_SECURITY_VIOLATION`, `RANE_DIAG_PARSE_ERROR`

---

### `pragma` / `#pragma`

* **Surface token:** `pragma` and `#pragma`
* **Typed CIL token:** `pragma(...)` and `#pragma ...`
* **AST node kind:** `PragmaDecl` / `PragmaStmt`
* **Typed CIL op kind:** `CIL_PRAGMA`
* **Effects / capabilities:** may be policy-gated
* **OSW hooks:** unroll/profile scheduling directives
* **x64 lowering snippet:** none directly
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR`, `RANE_DIAG_SECURITY_VIOLATION`

---

### `read32` / `write32`

* **Surface token:** `read32`, `write32`
* **Typed CIL token:** `read32 REG, off into x;` / `write32 REG, off, val;`
* **AST node kind:** `MmioRead32`, `MmioWrite32`
* **Typed CIL op kind:** `MMIO_READ32`, `MMIO_WRITE32`
* **Effects / capabilities:** typically `syscalls` or dedicated mmio cap
* **OSW hooks:** volatile barrier
* **x64 lowering snippet:** `mov eax, [addr]` / `mov [addr], eax`
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION`

---

### `recv` / `send`

* **Surface token:** `recv`, `send`
* **Typed CIL token:** `recv(ch)` / `send(ch, val)`
* **AST node kind:** `RecvExpr`, `SendStmt`
* **Typed CIL op kind:** `CHAN_RECV`, `CHAN_SEND`
* **Effects / capabilities:** requires `channels`
* **OSW hooks:** barrier/sync modeling
* **x64 lowering snippet:** runtime calls
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION`, `RANE_DIAG_TYPE_MISMATCH`

---

### `region` / `size`

* **Surface token:** `region`, `size` (in mmio decl)
* **Typed CIL token:** same
* **AST node kind:** part of `MmioRegionDecl`
* **Typed CIL op kind:** `MMIO_REGION`
* **Effects / capabilities:** declaration none
* **OSW hooks:** none
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR`

---

### `requires`

* **Surface token:** `requires`
* **Typed CIL token:** `requires(...)`
* **AST node kind:** `RequiresCaps(list)`
* **Typed CIL op kind:** `CIL_EFFECT_SET`
* **Effects / capabilities:** defines proc effect set; call rule: caller ⊇ callee
* **OSW hooks:** restrict motion/reordering, annotate effectful calls
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing cap at callsite / illegal effect inference

---

### `return`

* **Surface token:** `return`
* **Typed CIL token:** `return`
* **AST node kind:** `ReturnStmt`
* **Typed CIL op kind:** `RET`
* **Effects / capabilities:** none
* **OSW hooks:** DCE after return; tail-call opt
* **x64 lowering snippet:** `mov rax, val` + epilogue + `ret`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` bad return value

---

### `say`

* **Surface token:** `say`
* **Typed CIL token:** typically `print("...")` or `say("...")` intrinsic
* **AST node kind:** `SayStmt(stringExpr)`
* **Typed CIL op kind:** `CALL_RT_PRINT` (recommended)
* **Effects / capabilities:** requires print capability (our imports imply `rane_rt_print`)
* **OSW hooks:** treat as IO barrier if actual output
* **x64 lowering snippet:** runtime call
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing print capability (if modeled)

---

### `spawn`

* **Surface token:** `spawn`
* **Typed CIL token:** `spawn_proc(...)`
* **AST node kind:** `SpawnExpr(procRef, args)`
* **Typed CIL op kind:** `THREAD_SPAWN`
* **Effects / capabilities:** requires `threads`
* **OSW hooks:** barrier; treat join as sync point
* **x64 lowering snippet:** runtime call to spawn
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing threads

---

### `start`

* **Surface token:** `start` (in `start at node start`)
* **Typed CIL token:** module entry mapping
* **AST node kind:** `StartAtNode(name)`
* **Typed CIL op kind:** `ENTRY_BIND`
* **Effects / capabilities:** none
* **OSW hooks:** none
* **x64 lowering snippet:** entry jumps to node label
* **Canonical errors:** `RANE_DIAG_UNDEFINED_NAME` unknown node

---

### `store`

* **Surface token:** `store` (as in `store u32 addr ... 7`)
* **Typed CIL token:** `store(u32, addr(...), 7)`
* **AST node kind:** `StoreExpr(type, address, value)`
* **Typed CIL op kind:** `STORE`
* **Effects / capabilities:** policy may gate raw store; mmio store is volatile
* **OSW hooks:** store combine, DSE, alias analysis
* **x64 lowering snippet:** compute addr → `mov [rax], eax`
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION`, `RANE_DIAG_TYPE_MISMATCH`

---

### `struct`

* **Surface token:** `struct`
* **Typed CIL token:** `struct`
* **AST node kind:** `StructDecl`
* **Typed CIL op kind:** `CIL_STRUCT`
* **Effects / capabilities:** none
* **OSW hooks:** SROA, field load/store folding
* **x64 lowering snippet:** field offsets
* **Canonical errors:** `RANE_DIAG_REDECLARED_NAME` field dup

---

### `switch`

* **Surface token:** `switch`
* **Typed CIL token:** `switch`
* **AST node kind:** `SwitchStmt`
* **Typed CIL op kind:** `SWITCH`
* **Effects / capabilities:** none
* **OSW hooks:** jump-table selection
* **x64 lowering snippet:** jump table / cmp chain
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`

---

### `template`

* **Surface token:** `template`
* **Typed CIL token:** `template <T>`
* **AST node kind:** `TemplateDecl(TypeParams)`
* **Typed CIL op kind:** `GENERIC_BIND`
* **Effects / capabilities:** none
* **OSW hooks:** specialization
* **x64 lowering snippet:** per-instantiation proc code
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR`

---

### `throw`

* **Surface token:** `throw`
* **Typed CIL token:** `throw`
* **AST node kind:** `ThrowStmt(expr)`
* **Typed CIL op kind:** `THROW`
* **Effects / capabilities:** none
* **OSW hooks:** simplify if never caught; DCE unreachable
* **x64 lowering snippet:** set error code + jump to handler or call runtime
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` throw outside try context (if disallowed)

---

### `trap`

* **Surface token:** `trap`
* **Typed CIL token:** `trap`
* **AST node kind:** `TrapStmt(code?)`
* **Typed CIL op kind:** `TRAP`
* **Effects / capabilities:** none
* **OSW hooks:** marks unreachable after
* **x64 lowering snippet:** `ud2` or `int3` (policy)
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` invalid trap operand

---

### `true`

* **Surface token:** `true`
* **Typed CIL token:** `true`
* **AST node kind:** `BoolLit(true)`
* **Typed CIL op kind:** `CONST_BOOL true`
* **Effects / capabilities:** none
* **OSW hooks:** branch pruning
* **x64 lowering snippet:** `mov eax, 1`
* **Canonical errors:** none

---

### `try`

* **Surface token:** `try`
* **Typed CIL token:** `try`
* **AST node kind:** `TryStmt(try, catch?, finally?)`
* **Typed CIL op kind:** `TRY_REGION`
* **Effects / capabilities:** none
* **OSW hooks:** region simplification
* **x64 lowering snippet:** handler labels + explicit cleanup edges
* **Canonical errors:** `RANE_DIAG_PARSE_ERROR` malformed try/catch/finally

---

### `type`

* **Surface token:** `type`
* **Typed CIL token:** `type`
* **AST node kind:** `TypeDecl`
* **Typed CIL op kind:** `CIL_TYPE_DECL`
* **Effects / capabilities:** none
* **OSW hooks:** none
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_REDECLARED_NAME`

---

### `typealias`

* **Surface token:** `typealias`
* **Typed CIL token:** `typealias`
* **AST node kind:** `TypeAliasDecl`
* **Typed CIL op kind:** `CIL_TYPEALIAS`
* **Effects / capabilities:** none
* **OSW hooks:** preserve nominal until late, then erase representation
* **x64 lowering snippet:** none
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` implicit use (if disallowed)

---

### `union`

* **Surface token:** `union`
* **Typed CIL token:** `union`
* **AST node kind:** `UnionDecl`
* **Typed CIL op kind:** `CIL_UNION`
* **Effects / capabilities:** none
* **OSW hooks:** conservative aliasing
* **x64 lowering snippet:** base+0 accesses
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` invalid union field types

---

### `variant`

* **Surface token:** `variant`
* **Typed CIL token:** `variant`
* **AST node kind:** `VariantDecl`
* **Typed CIL op kind:** `CIL_VARIANT` (tagged union)
* **Effects / capabilities:** none
* **OSW hooks:** match lowering; constant-case pruning
* **x64 lowering snippet:** tag compare + branch
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` invalid payload

---

### `vector`

* **Surface token:** `vector`
* **Typed CIL token:** `vector(...)`
* **AST node kind:** `VectorLiteralExpr`
* **Typed CIL op kind:** `RT_VEC_NEW` + pushes
* **Effects / capabilities:** requires `heap_alloc`
* **OSW hooks:** barrier; may fold literal construction if immutable + enabled
* **x64 lowering snippet:** runtime calls
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing heap_alloc

---

### `while`

* **Surface token:** `while`
* **Typed CIL token:** `while (...)`
* **AST node kind:** `WhileStmt(cond, body)`
* **Typed CIL op kind:** `WHILE` lowered to CFG (`BR/JMP`)
* **Effects / capabilities:** none
* **OSW hooks:** loop opts
* **x64 lowering snippet:** loop labels + `cmp/jcc`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH` cond not bool

---

### `with`

* **Surface token:** `with`
* **Typed CIL token:** lowered to `try/finally`
* **AST node kind:** `WithStmt(resourceExpr, binder, body)`
* **Typed CIL op kind:** `WITH` → `TRY_FINALLY`
* **Effects / capabilities:** acquiring/closing resource requires capability (e.g., `file_io`)
* **OSW hooks:** cleanup merging; ensure all exits run cleanup
* **x64 lowering snippet:** cleanup label and jumps
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION` missing capability

---

### `write`

* **Surface token:** `write`
* **Typed CIL token:** `write(f, "hello")`
* **AST node kind:** `CallExpr(name="write")`
* **Typed CIL op kind:** `CALL_RT_WRITE`
* **Effects / capabilities:** requires `file_io`
* **OSW hooks:** IO barrier
* **x64 lowering snippet:** runtime call
* **Canonical errors:** `RANE_DIAG_SECURITY_VIOLATION`

---

### `xor`

* **Surface token:** `xor`
* **Typed CIL token:** `xor`
* **AST node kind:** `BitXorExpr(lhs, rhs)`
* **Typed CIL op kind:** `XOR`
* **Effects / capabilities:** none
* **OSW hooks:** algebraic simplify
* **x64 lowering snippet:** `xor rax, rbx`
* **Canonical errors:** `RANE_DIAG_TYPE_MISMATCH`

---

## Appendix B — Operator Precedence Table

> Highest precedence at top.
> All operators listed are exactly those present in our syntax examples and keyword list.

| Precedence | Operators / Forms                               | Associativity | Notes                               |      |               |
| 

---

---

---

: | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

- | 

---

---

---

---

- |
|          1 | `()` indexing `[]` field `.` generic call `<T>` | left          | `a.b`, `a[i]`, `f<T>(x)`            |      |               |
|          2 | unary `-` `!` `not` `~`                         | right         | unary binds tight                   |      |               |
|          3 | `*` `/` `%`                                     | left          | integer/float by type               |      |               |
|          4 | `+` `-`                                         | left          | arithmetic                          |      |               |
|          5 | shifts `shl` `shr` `sar` `<<` `>>`              | left          | shift count int                     |      |               |
|          6 | bitwise `&`                                     | left          |                                     |      |               |
|          7 | bitwise `^` `xor`                               | left          |                                     |      |               |
|          8 | bitwise `                                       | `             | left                                |      |               |
|          9 | comparisons `<` `<=` `>` `>=`                   | left          | yield `bool`                        |      |               |
|         10 | equality `==` `!=`                              | left          | yield `bool`                        |      |               |
|         11 | logical AND `and` `&&`                          | left          | short-circuit                       |      |               |
|         12 | logical OR `or` `                               |               | `                                   | left | short-circuit |
|         13 | ternary `?:`                                    | right         | `c ? a : b`                         |      |               |
|         14 | assignment `=` and compound (lowered)           | right         | `a = b`                             |      |               |
|         15 | sequence / statement separators                 | n/a           | surface uses newlines; CIL uses `;` |      |               |

---

## Appendix C — Typed CIL Opcode Catalog (Current Feature Set)

> This is a **catalog of canonical Typed CIL ops** that cover everything we demonstrated.
> Each op includes: **Signature**, **Type rules**, **Effects**, and **Lowering target**.

### C.1 Module / Decls

* **`CIL_MODULE(name)`**

  * Decl-only.
* **`CIL_IMPORT(path | ns::sym)`**
* **`CIL_NAMESPACE(name)`**
* **`CIL_CAP_DECL(cap)`**
* **`CIL_TYPE_DECL(name)`**
* **`CIL_ALIAS(name, type)`**
* **`CIL_TYPEALIAS(name, type)`**
* **`CIL_CONST(name, type, value)`**
* **`CIL_CONSTEXPR(name, type, value)`**
* **`CIL_CONSTINIT(name, type, value)`**
* **`CIL_CONTRACT(name, params, ensuresExpr)`**
* **`CIL_STRUCT(name, fields)`**
* **`CIL_ENUM(name, repr, cases)`**
* **`CIL_VARIANT(name, typeParams, cases)`**
* **`CIL_UNION(name, fields)`**
* **`CIL_PRAGMA(kind, args...)`**
* **`CIL_DEFINE(name, value)`**
* **`CIL_ATTR_DERIVE(list)`**

### C.2 Control Flow (CFG)

* **`LABEL(name)`**
* **`JMP(label)`**
* **`BR_COND(cond: bool, t: label, f: label)`**
* **`RET(value?)`**
* **`HALT`** (terminator)
* **`TRAP(code?)`** (terminator)

### C.3 Locals / Values / Memory

* **`LOCAL(name, type)`** (alloc local)
* **`LOAD(type, addr)`**
* **`STORE(type, addr, value)`**
* **`ADDR_CALC(base, a, b, c)`** (our addr form; may generalize)

### C.4 Arithmetic / Logical

* **`ADD/SUB/MUL/DIV/MOD(T, a, b)`**
* **`NEG(T, a)`**
* **`AND/OR/XOR(T, a, b)`** (bitwise, integer types)
* **`NOT_BOOL(a)`**, **`BIT_NOT(T, a)`**
* **`SHL/SHR/SAR(T, a, shift)`**
* **`CMP_LT/LE/GT/GE/EQ/NE(T, a, b) -> bool`**

### C.5 Select / Ternary

* **`SELECT(cond: bool, a: T, b: T) -> T`**

  * Lowers to branches + phi, or `cmov` if safe.

### C.6 Calls / Intrinsics

* **`CALL(proc, args...) -> T`**

  * Must satisfy effect superset rule.
* **`CALL_RT(symbol, args...) -> T`**

  * Same as CALL but import-resolved.
* **`INLINE_ASM(lines, constraints...)`**
* **`DYNAMIC_EVAL(expr: string) -> T`**
* **`ASSERT(cond: bool, msg: string)`**
* **`HEAP_ALLOC(T, count) -> ptr`**
* **`HEAP_FREE(ptr)`**
* **`BORROW_REF(x) -> &T`**

### C.7 Concurrency

* **`THREAD_SPAWN(procRef, args...) -> thread_handle`**
* **`THREAD_JOIN(handle) -> T`** (typed join variants)
* **`MUTEX_LOCK(mutex)`**
* **`MUTEX_UNLOCK(mutex)`**
* **`CHAN_SEND(ch, val)`**
* **`CHAN_RECV(ch) -> T`**
* **`AWAIT(awaitable) -> T`** (lowered via state machines)

### C.8 MMIO

* **`MMIO_REGION(name, base, size)`**
* **`MMIO_READ32(region, offset) -> u32`**
* **`MMIO_WRITE32(region, offset, value)`**

### C.9 Pattern / Switch

* **`SWITCH(scrutinee, cases..., default)`** (integers/enums)
* **`MATCH(scrutinee, patternCases..., default?)`** (variants/struct patterns)

  * Both lower to CFG branches + (optional) jump table.

---

## Appendix D — x64 Emission Templates (Windows x86-64, bytes-level forms)

> These are **bytes-level templates** for the core patterns we requested previously (Rel32 calls, Abs64 immediates, prologue/epilogue, branches, jump tables).
> Bytes shown in hex with placeholders in brackets.

### D.1 Windows x64 Prologue / Epilogue (minimal)

**Prologue:**

* `sub rsp, imm32`

  * bytes: `48 81 EC [imm32]`
* (optional) save nonvolatile regs, e.g. `push rbx`

  * `53`

**Epilogue:**

* `add rsp, imm32`

  * `48 81 C4 [imm32]`
* `ret`

  * `C3`

**Canonical errors:**

* `RANE_DIAG_INTERNAL_ERROR` frame misalignment, shadow-space omission

---

### D.2 Call (Rel32) + Shadow Space Pattern

**Rule:** before any call: ensure **32 bytes shadow** + **16-byte alignment**.

Typical sequence:

* `sub rsp, 40h` (32 shadow + 16 align cushion)

  * `48 83 EC 40`
* `call rel32`

  * `E8 [rel32]` where `rel32 = target - (next_ip)`
* `add rsp, 40h`

  * `48 83 C4 40`

**Canonical errors:**

* `RANE_DIAG_INTERNAL_ERROR` bad rel32 patch / import thunk missing

---

### D.3 Load Immediate (Abs64) into RAX

* `mov rax, imm64`

  * `48 B8 [imm64]`

Common for addresses or large constants.

---

### D.4 Move / Load / Store (stack and memory)

* `mov rax, [rsp+disp32]`

  * `48 8B 84 24 [disp32]`
* `mov [rsp+disp32], rax`

  * `48 89 84 24 [disp32]`
* `mov eax, [rax]` (u32 load)

  * `8B 00`
* `mov [rax], eax` (u32 store)

  * `89 00`

---

### D.5 Compare + Conditional Branch

* `cmp rax, rbx`

  * `48 39 D8`
* `cmp eax, imm32`

  * `3D [imm32]`
* `test al, al`

  * `84 C0`

Conditional jumps:

* `je rel32` → `0F 84 [rel32]`
* `jne rel32` → `0F 85 [rel32]`
* `jl rel32` → `0F 8C [rel32]`
* `jle rel32` → `0F 8E [rel32]`
* `jg rel32` → `0F 8F [rel32]`
* `jge rel32` → `0F 8D [rel32]`

Unconditional:

* `jmp rel32` → `E9 [rel32]`

---

### D.6 Boolean Materialization (setcc)

* `setl al` (example for `<`)

  * `0F 9C C0`
* zero-extend to eax:

  * `0F B6 C0` (`movzx eax, al`)

---

### D.7 Arithmetic Core

* `add rax, rbx` → `48 01 D8`
* `sub rax, rbx` → `48 29 D8`
* `imul rax, rbx` → `48 0F AF C3` (one common encoding)

**Signed div/mod (i64):**

* `cqo` → `48 99`
* `idiv rbx` → `48 F7 FB`
* quotient in `rax`, remainder in `rdx`

---

### D.8 Conditional Move (cmov) for `choose` / `select`

* `cmp rax, rbx` → `48 39 D8`
* `cmovl rax, rbx` (move if less)

  * `48 0F 4C C3`

Use only when both sides are already computed and side-effect free.

---

### D.9 Jump Table Template (dense switch)

Pattern:

1. bounds check
2. indirect jump through table

Example for `eax` index:

* `cmp eax, imm32` → `3D [imm32]`
* `ja default` → `0F 87 [rel32]`
* `lea rdx, [rip + table]` → `48 8D 15 [rel32]`
* `movsxd rax, eax` → `48 63 C0`
* `jmp qword ptr [rdx + rax*8]` → `FF 24 C2`

Table is 8-byte absolute addresses or RIP-relative thunks (preferred: RIP-relative thunks for PIE-like behavior).

---

### D.10 Trap / Halt Templates

* `ud2` (invalid opcode trap) → `0F 0B`
* `int3` (breakpoint trap) → `CC`

“Halt” policy options:

* `ud2` (terminate)
* `jmp $` (spin) → `EB FE`
* call `ExitProcess` (clean exit) using IAT call pattern

---

## (1) Surface → AST Mapping Table (every keyword / construct)

> **Legend**

* **KW** = surface keyword/token
* **AST Kind** = canonical node kind name
* **Children / fields** = required subparts
* **Notes** = disambiguation / shape rules

### A1. Directives / Decls / Qualifiers

| KW                   | AST Kind          | Children / fields                                          | Notes                                         |                 |
| 

---

---

---

---

---

---

-- | 

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

 |
| `module`             | `ModuleDecl`      | `name: Ident`                                              | One per file (recommended).                   |                 |
| `import`             | `ImportDecl`      | `path: Path` OR `path: Path, sym: Ident`                   | `math::square` binds symbol import.           |                 |
| `namespace`          | `NamespaceDecl`   | `name: Ident, decls: [Decl]`                               | Block `: ... end`.                            |                 |
| `public`             | `VisQualifier`    | `vis = Public`                                             | Attaches to next decl.                        |                 |
| `protected`          | `VisQualifier`    | `vis = Protected`                                          | Attaches to next decl.                        |                 |
| `private`            | `VisQualifier`    | `vis = Private`                                            | Attaches to next decl.                        |                 |
| `admin`              | `VisQualifier`    | `vis = Admin`                                              | Policy-gated access.                          |                 |
| `export`             | `ExportQualifier` | `enabled = true`                                           | Attaches to decl.                             |                 |
| `inline`             | `ProcQualifier`   | `kind = Inline`                                            | Strong inlining hint.                         |                 |
| `async`              | `ProcQualifier`   | `kind = Async`                                             | Transforms body to state machine.             |                 |
| `dedicate`           | `ProcQualifier`   | `kind = Dedicate`                                          | Spawn-target semantics.                       |                 |
| `linear`             | `ProcQualifier`   | `kind = Linear`                                            | Enforces linear usage within proc.            |                 |
| `nonlinear`          | `ProcQualifier`   | `kind = Nonlinear`                                         | Opt-out tag (policy-defined).                 |                 |
| `requires`           | `RequiresCaps`    | `caps: [Ident]`                                            | Attaches to proc signature.                   |                 |
| `capability`         | `CapabilityDecl`  | `name: Ident`                                              | Declares capability symbol.                   |                 |
| `type`               | `TypeDecl`        | `name: Ident`                                              | Builtin registration/opaque (per our rules). |                 |
| `alias`              | `AliasDecl`       | `name: Ident, target: TypeRef`                             | Pure synonym.                                 |                 |
| `typealias`          | `TypeAliasDecl`   | `name: Ident, target: TypeRef`                             | Nominal identity (representation same).       |                 |
| `struct`             | `StructDecl`      | `name: Ident, fields: [FieldDecl]`                         | Field: `name TypeRef`.                        |                 |
| `enum`               | `EnumDecl`        | `name: Ident, repr: TypeRef, cases: [EnumCase]`            | Case may have explicit value.                 |                 |
| `variant`            | `VariantDecl`     | `name: Ident, tparams?: [TypeParam], cases: [VariantCase]` | Tagged union.                                 |                 |
| `union`              | `UnionDecl`       | `name: Ident, fields: [FieldDecl]`                         | Unsafe-by-policy unless tracked.              |                 |
| `@derive`            | `AttrDerive`      | `names: [Ident], targetDecl: Decl`                         | Precedes `struct/enum/variant`.               |                 |
| `const`              | `ConstDecl`       | `name, type: TypeRef, init: Expr`                          | Init effect-free unless compile-time.         |                 |
| `constexpr`          | `ConstexprDecl`   | `name, type, init`                                         | Must evaluate at compile-time.                |                 |
| `constinit`          | `ConstinitDecl`   | `name, type, init`                                         | Static-init safe only.                        |                 |
| `consteval`          | `ProcQualifier`   | `kind = Consteval`                                         | Compile-time-only proc.                       |                 |
| `contract`           | `ContractDecl`    | `name, params, ensures: Expr`                              | Predicates pure.                              |                 |
| `ensures`            | `ContractEnsures` | `expr: Expr`                                               | Used inside contract decl.                    |                 |
| `macro`              | `MacroDecl`       | `name, params, bodyTokens`                                 | Expands before typed CIL.                     |                 |
| `template`           | `TemplateDecl`    | `tparams: [TypeParam], target: Decl`                       | e.g., `proc identity<T>`.                     |                 |
| `pragma`             | `PragmaDecl`      | `key: String, args: [String                                | Int]`                                         | Also `#pragma`. |
| `#pragma`            | `PragmaDecl`      | `key: Ident, args...`                                      | Preprocessor-style.                           |                 |
| `define` / `#define` | `DefineDecl`      | `name: Ident, value: ConstExpr`                            | Compile-time macro constant.                  |                 |

### A2. Control Blocks / Statements

| KW        | AST Kind                                            | Children / fields                                                         | Notes                                                     |
| 

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 |
| `proc`    | `ProcDecl`                                          | `name, params: [Param], ret: TypeRef, qualifiers, requires?, body: Block` | Surface params: `x i64 y i64`.                            |
| `let`     | `LetStmt`                                           | `name, type?: TypeRef, init?: Expr`                                       | Inference only if allowed.                                |
| `set`     | `AssignStmt`                                        | `target: LValue, value: Expr`                                             | Surface: `set x to expr`.                                 |
| `mutate`  | `MutateStmt`                                        | `target: LValue, value: Expr`                                             | Same as store but semantics may enforce mutability rules. |
| `add`     | `AddAssignStmt`                                     | `target: LValue, value: Expr`                                             | Lowers to `target = target + value`.                      |
| `return`  | `ReturnStmt`                                        | `value?: Expr`                                                            | Must match proc ret.                                      |
| `if`      | `IfStmt`                                            | `cond: Expr, then: Block, else?: Block`                                   | Blocked by `:` and `end`.                                 |
| `else`    | `ElseClause`                                        | `block: Block`                                                            | Only inside `IfStmt`.                                     |
| `while`   | `WhileStmt`                                         | `cond, body`                                                              |                                                           |
| `for`     | `ForStmt`                                           | `init: Stmt, cond: Expr, step: Stmt, body`                                | Lowers to CFG loop.                                       |
| `match`   | `MatchStmt`                                         | `scrutinee: Expr, arms: [MatchArm], default?: Block`                      | Patterns include variant cases.                           |
| `case`    | `CaseArm`                                           | `patOrConst, block`                                                       | Used by match/switch/decide.                              |
| `default` | `DefaultArm`                                        | `block`                                                                   |                                                           |
| `switch`  | `SwitchStmt`                                        | `scrutinee, cases: [ConstCase], default`                                  | Integer/enum oriented.                                    |
| `decide`  | `DecideStmt`                                        | `scrutinee, cases, default`                                               | Alias of switch-like with different style.                |
| `goto`    | `GotoStmt`                                          | `cond?: Expr, trueLabel: Ident, falseLabel?: Ident`                       | Our form `goto c ? A : B`.                               |
| `label`   | `LabelStmt`                                         | `name: Ident`                                                             |                                                           |
| `node`    | `NodeDecl`                                          | `name: Ident, body: Block`                                                | CFG block grouping.                                       |
| `start`   | `StartAtNode`                                       | `nodeName: Ident`                                                         | Entry binding.                                            |
| `go`      | `GoToNode`                                          | `nodeName: Ident`                                                         | Lowers to jmp.                                            |
| `say`     | `SayStmt`                                           | `msg: Expr`                                                               | Often maps to print runtime.                              |
| `halt`    | `HaltStmt`                                          | `mode?: Ident/Int`                                                        | Terminator.                                               |
| `trap`    | `TrapStmt`                                          | `code?: Int`                                                              | Terminator.                                               |
| `assert`  | `AssertStmt`                                        | `cond: Expr, msg: Expr`                                                   | Msg string.                                               |
| `try`     | `TryStmt`                                           | `tryBlock, catch?: CatchClause, finally?: FinallyClause`                  |                                                           |
| `catch`   | `CatchClause`                                       | `binder?: Ident, block`                                                   |                                                           |
| `finally` | `FinallyClause`                                     | `block`                                                                   |                                                           |
| `throw`   | `ThrowStmt`                                         | `value?: Expr`                                                            | Error model determines lowering.                          |
| `with`    | `WithStmt`                                          | `resource: Expr, binder?: Ident, body: Block`                             | Lowers to try/finally.                                    |
| `defer`   | `DeferStmt`                                         | `expr: Expr`                                                              | Scope-exit action.                                        |
| `lock`    | `LockStmt`                                          | `mutexExpr: Expr, body: Block`                                            | Lowers to lock/unlock in try/finally.                     |
| `spawn`   | `SpawnExpr`                                         | `procRef: Expr, args: [Expr]`                                             | Returns thread handle.                                    |
| `join`    | `JoinExpr`                                          | `handle: Expr`                                                            | Returns typed join result.                                |
| `send`    | `SendStmt`                                          | `ch: Expr, val: Expr`                                                     |                                                           |
| `recv`    | `RecvExpr`                                          | `ch: Expr`                                                                |                                                           |
| `asm`     | `AsmStmt`                                           | `lines: [String]`                                                         | Optional constraints/clobbers if we add.                 |
| `mmio`    | `MmioRegionDecl` / `MmioReadExpr` / `MmioWriteStmt` | region decl fields or read/write args                                     | `read32/write32` are separate KWs below.                  |
| `region`  | (part of) `MmioRegionDecl`                          | `name`                                                                    | token inside mmio decl                                    |
| `from`    | (part of) `MmioRegionDecl`                          | `base`                                                                    |                                                           |
| `size`    | (part of) `MmioRegionDecl`                          | `bytes`                                                                   |                                                           |
| `read32`  | `MmioReadExpr`                                      | `region: Ident, offset: Expr`                                             | Produces `u32`.                                           |
| `write32` | `MmioWriteStmt`                                     | `region, offset, value`                                                   |                                                           |

### A3. Expression Keywords / Operators-as-tokens

| Token            | AST Kind       | Fields                    | Notes                                           |
| 

---

---

---

---

---

- | 

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `addr`           | `AddrExpr`     | `(base,a,b,c)`            | our 4-arg addr form                            |
| `load`           | `LoadExpr`     | `(type, addrExpr)`        |                                                 |
| `store`          | `StoreExpr`    | `(type, addrExpr, value)` | statement-like, but expression ok if we allow  |
| `borrow`         | `BorrowExpr`   | `(expr)`                  |                                                 |
| `choose`         | `ChooseExpr`   | `(mode, a, b)`            | mode inferred from token sequence (e.g., `max`) |
| `eval`           | `EvalExpr`     | `(stringExpr)`            | dynamic eval                                    |
| `open`           | `CallExpr`     | runtime call              | same for `write`, `free`, `allocate`            |
| `allocate`       | `AllocateExpr` | `(type, count)`           |                                                 |
| `free`           | `FreeStmt`     | `(ptr)`                   |                                                 |
| `true` / `false` | `BoolLit`      | value                     |                                                 |
| `null`           | `NullLit`      | —                         |                                                 |

Operators:

* unary `-` → `UnaryNegExpr`
* unary `not`/`!` → `UnaryNotExpr`
* unary `~` → `UnaryBitNotExpr`
* `+ - * / %` → `BinaryArithExpr(op, a, b)`
* `& | xor/^` → `BinaryBitExpr(op, a, b)`
* `shl/shr/sar/<< >>` → `BinaryShiftExpr(op, a, b)`
* comparisons → `CompareExpr(op, a, b)`
* logical `and/or/&&/||` → `LogicalExpr(op, a, b)` (short-circuit nodes)
* ternary `?:` → `TernaryExpr(cond, t, f)`
* cast `as` → `CastExpr(expr, type)`

---

## (2) Typed CIL Opcode → Exact Emission Template(s) Table

> **Mechanical backend contract**

* Architecture: **x86-64**
* ABI: **Windows x64**
* Registers: GP: `RAX RCX RDX R8 R9 R10 R11` volatile, `RBX RBP RSI RDI R12-R15` nonvolatile.
* Return: `RAX` (integers/pointers), `XMM0` (floats if implemented).
* **Call discipline:** reserve **32-byte shadow**; ensure **RSP 16-byte alignment at call**.

> **Notation**

* `[…]` means patch/placeholder filled by emitter.
* `rel32(dst)` computed as `dst - (next_ip)` at patch time.
* `imm32/imm64` little-endian.
* `disp32` little-endian.
* `slot(x)` means compiler-assigned stack slot displacement for local `x` relative to `rsp` after prologue.
* For brevity, templates show the **primary encoding** we’ll use most often.

### D2.1 Control Flow / CFG

| Typed CIL Op        | Purpose                                       | Exact emission template(s)                                                                                                                                                           |
| 

---

---

---

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 |
| `LABEL L`           | label                                         | `L:` *(no bytes; assembler label)*                                                                                                                                                   |
| `JMP L`             | unconditional jump                            | `E9 [rel32(L)]`                                                                                                                                                                      |
| `BR_COND v, Lt, Lf` | conditional branch (v is bool in AL or stack) | **If v in AL:** `84 C0` (`test al,al`) + `0F 85 [rel32(Lt)]` + `E9 [rel32(Lf)]`  \n**If v in [rsp+disp32]:** `80 BC 24 [disp32] 00` (`cmp byte [rsp+disp32],0`) + `0F 85 …` + `E9 …` |
| `RET`               | return void                                   | epilogue + `C3`                                                                                                                                                                      |
| `RET v`             | return value in RAX                           | move v → RAX then epilogue + `C3`                                                                                                                                                    |
| `HALT`              | terminate                                     | policy: `0F 0B` (`ud2`) **or** `EB FE` (spin) **or** call `ExitProcess`                                                                                                              |
| `TRAP`              | trap                                          | `0F 0B` (`ud2`) *(or `CC`)*                                                                                                                                                          |

### D2.2 Prologue / Epilogue (emitter-owned, not an opcode but required)

We’ll apply this around every proc:

* **PROLOGUE(frame):** `48 81 EC [imm32 FRAME]` (or small imm8 form `48 83 EC xx`) + saves
* **EPILOGUE(frame):** restores + `48 81 C4 [imm32 FRAME]` + `C3`

### D2.3 Locals / Loads / Stores / Addressing

| Typed CIL Op             | Purpose                  | Exact emission template(s)                                                                                                                                              |
| 

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `LOCAL x: T`             | reserve slot / virtual   | **No direct bytes** (frame planner assigns `slot(x)`)                                                                                                                   |
| `ADDR_CALC`              | compute address into RAX | **Common:** `48 B8 [imm64 base]` then `48 05 [imm32 off]` for small off \n**Preferred:** fold into `LEA` when base is reg: `48 8D 80 [disp32]` (`lea rax,[rax+disp32]`) |
| `LOAD u32, addr -> eax`  | load 32-bit              | if addr in RAX: `8B 00` (`mov eax,[rax]`)                                                                                                                               |
| `LOAD u64, addr -> rax`  | load 64-bit              | `48 8B 00` (`mov rax,[rax]`)                                                                                                                                            |
| `STORE u32, addr, v`     | store 32-bit             | ensure value in EAX then addr in RAX: `89 00` (`mov [rax],eax`)                                                                                                         |
| `STORE u64, addr, v`     | store 64-bit             | ensure value in RDX/ RAX then: `48 89 10` (`mov [rax],rdx`) or `48 89 00` if in RAX (choose convention)                                                                 |
| `LOAD_LOCAL x -> rax`    | load local               | `48 8B 84 24 [disp32 slot(x)]`                                                                                                                                          |
| `STORE_LOCAL x <- rax`   | store local              | `48 89 84 24 [disp32 slot(x)]`                                                                                                                                          |
| `LOAD_LOCAL32 x -> eax`  | 32-bit local load        | `8B 84 24 [disp32 slot(x)]`                                                                                                                                             |
| `STORE_LOCAL32 x <- eax` | 32-bit local store       | `89 84 24 [disp32 slot(x)]`                                                                                                                                             |

### D2.4 Constants / Moves

| Typed CIL Op             | Purpose           | Exact emission template(s)                                       |
| 

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

- |
| `CONST_I64 imm -> rax`   | materialize imm64 | `48 B8 [imm64]`                                                  |
| `CONST_I32 imm -> eax`   | materialize imm32 | `B8 [imm32]`                                                     |
| `CONST_BOOL true -> al`  | bool true         | `B0 01`                                                          |
| `CONST_BOOL false -> al` | bool false        | `30 C0` (`xor al,al`)                                            |
| `CONST_NULL -> rax`      | null pointer      | `48 31 C0` (`xor rax,rax`)                                       |
| `MOV rax, rbx`           | reg move          | `48 89 D8` (rbx→rax) or `48 8B C3` (rax←rbx) depending direction |

### D2.5 Integer Arithmetic / Bitwise / Shifts

> These assume operands are already in regs (our reg allocator decides which).

| Typed CIL Op              | Purpose          | Exact emission template(s)                                                   |
| 

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

- |
| `ADD_I64 rax, rbx -> rax` | add              | `48 01 D8`                                                                   |
| `SUB_I64 rax, rbx -> rax` | sub              | `48 29 D8`                                                                   |
| `MUL_I64 rax, rbx -> rax` | imul             | `48 0F AF C3` *(rax = rax*rbx uses other encodings too; pick one canonical)* |
| `NEG_I64 rax`             | neg              | `48 F7 D8`                                                                   |
| `AND_I64 rax, rbx`        | and              | `48 21 D8`                                                                   |
| `OR_I64 rax, rbx`         | or               | `48 09 D8`                                                                   |
| `XOR_I64 rax, rbx`        | xor              | `48 31 D8`                                                                   |
| `BITNOT_I64 rax`          | not              | `48 F7 D0`                                                                   |
| `SHL_I64 rax, cl`         | shift left       | `48 D3 E0`                                                                   |
| `SHR_I64 rax, cl`         | logical right    | `48 D3 E8`                                                                   |
| `SAR_I64 rax, cl`         | arithmetic right | `48 D3 F8`                                                                   |
| `DIVMOD_I64 rax, rbx`     | signed div/mod   | `48 99` (`cqo`) + `48 F7 FB` (`idiv rbx`)                                    |

### D2.6 Comparisons + Boolean Materialization

| Typed CIL Op       | Purpose              | Exact emission template(s) |
| 

---

---

---

---

---

---

 | 

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

-- |
| `CMP_I64 rax, rbx` | set flags            | `48 39 D8`                 |
| `SET_LT -> al`     | al = (LT)            | `0F 9C C0`                 |
| `SET_LE -> al`     |                      | `0F 9E C0`                 |
| `SET_GT -> al`     |                      | `0F 9F C0`                 |
| `SET_GE -> al`     |                      | `0F 9D C0`                 |
| `SET_EQ -> al`     |                      | `0F 94 C0`                 |
| `SET_NE -> al`     |                      | `0F 95 C0`                 |
| `MOVZX_AL_TO_EAX`  | eax = zero-extend al | `0F B6 C0`                 |

### D2.7 Logical Short-Circuit (`AND_SC`, `OR_SC`)

These are CFG patterns, not single instructions.

| Typed CIL Op | Exact emission template(s)                                                                                           |
| 

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `AND_SC`     | `test al,al` (`84 C0`) + `je Lfalse` (`0F 84 rel32`) + eval rhs into AL + `jmp Lend` + `Lfalse: xor al,al` + `Lend:` |
| `OR_SC`      | `test al,al` + `jne Ltrue` + eval rhs + `jmp Lend` + `Ltrue: mov al,1` + `Lend:`                                     |

### D2.8 SELECT / `choose` (cmov variant)

| Typed CIL Op                   | Purpose                             | Exact emission template(s)                                                                                                                                                                                               |
| 

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 |
| `SELECT_I64 cond, a, b -> rax` | select without branches (when safe) | **Assume:** `a in rax`, `b in rbx`, `cond in al` \n`test al,al` (`84 C0`) \n`cmovz rax, rbx` isn't available for Z flag directly unless flags set; so canonical is flags from `test`: \n`48 0F 44 C3` (`cmovz rax, rbx`) |
| `CHOOSE_MAX_I64 a,b -> rax`    | max                                 | `mov rax,a` + `mov rbx,b` + `cmp rax,rbx` (`48 39 D8`) + `48 0F 4C C3` (`cmovl rax,rbx`)                                                                                                                                 |

### D2.9 CALL / Runtime Calls (IAT / thunk)

| Typed CIL Op         | Purpose                        | Exact emission template(s)                                                                                   |
| 

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 |
| `CALL target`        | call internal symbol           | ensure shadow/align: `48 83 EC 40` + `E8 [rel32(target)]` + `48 83 C4 40`                                    |
| `CALL_IAT __imp_F`   | imported function pointer call | `48 83 EC 40` + `48 8B 05 [rel32(__imp_F)]` *(mov rax,[rip+__imp_F])* + `FF D0` *(call rax)* + `48 83 C4 40` |
| `CALL_THUNK thunk_F` | CFG/DEP-friendly thunk         | `48 83 EC 40` + `E8 [rel32(thunk_F)]` + `48 83 C4 40`                                                        |

### D2.10 ASSERT / TRAP / THROW (error model)

| Typed CIL Op                      | Purpose            | Exact emission template(s)                                  |
| 

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `ASSERT cond,msgid`               | trap if false      | `test al,al` + `jne Lok` + `0F 0B` + `Lok:`                 |
| `THROW code` (error-return model) | set error + jump   | `mov eax,[imm32 code]` (`B8 imm32`) + `E9 [rel32(handler)]` |
| `TRY/CATCH/FINALLY`               | structured regions | lowered into labels + edges; no single opcode emission      |

### D2.11 MMIO (volatile)

| Typed CIL Op              | Purpose        | Exact emission template(s)                                               |
| 

---

---

---

---

---

---

---

---

- | 

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 |
| `MMIO_READ32 addr -> eax` | volatile load  | addr in RAX: `8B 00` plus **no reordering** (compiler barrier semantics) |
| `MMIO_WRITE32 addr, eax`  | volatile store | `89 00` plus barrier semantics                                           |

### D2.12 Concurrency (runtime-call based)

| Typed CIL Op   | Purpose      | Exact emission template(s)                            |
| 

---

---

---

---

-- | 

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `THREAD_SPAWN` | spawn thread | `CALL_IAT __imp_rane_rt_threads_spawn` (args in regs) |
| `THREAD_JOIN`  | join         | `CALL_IAT __imp_rane_rt_threads_join_*`               |
| `MUTEX_LOCK`   | lock         | `CALL_IAT __imp_rane_rt_mutex_lock`                   |
| `MUTEX_UNLOCK` | unlock       | `CALL_IAT __imp_rane_rt_mutex_unlock`                 |
| `CHAN_SEND`    | send         | `CALL_IAT __imp_rane_rt_chan_send`                    |
| `CHAN_RECV`    | recv         | `CALL_IAT __imp_rane_rt_chan_recv_*`                  |

---

## “Backend mechanically specifiable” closing constraints (the strict rules)

To make this truly mechanical, the emitter must also obey these **invariants**:

1. **Value placement contract** per opcode (e.g., `ADD_I64` requires inputs in specific regs).

   * If we want it deterministic, define a fixed operand register convention per opcode (RAX/RBX/RCX patterns) *or* define a small register allocator that maps virtual regs to physical regs and spills to stack.

2. **Frame lawet contract** (we already asked for a frame planner earlier):

   * every local has a slot and known size/alignment
   * stack frame size includes: locals + spills + temps + **32 shadow** + alignment padding

3. **Relocation contract**:

   * `CALL rel32`, `JMP rel32`, `Jcc rel32`, RIP-rel loads all require patch records
   * each patch record: site_offset, kind, target_symbol_id

4. **Volatile / barrier contract**:

   * MMIO and lock/send/recv calls are barriers (OSW must not move across them)

---

# Emitter Contract Sheet (Consolidated)

This sheet makes the backend **mechanically specifiable**: for each Typed CIL opcode, it fixes **(1)** operand locations, **(2)** result location, **(3)** clobbers, **(4)** flags contract, **(5)** emitted bytes, **(6)** relocations.

---

## 0) Global ABI + Emitter Conventions (mandatory)

### 0.1 Target + ABI

* Target: **x86-64**
* ABI: **Windows x64**

  * Args in: **RCX, RDX, R8, R9** then stack
  * Return in: **RAX** (integers/pointers), (float return not covered here)
  * Caller must reserve **32 bytes shadow space** at every external call.
  * Stack alignment: **RSP must be 16-byte aligned at call boundary**.

### 0.2 Canonical value locations (fixed)

To make emission deterministic, **Typed CIL lowering must place operands exactly here** before emitting each opcode:

* **Primary scalar (i64/u64/ptr)**: `RAX`
* **Secondary scalar**: `RBX`
* **Tertiary scratch**: `R10` (emitter scratch), `R11` (emitter scratch)
* **Bool**: `AL` (0/1)
* **Shift count**: `CL` (low 8 bits of RCX)
* **32-bit scalar**: `EAX` (primary), `EBX` (secondary)
* **Store value**:

  * 64-bit store uses **RDX** as value register (so address can remain in RAX)
  * 32-bit store uses **EDX**

> If the register allocator wants different regs, it must insert `MOV` shims to conform to this contract **before** each opcode.

### 0.3 Canonical memory operand form for locals

All locals/spills are addressed as:

* `[RSP + disp32]` using SIB form `[rsp+disp32]`

Canonical encodings:

* `mov rax, [rsp+disp32]` = `48 8B 84 24 [disp32]`
* `mov [rsp+disp32], rax` = `48 89 84 24 [disp32]`
* `mov eax, [rsp+disp32]` = `8B 84 24 [disp32]`
* `mov [rsp+disp32], eax` = `89 84 24 [disp32]`

### 0.4 Relocation record kinds (exact)

ther ExecMeta/patcher should support these relocation kinds:

* `REL32_CALL`: patch a 4-byte rel32 operand of `E8`
* `REL32_JMP`: patch a 4-byte rel32 operand of `E9`
* `REL32_JCC`: patch a 4-byte rel32 operand of `0F 8?`
* `RIPREL32_LOAD`: patch a 4-byte disp32 of `mov rax, [rip+disp32]` (IAT pointer loads)
* `ABS64_IMM`: patch an 8-byte immediate (e.g., `mov rax, imm64`) when we materialize absolute addresses

Each record:

* `site_offset` (file/code offset of the 4 or 8 bytes)
* `target_symbol_id`
* `kind` (one of above)
* optional `addend`

---

## 1) Opcode Contracts (one entry per opcode)

> **Formatting legend**

* **Inputs (must be in)**: exact required location(s)
* **Output (produced in)**: exact produced location(s)
* **Clobbers**: registers overwritten by the opcode
* **Flags**: required flags state (or “sets flags” / “consumes flags”)
* **Bytes**: exact byte sequence(s) emitted (placeholders in brackets)
* **Relocs**: relocation records that must be emitted (if any)

---

## 1A) Structural / CFG

### `LABEL L`

* Inputs: —
* Output: —
* Clobbers: —
* Flags: —
* Bytes: *(no bytes; assembler label binding)*
* Relocs: —

### `JMP L`

* Inputs: —
* Output: —
* Clobbers: —
* Flags: —
* Bytes: `E9 [rel32(L)]`
* Relocs: `REL32_JMP(site=+1, target=L)`

### `BR_COND Lt, Lf`

**Contract:** condition already in `AL` (0/1).

* Inputs: `AL`
* Output: —
* Clobbers: flags only
* Flags: sets flags with `test`, then consumes flags by `Jcc`
* Bytes:

  * `84 C0` *(test al, al)*
  * `0F 85 [rel32(Lt)]` *(jne Lt)*
  * `E9 [rel32(Lf)]` *(jmp Lf)*
* Relocs:

  * `REL32_JCC(site=+4, target=Lt)` (the 4-byte operand of `0F 85`)
  * `REL32_JMP(site=+?+1, target=Lf)` (the 4-byte operand of `E9`)

### `RET`

* Inputs: —
* Output: —
* Clobbers: —
* Flags: —
* Bytes: *(epilogue emitted by proc emitter)* + `C3`
* Relocs: —

### `RET_RAX`

**Return scalar already in `RAX`.**

* Inputs: `RAX`
* Output: —
* Clobbers: —
* Flags: —
* Bytes: *(epilogue)* + `C3`
* Relocs: —

### `HALT`

* Inputs: —
* Output: —
* Clobbers: —
* Flags: —
* Bytes (canonical): `0F 0B` *(ud2)*
* Relocs: —

### `TRAP imm?`

* Inputs: optional imm in emitter metadata (not machine operand)
* Output: —
* Clobbers: —
* Flags: —
* Bytes (canonical): `0F 0B` *(ud2)*
  *(Optionally: `CC` for int3; choose one and standardize.)*
* Relocs: —

---

## 1B) Locals / Moves / Constants

### `LOAD_LOCAL64 slot(x) -> RAX`

* Inputs: `disp32(slot(x))`
* Output: `RAX`
* Clobbers: `RAX`
* Flags: —
* Bytes: `48 8B 84 24 [disp32]`
* Relocs: —

### `STORE_LOCAL64 slot(x) <- RAX`

* Inputs: `RAX`
* Output: —
* Clobbers: memory
* Flags: —
* Bytes: `48 89 84 24 [disp32]`
* Relocs: —

### `LOAD_LOCAL32 slot(x) -> EAX`

* Inputs: `disp32(slot(x))`
* Output: `EAX`
* Clobbers: `EAX`
* Flags: —
* Bytes: `8B 84 24 [disp32]`
* Relocs: —

### `STORE_LOCAL32 slot(x) <- EAX`

* Inputs: `EAX`
* Output: —
* Clobbers: memory
* Flags: —
* Bytes: `89 84 24 [disp32]`
* Relocs: —

### `CONST_I64 imm64 -> RAX`

* Inputs: `imm64`
* Output: `RAX`
* Clobbers: `RAX`
* Flags: —
* Bytes: `48 B8 [imm64]`
* Relocs: `ABS64_IMM(site=+2, target=const-or-symbol)` *(only if this imm64 is a symbol address; otherwise none)*

### `CONST_I32 imm32 -> EAX`

* Inputs: `imm32`
* Output: `EAX`
* Clobbers: `EAX`
* Flags: —
* Bytes: `B8 [imm32]`
* Relocs: —

### `CONST_BOOL_TRUE -> AL`

* Inputs: —
* Output: `AL=1`
* Clobbers: `AL`
* Flags: —
* Bytes: `B0 01`
* Relocs: —

### `CONST_BOOL_FALSE -> AL`

* Inputs: —
* Output: `AL=0`
* Clobbers: `AL`
* Flags: —
* Bytes: `30 C0` *(xor al, al)*
* Relocs: —

### `CONST_NULL -> RAX`

* Inputs: —
* Output: `RAX=0`
* Clobbers: `RAX`
* Flags: —
* Bytes: `48 31 C0` *(xor rax, rax)*
* Relocs: —

### `MOV_RAX_RBX`  *(copy RBX → RAX)*

* Inputs: `RBX`
* Output: `RAX`
* Clobbers: `RAX`
* Flags: —
* Bytes: `48 89 D8`
* Relocs: —

### `MOV_RBX_RAX` *(copy RAX → RBX)*

* Inputs: `RAX`
* Output: `RBX`
* Clobbers: `RBX`
* Flags: —
* Bytes: `48 89 C3`
* Relocs: —

### `MOVZX_AL_TO_EAX`

* Inputs: `AL`
* Output: `EAX`
* Clobbers: `EAX`
* Flags: —
* Bytes: `0F B6 C0`
* Relocs: —

---

## 1C) Addressing + Loads/Stores (non-MMIO)

> These assume the **address is already in RAX**.

### `LOAD_U32 [RAX] -> EAX`

* Inputs: `RAX=addr`
* Output: `EAX`
* Clobbers: `EAX`
* Flags: —
* Bytes: `8B 00`
* Relocs: —

### `LOAD_U64 [RAX] -> RAX`

* Inputs: `RAX=addr`
* Output: `RAX`
* Clobbers: `RAX`
* Flags: —
* Bytes: `48 8B 00`
* Relocs: —

### `STORE_U32 [RAX] <- EDX`

* Inputs: `RAX=addr`, `EDX=value`
* Output: —
* Clobbers: memory
* Flags: —
* Bytes: `89 10`
* Relocs: —

### `STORE_U64 [RAX] <- RDX`

* Inputs: `RAX=addr`, `RDX=value`
* Output: —
* Clobbers: memory
* Flags: —
* Bytes: `48 89 10`
* Relocs: —

### `ADDR_IMM64 imm64 -> RAX`

**Materialize absolute address.**

* Inputs: `imm64`
* Output: `RAX`
* Clobbers: `RAX`
* Flags: —
* Bytes: `48 B8 [imm64]`
* Relocs: `ABS64_IMM(site=+2, target=symbol)` *(if symbolic)*

### `ADDR_ADD_DISP32 disp32`  *(RAX += disp32)*

* Inputs: `RAX`, `disp32`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags (from add), not consumed unless immediately used
* Bytes: `48 05 [imm32]` *(add rax, imm32)*
* Relocs: —

### `LEA_RAX_RAX_DISP32 disp32` *(preferred; does not set flags)*

* Inputs: `RAX`, `disp32`
* Output: `RAX`
* Clobbers: `RAX`
* Flags: **does not change flags**
* Bytes: `48 8D 80 [disp32]`
* Relocs: —

---

## 1D) Integer Arithmetic / Bitwise / Shifts (i64)

> Binary ops require:

* LHS in `RAX`
* RHS in `RBX`
* Result in `RAX`

### `ADD_I64`

* Inputs: `RAX`, `RBX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 01 D8` *(add rax, rbx)*
* Relocs: —

### `SUB_I64`

* Inputs: `RAX`, `RBX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 29 D8` *(sub rax, rbx)*
* Relocs: —

### `MUL_I64` *(signed)*

* Inputs: `RAX`, `RBX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 0F AF C3` *(imul rax, rbx)*
* Relocs: —

### `NEG_I64`

* Inputs: `RAX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 F7 D8` *(neg rax)*
* Relocs: —

### `AND_I64`

* Inputs: `RAX`, `RBX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 21 D8`
* Relocs: —

### `OR_I64`

* Inputs: `RAX`, `RBX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 09 D8`
* Relocs: —

### `XOR_I64`

* Inputs: `RAX`, `RBX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 31 D8`
* Relocs: —

### `BITNOT_I64`

* Inputs: `RAX`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 F7 D0` *(not rax)*
* Relocs: —

### `SHL_I64`

* Inputs: `RAX`, `CL`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 D3 E0` *(shl rax, cl)*
* Relocs: —

### `SHR_I64`

* Inputs: `RAX`, `CL`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 D3 E8` *(shr rax, cl)*
* Relocs: —

### `SAR_I64`

* Inputs: `RAX`, `CL`
* Output: `RAX`
* Clobbers: `RAX`, flags
* Flags: sets flags
* Bytes: `48 D3 F8` *(sar rax, cl)*
* Relocs: —

### `DIVMOD_I64` *(signed; quotient->RAX remainder->RDX)*

* Inputs: `RAX=numerator`, `RBX=denominator`
* Output: `RAX=quotient`, `RDX=remainder`
* Clobbers: `RAX`, `RDX`, flags
* Flags: undefined after idiv (do not consume)
* Bytes:

  * `48 99` *(cqo)*
  * `48 F7 FB` *(idiv rbx)*
* Relocs: —

---

## 1E) Compare + SetCC + Boolean Ops

### `CMP_I64`  *(sets flags for following SETcc / CMOV / Jcc)*

* Inputs: `RAX`, `RBX`
* Output: flags
* Clobbers: flags only
* Flags: **sets flags** (required by subsequent op)
* Bytes: `48 39 D8` *(cmp rax, rbx)*
* Relocs: —

> **SETcc contract:** must immediately follow a flag-producing compare/test unless we intentionally preserve flags.

### `SET_LT -> AL`

* Inputs: flags from `CMP_I64` (signed LT)
* Output: `AL`
* Clobbers: `AL`
* Flags: consumes flags (reads)
* Bytes: `0F 9C C0`
* Relocs: —

### `SET_LE -> AL`

* Inputs: flags
* Output: `AL`
* Clobbers: `AL`
* Flags: consumes flags
* Bytes: `0F 9E C0`

### `SET_GT -> AL`

* Bytes: `0F 9F C0`

### `SET_GE -> AL`

* Bytes: `0F 9D C0`

### `SET_EQ -> AL`

* Bytes: `0F 94 C0`

### `SET_NE -> AL`

* Bytes: `0F 95 C0`

### `TEST_AL_AL` *(useful before BR_COND or CMOVZ pattern)*

* Inputs: `AL`
* Output: flags
* Clobbers: flags
* Flags: sets ZF based on AL
* Bytes: `84 C0`

### `BOOL_NOT_AL` *(logical not; AL = (AL==0))*

* Inputs: `AL`
* Output: `AL`
* Clobbers: `AL`, flags
* Flags: sets flags internally
* Bytes (canonical):

  * `84 C0` *(test al,al)*
  * `0F 94 C0` *(setz al)*
* Relocs: —

---

## 1F) Select / Choose (branchless)

### `SELECT_I64` *(RAX = cond ? RAX : RBX)*

* Inputs: `AL=cond`, `RAX=trueVal`, `RBX=falseVal`
* Output: `RAX`
* Clobbers: flags
* Flags: uses flags from `test al,al` inside
* Bytes:

  * `84 C0` *(test al,al)*
  * `48 0F 44 C3` *(cmovz rax, rbx)*  ; if cond==0, take RBX
* Relocs: —

### `CHOOSE_MAX_I64` *(RAX = max(RAX,RBX))*

* Inputs: `RAX=a`, `RBX=b`
* Output: `RAX`
* Clobbers: flags
* Flags: requires `cmp` immediately before cmov (emitted here)
* Bytes:

  * `48 39 D8` *(cmp rax,rbx)*
  * `48 0F 4C C3` *(cmovl rax,rbx)* ; if a<b, rax=b
* Relocs: —

### `CHOOSE_MIN_I64` *(RAX = min(RAX,RBX))*

* Inputs: `RAX=a`, `RBX=b`
* Output: `RAX`
* Clobbers: flags
* Bytes:

  * `48 39 D8`
  * `48 0F 4F C3` *(cmovg rax,rbx)* ; if a>b, rax=b
* Relocs: —

---

## 1G) Calls (internal + IAT) + Shadow/Align

### `CALL_INTERNAL target`

**Contract:** arguments already placed in ABI regs/stack by lowering.

* Inputs: `RCX,RDX,R8,R9` (+ stack args), `RSP` aligned
* Output: `RAX` (return)
* Clobbers: `RAX, RCX, RDX, R8, R9, R10, R11` (per ABI volatile), flags undefined
* Flags: do not rely on flags across calls
* Bytes (canonical call wrapper):

  * `48 83 EC 40` *(sub rsp, 0x40)*  ; 0x20 shadow + 0x20 align/padding (canonical)
  * `E8 [rel32(target)]`
  * `48 83 C4 40` *(add rsp, 0x40)*
* Relocs:

  * `REL32_CALL(site=+1 after E8 opcode, target=target)`

### `CALL_IAT __imp_F`

**Contract:** imported function pointer lives at `__imp_F` in `.rdata/.idata`.

* Inputs: args in ABI regs/stack
* Output: `RAX`
* Clobbers: `RAX` (also ABI volatiles), flags undefined
* Flags: do not rely on flags across calls
* Bytes:

  * `48 83 EC 40`
  * `48 8B 05 [riprel32(__imp_F)]` *(mov rax, [rip+disp32])*
  * `FF D0` *(call rax)*
  * `48 83 C4 40`
* Relocs:

  * `RIPREL32_LOAD(site=disp32 of mov, target=__imp_F)`
  * *(No REL32_CALL here because call is indirect `FF D0`)*

### `CALL_THUNK thunk_F`

**Contract:** thunk is an internal label/proc that performs IAT load then tailcalls.

* Inputs/Output/Clobbers: same as call
* Bytes:

  * `48 83 EC 40`
  * `E8 [rel32(thunk_F)]`
  * `48 83 C4 40`
* Relocs: `REL32_CALL(target=thunk_F)`

---

## 1H) Assert / Diagnostics at Runtime Boundary

### `ASSERT_AL msgid`

* Inputs: `AL` (cond), optional `msgid` known to compiler for debug tables
* Output: —
* Clobbers: flags
* Flags: consumes flags from `test`
* Bytes:

  * `84 C0` *(test al,al)*
  * `0F 85 [rel32(L_ok)]`
  * `0F 0B` *(ud2)*
  * `L_ok:`
* Relocs:

  * `REL32_JCC(target=L_ok)`

*(If we prefer “assert calls a runtime reporter then traps”, swap ud2 with CALL_IAT of reporter; record reloc accordingly.)*

---

## 1I) MMIO (volatile semantics)

### `MMIO_READ32`

* Inputs: `RAX=addr`
* Output: `EAX=value`
* Clobbers: `EAX`
* Flags: —
* Bytes: `8B 00`
* Relocs: —
* **OSW barrier rule:** this op is a **memory barrier**; do not reorder across it.

### `MMIO_WRITE32`

* Inputs: `RAX=addr`, `EDX=value`
* Output: —
* Clobbers: memory
* Flags: —
* Bytes: `89 10`
* Relocs: —
* Barrier: yes

---

## 1J) Concurrency / Channels / Mutex (runtime-call ops)

These are “call ops” with fixed import targets. Treat them as `CALL_IAT` plus fixed symbol.

### `THREAD_SPAWN`

* Inputs: ABI regs per runtime signature
* Output: `RAX=handle`
* Clobbers: ABI volatiles, flags undefined
* Bytes: `CALL_IAT __imp_rane_rt_threads_spawn_proc` *(using the CALL_IAT template above)*
* Relocs: `RIPREL32_LOAD(target=__imp_rane_rt_threads_spawn_proc)`

### `THREAD_JOIN_I64`

* Inputs: `RCX=handle` (canonical)
* Output: `RAX=i64`
* Clobbers: ABI volatiles
* Bytes: `CALL_IAT __imp_rane_rt_threads_join_i64`
* Relocs: `RIPREL32_LOAD(target=__imp_rane_rt_threads_join_i64)`

### `MUTEX_LOCK`

* Inputs: `RCX=mutex*`
* Output: —
* Clobbers: ABI volatiles
* Bytes: `CALL_IAT __imp_rane_rt_threads_mutex_lock`
* Relocs: `RIPREL32_LOAD(target=__imp_rane_rt_threads_mutex_lock)`
* Barrier: yes

### `MUTEX_UNLOCK`

* Inputs: `RCX=mutex*`
* Output: —
* Clobbers: ABI volatiles
* Bytes: `CALL_IAT __imp_rane_rt_threads_mutex_unlock`
* Relocs: `RIPREL32_LOAD(target=__imp_rane_rt_threads_mutex_unlock)`
* Barrier: yes

### `CHAN_SEND_I64` *(example typed channel send)*

* Inputs: `RCX=chan*`, `RDX=value` (or per signature)
* Output: —
* Clobbers: ABI volatiles
* Bytes: `CALL_IAT __imp_rane_rt_channels_send_i64`
* Relocs: `RIPREL32_LOAD(target=__imp_rane_rt_channels_send_i64)`
* Barrier: yes

### `CHAN_RECV_I64`

* Inputs: `RCX=chan*`
* Output: `RAX=value`
* Clobbers: ABI volatiles
* Bytes: `CALL_IAT __imp_rane_rt_channels_recv_i64`
* Relocs: `RIPREL32_LOAD(target=__imp_rane_rt_channels_recv_i64)`
* Barrier: yes

---

## 1K) “Structured” constructs that must be lowered to these opcodes (no direct emission)

These do **not** have direct bytes; they must be lowered by CIAM/Resolver into CFG + calls + labels:

* `IF / ELSE` → `CMP/SET/BR_COND/JMP/LABEL`
* `WHILE / FOR` → labels + cond branches + backedges
* `MATCH / SWITCH / DECIDE` → compare chain or jump table (when we add jump tables, that becomes a new opcode contract)
* `TRY / CATCH / FINALLY` → region lowering into labels + calls + cleanup edges
* `WITH / DEFER / LOCK` → `TRY/FINALLY` lowering + call ops
* `ASYNC / AWAIT` → state machine lowering (will introduce new opcodes once we formalize them)
* `ASM` → either “opaque bytes block” or parsed mini-ops; not part of Typed CIL opcode contract unless we define it as such

---

# 2) Minimal “Relocation Emission Rules” (exact)

Whenever an opcode contains a placeholder, emit the relocation:

* `E8 [rel32(sym)]` → `REL32_CALL` at the 4-byte operand
* `E9 [rel32(label)]` → `REL32_JMP` at the 4-byte operand
* `0F 8? [rel32(label)]` → `REL32_JCC` at the 4-byte operand
* `48 8B 05 [riprel32(__imp)]` → `RIPREL32_LOAD` at the disp32
* `48 B8 [imm64(sym)]` → `ABS64_IMM` at the imm64 (only if imm is a symbol address)

---

# 3) Quick “Clobber Truth Table” (so OSW can reason)

* Any `CALL_*` clobbers: `RAX RCX RDX R8 R9 R10 R11` (and flags undefined)
* Any arithmetic op clobbers: destination reg + flags
* `LEA` does **not** clobber flags
* `CMP/TEST` clobbers flags only
* `SETcc` clobbers `AL` (and reads flags)

---

* **required input locations** (reg/stack)
* **produced output location**
* **clobbered regs**
* **required flags** (if any)
* **emitted bytes sequence(s)** (bytes-level, with placeholders)
* **relocation records generated** (if any)
We’ll also include the **canonical lowering contracts** (CIAM mapping) where the opcode exists specifically to complete the surface syntax.

---

# RANE Emitter Contract Sheet v2 (Windows x86-64 PE)
## 0) Global backend invariants (apply to all opcodes)
### 0.1 Windows x64 ABI invariants
* **Integer/pointer args 0..3** in: `RCX, RDX, R8, R9`
* **Return integer/pointer** in: `RAX`
* **Caller allocates 32 bytes shadow space** before `call` (always).
* **RSP must be 16-byte aligned at the `call` instruction**.
* **Volatile (caller-saved):** `RAX, RCX, RDX, R8, R9, R10, R11`
* **Non-volatile (callee-saved):** `RBX, RBP, RSI, RDI, R12–R15`
### 0.2 Location model
Every SSA-ish “value” in Typed CIL is materialized as one of:
* `Reg64(rX)` (one of the general regs)
* `Reg32(rX)` (low 32-bit reg view)
* `Stack64([rsp+disp])`
* `Stack32([rsp+disp])`
* `Imm32/Imm64`
* `AddrRipRel([rip+disp32])` (for globals, imports, rodata)
### 0.3 Frame planner contract (inputs to emitter)
Emitter receives per-proc:
* `FRAME_SIZE` (includes locals/temps/spills)
* `SHADOW_SIZE = 32`
* `ALIGN_PAD` so that `rsp` is 16-aligned at calls
* `stack_slot(value_id) -> disp32`
### 0.4 Relocation record types (canonical)
* `Rel32_Call` — patch a `call rel32` immediate (4 bytes)
* `Rel32_Jmp` — patch `jmp rel32` or `jcc rel32` immediate
* `Rel32_RipDisp` — patch a RIP-relative disp32 for `[rip+disp32]`
* `Abs64_Imm` — patch an imm64 (8 bytes), used rarely (absolute pointers)

---

# 1) New Typed CIL opcodes needed to fully cover the syntax
Below are the **missing families** that make the sample fully specifiable.

---

## 1.1 Boolean materialization and branch
### OPCODE: `BOOL_FROM_FLAGS(cc)`
Used after `CMP_*` to produce a `bool` value in `AL`.
* **Required inputs:** flags set by a prior `CMP/TEST` (same basic block, no clobber in-between)
* **Output:** `Reg8(AL)` (canonical bool 0/1)
* **Clobbers:** `RAX` (low byte written), flags preserved
* **Flags required:** yes (from immediately preceding compare/test)
* **Emission:**
* `SETcc AL`
Bytes vary by cc:
* `sete al` = `0F 94 C0`
* `setne al` = `0F 95 C0`
* `setl al` (signed <) = `0F 9C C0`
* `setb al` (unsigned <) = `0F 92 C0`
* etc.
* `MOVZX EAX, AL` (if we normalize to i32/i64 later)
* `0F B6 C0`
* **Relocs:** none
* **Canonical errors:**
* `RANE_DIAG_INTERNAL_ERROR`: “BOOL_FROM_FLAGS without prior flags producer”
### OPCODE: `BR_BOOL(label_true, label_false)`
Branches based on `AL` (or any canonical bool value).
* **Required inputs:** `Reg8(AL)` contains 0/1
* **Output:** control flow only
* **Clobbers:** flags
* **Flags required:** no
* **Emission:**
* `TEST AL, AL` → `84 C0`
* `JNE rel32` to true → `0F 85 xx xx xx xx` (Rel32_Jmp)
* `JMP rel32` to false → `E9 xx xx xx xx` (Rel32_Jmp)
* **Relocs:** `Rel32_Jmp` for both branches
**Surface lowering:**
`goto (expr) -> Lt Lf` ⇒ `emit(expr) -> bool in AL` ⇒ `BR_BOOL Lt Lf`

---

# 1.2 Call argument placement (this is the big missing piece)
We introduce explicit arg-move opcodes so the backend is deterministic.
### OPCODE: `ARG_CLEAR(argc)`
Reserves argument staging area for stack args (beyond 4) and guarantees shadow+alignment are in effect.
* **Required inputs:** current `RSP` points to frame base (after prologue)
* **Output:** stack space reserved for call args
* **Clobbers:** `RSP`
* **Flags:** none
* **Emission:** (if stack args exist)
* `SUB RSP, call_arg_area_size`
`48 81 EC imm32`
* **Relocs:** none
*(If no stack args, ARG_CLEAR can be a no-op; shadow space is already counted in frame planner.)*
### OPCODE: `ARG_I64(index, src)`
Places argument `index` (0-based) into ABI location.
* **Required inputs:** `src` in Reg64/Stack64/Imm
* **Output:** arg in one of RCX/RDX/R8/R9 or `[rsp+shadow+arg_off]`
* **Clobbers:** `RAX` (as scratch if needed)
* **Flags:** none
* **Emission patterns:**
* If `index==0`: move to RCX
* `MOV RCX, r64` = `48 89 C1` (from RAX example; actual ModRM depends)
* `MOV RCX, [rsp+disp]` = `48 8B 8C 24 disp32`
* `MOV RCX, imm64` = `48 B9 imm64` (**Abs64_Imm** patch)
* If `index==1`: to RDX (`48 89 D1` etc)
* If `index==2`: to R8 (`49 89 C0` style)
* If `index==3`: to R9 (`49 89 C1` style)
* If `index>=4`: store to `[rsp+shadow+8*(index-4)]`
* `MOV [rsp+disp32], r64` = `48 89 84 24 disp32`
* `MOV [rsp+disp32], imm32` = `48 C7 84 24 disp32 imm32` (sign-extended imm32)
* **Relocs:** only if using imm64 (rare; prefer rip-relative constants for pointers)
* **Canonical errors:**
* `RANE_DIAG_TYPE_MISMATCH`: arg width mismatch (e.g., trying to pass struct by value without ABI rule)
* `RANE_DIAG_INTERNAL_ERROR`: index out of bounds for computed argc
### OPCODE: `CALL_DIRECT(sym, argc)`
Direct call to known internal proc.
* **Required inputs:** args already placed by `ARG_*`
* **Output:** return in `RAX` (if non-void)
* **Clobbers:** volatile regs per ABI (assume RCX/RDX/R8/R9/R10/R11/RAX)
* **Flags:** none
* **Emission:**
* `CALL rel32` = `E8 xx xx xx xx`
→ **Rel32_Call** relocation to `sym`
* **Relocs:** `Rel32_Call`
### OPCODE: `CALL_IMPORT(imp_sym, argc)`
Call imported function via IAT thunk pointer.
* **Required inputs:** args already placed
* **Output:** `RAX`
* **Clobbers:** volatile regs, plus `RAX` used for the indirect call
* **Flags:** none
* **Emission canonical (CFG/DEP friendly):**
1. `MOV RAX, [RIP+disp32]` = `48 8B 05 dd dd dd dd` → **Rel32_RipDisp** to IAT slot
2. `CALL RAX` = `FF D0`
* **Relocs:** `Rel32_RipDisp`
### OPCODE: `CALL_END()`
Restores stack if `ARG_CLEAR` subtracted extra for stack args.
* **Required inputs:** call_arg_area_size known
* **Output:** stack restored
* **Clobbers:** `RSP`
* **Flags:** none
* **Emission:** `ADD RSP, imm32` = `48 81 C4 imm32` (if used)

---

# 1.3 Integer width conversion (needed for u32/u16/i32 fields)
### OPCODE: `ZEXT_U32_TO_U64(src32)`
* **Input:** `Reg32(rX)` or `Stack32([rsp+disp])`
* **Output:** `Reg64(rX)` (canonical: same reg, upper cleared)
* **Clobbers:** none (if reg form), else `RAX` scratch
* **Flags:** none
* **Emission:**
* If src is `Reg32`: **no-op** (writing to a 32-bit reg zero-extends automatically)
* If src is stack: `MOV EAX, [rsp+disp32]` = `8B 84 24 disp32` then treat `RAX` as zero-extended
* **Relocs:** none
### OPCODE: `SEXT_I32_TO_I64(src32)`
* **Input:** `Reg32` or `Stack32`
* **Output:** `Reg64` (canonical `RAX` if stack)
* **Clobbers:** `RAX` (if needed)
* **Flags:** none
* **Emission:**
* Reg form: `MOVSXD r64, r/m32` = `48 63 /r`
* e.g. `movsxd rax, ecx` bytes depend on ModRM
* Stack form: `MOVSXD RAX, [rsp+disp32]` = `48 63 84 24 disp32`
* **Relocs:** none
### OPCODE: `TRUNC_I64_TO_I32(src64)`
* **Input:** `Reg64` or `Stack64`
* **Output:** `Reg32` (low 32 bits)
* **Clobbers:** `RAX` if stack
* **Emission:**
* Reg form: **view** as 32-bit reg (no instruction)
* Stack form: `MOV EAX, [rsp+disp32]` loads low 32 bits

---

# 1.4 Field addressing and aggregate lawet (struct/union/variant)
We standardize lawet rules so offsets are deterministic.
## 1.4.1 Lawet rules (must be fixed to emit correct bytes)
* Struct lawet: C-like
* fields in order
* each field aligned to its natural alignment (or overridden)
* struct alignment = max(field align)
* struct size padded to struct alignment
* Union lawet:
* all fields at offset 0
* size = max(field size), align = max(field align)
* Enum:
* stored as underlying repr type
* Variant (tagged union):
* canonical lawet:
* `tag: repr` at offset 0
* `payload: union` at offset `align_up(sizeof(tag), payload_align)`
* size padded to overall align
We need this because we do:
* `h.magic`, `h.version`, `h.size`
* `u.i`, `u.f`
* `Maybe<T>` pattern match
## 1.4.2 Field opcodes
### OPCODE: `ADDR_LOCAL(slot_disp32)`
Compute address of a local/stack object.
* **Input:** none
* **Output:** `Reg64(RAX)` = `rsp + disp32`
* **Clobbers:** `RAX`
* **Flags:** none
* **Emission:** `LEA RAX, [RSP+disp32]` = `48 8D 84 24 disp32`
* **Relocs:** none
### OPCODE: `ADDR_FIELD(base_addr_reg, field_off32)`
* **Input:** `Reg64(base)` points to struct
* **Output:** `Reg64(RAX)` points to field
* **Clobbers:** `RAX`
* **Flags:** none
* **Emission:** `LEA RAX, [base + off32]`
* If base is RAX already: `48 8D 80 off32`
* General: `48 8D 80/81 …` (ModRM depends)
* **Relocs:** none
### OPCODE: `LOAD_FIELD_I64(base_addr_reg, off32)`
* **Input:** `Reg64(base)`
* **Output:** `Reg64(RAX)` (or chosen dest reg)
* **Clobbers:** dest
* **Flags:** none
* **Emission:** `MOV RAX, [base+off32]`
* example base=RAX: `48 8B 80 off32`
* **Relocs:** none
### OPCODE: `STORE_FIELD_I64(base_addr_reg, off32, src)`
* **Input:** base reg + src reg/imm
* **Output:** memory updated
* **Clobbers:** `RAX` if src is imm64 handling required
* **Emission:**
* `MOV [base+off32], r64` = `48 89 80 off32`
* `MOV QWORD PTR [base+off32], imm32` = `48 C7 80 off32 imm32`
* **Relocs:** none
*(Same family exists for I32/U32/U16/U8/F32/F64; each has specific MOV forms.)*

---

# 1.5 Array indexing and address calculation
### OPCODE: `ADDR_INDEX(base_addr_reg, idx_reg, elem_size_pow2, base_off32)`
Computes `base + base_off + idx * elem_size`.
* **Input:** base reg, idx reg (i64)
* **Output:** `RAX` address
* **Clobbers:** `RAX`
* **Flags:** none
* **Emission (scale must be 1,2,4,8):**
* `LEA RAX, [base + idx*scale + off32]`
* For scale=8 common: `48 8D 84 C1 off32` style (ModRM/SIB depend)
* **Relocs:** none
* **Canonical errors:**
* `RANE_DIAG_TYPE_MISMATCH`: elem_size not supported by LEA scale (must be 1/2/4/8); else fallback requires IMUL sequence opcode.
### OPCODE: `LOAD_AT_I64(addr_reg)`
* **Input:** `Reg64(addr)`
* **Output:** `RAX`
* **Clobbers:** `RAX`
* **Emission:** `MOV RAX, [addr]` = `48 8B 00`
### OPCODE: `STORE_AT_I64(addr_reg, src)`
* **Input:** addr + src
* **Output:** memory updated
* **Emission:** `MOV [addr], r64` = `48 89 00`

---

# 1.6 MMIO region addressing (the surface form needs this)
### OPCODE: `MMIO_ADDR(region_base_imm32, offset_imm32)`
Computes absolute address for MMIO access.
* **Input:** none
* **Output:** `RAX = region_base + offset`
* **Clobbers:** `RAX`
* **Flags:** none
* **Emission:**
* `MOV EAX, imm32` = `B8 imm32` (zero-extends into RAX)
* `ADD EAX, imm32` = `05 imm32` (still zero-extended in RAX)
* **Relocs:** none
### OPCODE: `MMIO_READ32(addr_reg)`
* **Input:** `RAX` holds address
* **Output:** `EAX` contains loaded u32
* **Clobbers:** `RAX`
* **Flags:** none
* **Emission:** `MOV EAX, DWORD PTR [RAX]` = `8B 00`
* **Note:** mark as **volatile** in optimizer (OSW must not delete/reorder across barriers)
* **Relocs:** none
### OPCODE: `MMIO_WRITE32(addr_reg, src32)`
* **Input:** `RAX` addr, `src` in reg/imm
* **Output:** memory written
* **Clobbers:** flags
* **Emission:**
* reg: `MOV [RAX], r32` = `89 00`
* imm32: `C7 00 imm32`
* **Relocs:** none
**Surface lowering:**
`read32 REG 0 into x` ⇒ `MMIO_ADDR(4096,0)` ⇒ `MMIO_READ32` ⇒ store to local `x`

---

# 1.7 `spawn/join/send/recv/lock` as deterministic runtime calls
To keep the backend mechanically specifiable, we define them as **CIAM-lowered** to imports with fixed names.
## Canonical runtime imports (example ABI)
* `rane_rt_threads.spawn_proc_i64(proc_ptr, arg0_i64) -> thread_handle`
* `rane_rt_threads.join_i64(thread_handle) -> i64`
* `rane_rt_channels.send_i64(chan_handle, val_i64) -> void`
* `rane_rt_channels.recv_i64(chan_handle) -> i64`
* `rane_rt_threads.mutex_lock(mutex_handle) -> void`
* `rane_rt_threads.mutex_unlock(mutex_handle) -> void`
### OPCODE: `SPAWN_I64(proc_sym, arg0)`
Lowered to: ARG + CALL_IMPORT `spawn_proc_i64`
* **Required inputs:** `proc_sym` addressable (either function pointer constant or symbol)
* **Output:** `RAX` thread_handle
* **Clobbers:** volatile regs
* **Emission:** sequence of `ARG_*` + `CALL_IMPORT`
* **Relocs:** rip-disp to IAT slot + maybe a symbol address relocation if proc passed by pointer
### OPCODE: `JOIN_I64(handle)`
Lowered to: ARG + CALL_IMPORT `join_i64`
### OPCODE: `CHAN_SEND_I64(ch, val)`
Lowered to: ARG + CALL_IMPORT `send_i64`
### OPCODE: `CHAN_RECV_I64(ch)`
Lowered to: ARG + CALL_IMPORT `recv_i64`
### OPCODE: `LOCK_BEGIN(mutex)` / `LOCK_END(mutex)`
Lowered to:
* lock: `CALL_IMPORT mutex_lock`
* unlock in guaranteed cleanup path: `CALL_IMPORT mutex_unlock`

---

# 1.8 `with`, `defer`, `try/catch/finally`, `throw` (mechanical lowering)
To keep this emitter contract purely codegen-focused, we adopt a **deterministic, compile-time rewritable model**:
### Model A (simplest, matches “compile-time centric”): no stack unwinding, only explicit control flow
* `try/catch/finally` is lowered into explicit CFG with **error codes**, not OS exceptions.
* `throw X` becomes `SET_ERR(X); JMP catch_label`
* `finally` runs on both normal exit and error exit.
This requires a canonical hidden local:
* `__err: i32` initialized to 0
* `__has_err: bool` or `__err != 0`
#### New opcodes
### OPCODE: `SET_ERR_I32(src)`
* **Input:** src i32 in reg/imm
* **Output:** store into `Stack32([rsp+err_slot])`
* **Clobbers:** none
* **Emission:** `MOV [rsp+disp32], imm32` or `MOV [rsp+disp32], r32`
### OPCODE: `GET_ERR_I32()`
* **Input:** none
* **Output:** `EAX`
* **Emission:** `MOV EAX, [rsp+err_disp32]`
### OPCODE: `CLEAR_ERR()`
* **Emission:** store 0
**Lowering template:**
Surface:
```rane
try:
S_try
catch e:
S_catch
finally:
S_fin
end
```
Lowered CFG (labels):
* init: `CLEAR_ERR`
* `L_try`: S_try
* on normal: `JMP L_fin_norm`
* `L_throw`: `SET_ERR`; `JMP L_catch`
* `L_catch`: load err into temp `e`; S_catch; `JMP L_fin_err`
* `L_fin_norm`: S_fin; `JMP L_end`
* `L_fin_err`: S_fin; `JMP L_end`
* `L_end`: continue
This makes codegen fully mechanical using only:
* stores/loads/branches/jumps

---

# 1.9 `asm:` with local name binding
We used:
```rane
asm:
mov rax 1
add rax 2
mov out rax
end
```
To keep it specifiable, define this rule:
* Inside `asm:` block, `mov <local_name> <reg>` is **not raw assembly**; it is a **RANE asm directive** lowered to `STORE_LOCAL_*` opcodes.
* Conversely, `mov <reg> <imm>` etc remains raw text passed to assembler.
#### Opcode for directive:
### OPCODE: `ASM_STORE_LOCAL_I64(slot_disp32, src_reg)`
* **Input:** src reg (e.g., RAX)
* **Output:** stack slot written
* **Emission:** `MOV [RSP+disp32], RAX` = `48 89 84 24 disp32`

---

# 1.10 `#symbol` (compile-time symbol IDs)
Define `#NAME` as a **u32 symbol ID** generated by the resolver.
### OPCODE: `SYM_ID(name)`
* **Input:** none
* **Output:** `EAX = sym_id_u32`
* **Clobbers:** `EAX`
* **Emission:** `MOV EAX, imm32` = `B8 imm32`
* **Relocs:** none (ID is compile-time constant)
* **Errors:** undefined symbol token → `RANE_DIAG_UNDEFINED_NAME`

---

# 2) The `=` ambiguity — mandatory rule so the backend is consistent
We must choose one of these; otherwise emission can’t be fully deterministic.
## Rule (recommended): `=` is **statement-only assignment**, never an expression
* Equality is only `==`
* Therefore: `let c6 = a = b` is a **compile-time parse/type error**
**Diagnostic:**
* `RANE_DIAG_PARSE_ERROR` or `RANE_DIAG_TYPE_MISMATCH`
* Message: “assignment `=` is not an expression; did we mean `==`?”
This single choice makes the entire contract sheet sane.

---

# 3) “Typed CIL opcode → exact emission template(s)” table (core + v2 additions)
Below is a compact “one row per opcode” mapping for the new v2 opcodes (the ones the syntax needed).
(The original v1 core ops still apply; these are the extensions.)
### Table: v2 Opcode → Emission Templates
| Opcode | Template (bytes-level) | Relocs |
| 

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

---

-- |
| `BOOL_FROM_FLAGS(cc)` | `0F 9? C0` (+ optional `0F B6 C0`) | none |
| `BR_BOOL(T,F)` | `84 C0` ; `0F 85 rel32` ; `E9 rel32` | `Rel32_Jmp` |
| `ARG_CLEAR(n)` | `48 81 EC imm32` | none |
| `ARG_I64(0,src)` | move src→RCX | maybe Abs64 if imm64 |
| `ARG_I64(1,src)` | move src→RDX | maybe Abs64 |
| `ARG_I64(2,src)` | move src→R8 | maybe Abs64 |
| `ARG_I64(3,src)` | move src→R9 | maybe Abs64 |
| `ARG_I64(k>=4,src)` | `48 89 84 24 disp32` (or imm32 store) | none |
| `CALL_DIRECT(sym)` | `E8 rel32` | `Rel32_Call` |
| `CALL_IMPORT(iat)` | `48 8B 05 disp32` ; `FF D0` | `Rel32_RipDisp` |
| `CALL_END()` | `48 81 C4 imm32` | none |
| `ZEXT_U32_TO_U64(r32)` | no-op | none |
| `SEXT_I32_TO_I64(r/m32)` | `48 63 /r` | none |
| `ADDR_LOCAL(disp32)` | `48 8D 84 24 disp32` | none |
| `ADDR_FIELD(base,off32)` | `48 8D 80 off32` (base=RAX) | none |
| `LOAD_FIELD_I64(base,off32)` | `48 8B 80 off32` | none |
| `STORE_FIELD_I64(base,off32,src)` | `48 89 80 off32` | none |
| `ADDR_INDEX(base,idx,scale,off)` | `48 8D 84 ?? off32` | none |
| `LOAD_AT_I64(addr)` | `48 8B 00` | none |
| `STORE_AT_I64(addr,src)` | `48 89 00` | none |
| `MMIO_ADDR(base,off)` | `B8 imm32` ; `05 imm32` | none |
| `MMIO_READ32()` | `8B 00` | none |
| `MMIO_WRITE32(src)` | `89 00` or `C7 00 imm32` | none |
| `SET_ERR_I32` | `C7 84 24 disp32 imm32` | none |
| `GET_ERR_I32` | `8B 84 24 disp32` | none |
| `CLEAR_ERR` | store 0 | none |
| `SYM_ID(name)` | `B8 imm32` | none |
| `ASM_STORE_LOCAL_I64(disp, reg)` | `48 89 84 24 disp32` | none |

---

# 4) What this v2 sheet now covers from the exact syntax
With the above additions, the sample becomes mechanically specifiable for:
✅ **Calls with any arg count** (`add5`, runtime functions, etc.)
✅ **u32/u16/i32 field loads/stores** (via field ops + ext rules)
✅ **struct field updates** (`set h.version to 2`, `add h.size by 512` → load/add/store field)
✅ **arrays indexing** (`arr[0]`)
✅ **mmio region operations** (`read32 REG 0 into x`)
✅ **spawn/join/send/recv/lock** (as deterministic CIAM lowering to imports)
✅ **try/catch/finally/throw** (via deterministic error-code CFG lowering)
✅ **goto (cond) -> L1 L2** (bool materialize + BR_BOOL)
✅ **#symbol** constants
✅ **asm: … mov out rax** (directive lowering)
The only “big” remaining unsolved area (if we want literal fidelity) is:
* **variants/pattern matching (`Maybe<T>`, `case Some x`)**
This needs a tagged-union lawet + `MATCH_VARIANT_TAG` style opcodes, and optional jump-table lowering.

---

# 5) One last hard requirement
To keep the whole pipeline deterministic:
### Mandate:
* `=` is **not** an expression.
* `let c6 = a = b` is a compile-time error.
Otherwise we’ll end up with a backend contract that can’t typecheck `c6` as `bool` without inventing coercions.

---

variants now, fully bytes-level + mechanically specifiable.
Below is the Variant/Match extension to the Emitter Contract Sheet v2 that covers:
•	variant Maybe<T>: Some T / None
•	let m1 Maybe<i64> = Some 123
•	let m2 Maybe<i64> = None
•	match m1: case Some x: … case None: … end
•	payload binding (x)
•	lawet + tag rules + exact x64 emission patterns
•	canonical diagnostics + OSW hooks
________________________________________
1) Canonical Variant Memory Lawet (must be fixed)
1.1 Variant lawet model (tagged union)
Every variant V<T...> lowers to a concrete monomorphized lawet (no runtime reflection needed).
Lawet fields:
•	tag at offset TAG_OFF = 0
•	payload at offset PAY_OFF = align_up(TAG_SIZE, PAY_ALIGN)
•	TAG_SIZE = sizeof(tag_repr) (default u8)
•	PAY_ALIGN = max(align(payload_alts...))
•	PAY_SIZE = max(size(payload_alts...))
•	SIZE = align_up(PAY_OFF + PAY_SIZE, ALIGN)
•	ALIGN = max(align(tag_repr), PAY_ALIGN)
Tag representation (default):
•	tag_repr = u8 unless explicitly overridden (we didn’t override it in sample)
1.2 Tag values (canonical)
For a variant with N alternatives, tags are:
•	alt[0] tag = 0
•	alt[1] tag = 1
•	…
•	alt[i] tag = i
So for:
variant Maybe<T>:
  Some T
  None
end
Canonical mapping:
•	None tag = 0
•	Some tag = 1
1.3 Example: Maybe<i64> lawet (wer sample)
•	tag: u8 (size 1, align 1)
•	payload union: only i64 (size 8, align 8)
•	PAY_OFF = align_up(1, 8) = 8
•	SIZE = align_up(8 + 8, 8) = 16
•	ALIGN = 8
Offsets:
•	TAG_OFF = 0
•	PAY_OFF = 8
________________________________________
2) Required Typed CIL Concepts (so backend is deterministic)
Typed CIL must carry, for each variant instantiation:
•	lawet_id
•	tag_off (always 0 in this model)
•	pay_off
•	size, align
•	per-alt payload type/size/align (or just union max)
This is compile-time; emitter only needs constants like PAY_OFF.
________________________________________
3) Variant/Match Opcode Family (Emitter Contract)
These opcodes are added to wer Typed CIL catalog.
3.1 Address + tag ops
OPCODE: VAR_ADDR_LOCAL(lawet_id, slot_disp32) -> RAX
Compute address of a variant local on stack.
•	Inputs: slot_disp32
•	Output: RAX = &local_variant
•	Clobbers: RAX
•	Flags: none
•	Bytes: 48 8D 84 24 [disp32] (lea rax, [rsp+disp32])
•	Relocs: none
(We can reuse ADDR_LOCAL from v2; this is just a typed alias.)
________________________________________
OPCODE: VAR_LOAD_TAG(lawet_id, base_reg) -> EAX
Loads tag as zero-extended u32 in EAX.
•	Inputs: base_reg (canonical: RAX)
•	Output: EAX = tag (0..255)
•	Clobbers: EAX
•	Flags: none
•	Bytes (base=RAX):
0F B6 80 [tag_off32]
For our model tag_off32=0, this becomes:
o	0F B6 00 (movzx eax, byte ptr [rax])
•	Relocs: none
•	Errors:
o	RANE_DIAG_INTERNAL_ERROR: missing lawet_id or base not an address value
________________________________________
OPCODE: VAR_TAG_EQ_IMM(lawet_id, imm_tag_u8) -> AL
Compares loaded tag with an immediate and produces a bool in AL.
Contract: tag is in EAX (from VAR_LOAD_TAG), then compare.
•	Inputs: EAX holds tag
•	Output: AL = 1 if tag == imm else 0
•	Clobbers: AL, flags
•	Flags: requires none; sets flags internally
•	Bytes:
o	83 F8 [imm8] (cmp eax, imm8)
o	0F 94 C0 (sete al)
•	Relocs: none
________________________________________
OPCODE: BR_ON_TAG(lawet_id, imm_tag_u8, L_match, L_nomatch)
Branch if tag equals immediate.
•	Inputs: EAX tag
•	Output: control flow
•	Clobbers: flags
•	Flags: none
•	Bytes:
o	83 F8 [imm8] (cmp eax, imm8)
o	0F 84 [rel32(L_match)] (je)
o	E9 [rel32(L_nomatch)] (jmp)
•	Relocs: Rel32_Jmp for both
________________________________________
3.2 Payload address/load/bind
OPCODE: VAR_ADDR_PAYLOAD(lawet_id, base_reg) -> RAX
Computes address of payload region (even if current tag is None).
•	Inputs: base_reg (canonical: RAX)
•	Output: RAX = base + PAY_OFF
•	Clobbers: RAX
•	Flags: none
•	Bytes (base already RAX):
48 8D 80 [pay_off32]
•	Relocs: none
For Maybe<i64>: pay_off32 = 8
________________________________________
OPCODE: VAR_LOAD_PAYLOAD_I64(lawet_id, base_reg) -> RAX
Loads payload as i64 into RAX.
•	Inputs: base address in RAX
•	Output: RAX = payload_i64
•	Clobbers: RAX
•	Flags: none
•	Bytes (base=RAX):
48 8B 80 [pay_off32]
•	Relocs: none
•	Errors:
o	RANE_DIAG_TYPE_MISMATCH: payload type is not i64 for that alt/lawet
________________________________________
OPCODE: VAR_STORE_PAYLOAD_I64(lawet_id, base_reg, src_reg64)
Stores payload i64.
Canonical constraint: keep base in RAX, store value from RDX.
•	Inputs: RAX = base, RDX = value
•	Output: memory updated
•	Clobbers: flags unchanged
•	Flags: none
•	Bytes:
o	48 89 90 [pay_off32] (mov [rax+pay_off], rdx)
•	Relocs: none
________________________________________
3.3 Construction ops
OPCODE: VAR_STORE_TAG_IMM(lawet_id, base_reg, imm_tag_u8)
Writes tag byte.
•	Inputs: RAX = base, imm8
•	Output: memory updated
•	Clobbers: none
•	Flags: none
•	Bytes:
o	C6 80 [tag_off32] [imm8] (mov byte ptr [rax+off], imm8)
•	Relocs: none
For our model: tag_off32 = 0
________________________________________
OPCODE: VAR_CONSTRUCT_NONE(lawet_id, dest_base_reg)
Constructs the “no-payload” alternative.
•	Inputs: RAX = dest_base
•	Output: variant initialized
•	Clobbers: none
•	Flags: none
•	Bytes:
o	C6 00 00 (mov byte ptr [rax], 0) ; tag=None
o	(optional payload scrub, policy-driven; see 3.5)
•	Relocs: none
________________________________________
OPCODE: VAR_CONSTRUCT_SOME_I64(lawet_id, dest_base_reg, src_reg64)
Constructs payload alternative (for Some i64).
•	Inputs: RAX = dest_base, RDX = payload_i64
•	Output: variant initialized
•	Clobbers: none
•	Flags: none
•	Bytes:
1.	tag:
	C6 00 01 (mov byte ptr [rax], 1) ; tag=Some
2.	payload:
	48 89 90 [pay_off32] (mov [rax+pay_off], rdx)
•	Relocs: none
________________________________________
3.4 Match lowering opcodes (structured → CFG)
Wer surface:
match m1:
  case Some x: print x
  case None:   print "none"
end
Canonical lowering template (mechanical)
Assume m1 is a local in slot(m1).
1.	VAR_ADDR_LOCAL Maybe<i64>, slot(m1) -> RAX
2.	VAR_LOAD_TAG Maybe<i64>, RAX -> EAX
3.	BR_ON_TAG Maybe<i64>, 1, L_some, L_next
4.	L_next: BR_ON_TAG Maybe<i64>, 0, L_none, L_default_or_trap
5.	In L_some:
o	bind x:
	VAR_LOAD_PAYLOAD_I64 Maybe<i64>, RAX -> RAX (but careful: RAX currently base; see note below)
6.	In L_none: …
7.	If exhaustive: L_default_or_trap should be unreachable → TRAP
Important base-address preservation rule
Because many ops use RAX as both address and value, the Typed CIL lowering must preserve base across payload loads. Two canonical ways:
Option A (recommended): keep base in RBX during match
•	VAR_ADDR_LOCAL -> RBX (or move RAX→RBX)
•	VAR_LOAD_TAG uses RBX
•	VAR_LOAD_PAYLOAD_I64 uses RBX (loads into RAX)
So we add one tiny helper opcode:
OPCODE: MOVE_ADDR_RAX_TO_RBX
•	Input: RAX
•	Output: RBX
•	Clobbers: RBX
•	Bytes: 48 89 C3 (mov rbx, rax)
Then match does:
•	addr in RAX
•	move to RBX
•	tag/payload read using RBX
•	payload ends up in RAX for printing/calls
I’m going to standardize match-on-variant base in RBX to avoid “address in RAX vs value in RAX” fights.
So update the variant address ops accordingly:
•	When matching: base address must be in RBX
•	When constructing: base address must be in RAX (fine)
We’ll reflect that in the match templates below.
________________________________________
4) Fully bytes-level match example (Maybe)
4.1 let m1 Maybe<i64> = Some 123
Lowering (contracted):
1.	compute address of m1 local:
•	lea rax, [rsp+slot_m1]
48 8D 84 24 [disp32]
2.	move payload into RDX:
•	mov rdx, 123
48 BA 7B 00 00 00 00 00 00 00
3.	construct Some:
•	mov byte ptr [rax+0], 1
C6 00 01
•	mov qword ptr [rax+8], rdx
48 89 90 08 00 00 00
4.2 let m2 Maybe<i64> = None
•	address:
48 8D 84 24 [disp32]
•	store tag 0:
C6 00 00
4.3 match m1: case Some x … case None … end
Assume slot(m1) known.
Tag dispatch:
1.	lea rax, [rsp+slot_m1]
48 8D 84 24 [disp32]
2.	preserve base in RBX:
48 89 C3
3.	movzx eax, byte ptr [rbx+0]
0F B6 03
4.	cmp eax, 1
83 F8 01
5.	je L_some
0F 84 [rel32]
6.	cmp eax, 0
83 F8 00
7.	je L_none
0F 84 [rel32]
8.	ud2 (exhaustive guarantee)
0F 0B
L_some: bind x and run body
•	bind x (payload load i64 into RAX):
o	mov rax, qword ptr [rbx+8]
48 8B 83 08 00 00 00
•	then print x becomes arg placement + CALL_IMPORT print (already covered in v2 call family)
L_none: body
•	print "none" via rodata string pointer + call import
________________________________________
5) Variant opcode → exact emission templates table (one row per opcode)
This is the “backend mechanically specifiable” portion.
Typed CIL Opcode	Required inputs (reg/stack)	Output	Clobbers	Flags	Emitted bytes	Relocs
VAR_LOAD_TAG(lawet, base=RBX)	RBX=base	EAX=tag	EAX	none	0F B6 03	none
VAR_STORE_TAG_IMM(lawet, base=RAX, imm)	RAX=base	mem	none	none	C6 80 off32 imm8 (off32=0 ⇒ C6 00 imm)	none
VAR_LOAD_PAYLOAD_I64(lawet, base=RBX)	RBX=base	RAX=payload	RAX	none	48 8B 83 pay_off32	none
VAR_STORE_PAYLOAD_I64(lawet, base=RAX, src=RDX)	RAX=base, RDX=val	mem	none	none	48 89 90 pay_off32	none
VAR_CONSTRUCT_NONE(lawet, base=RAX)	RAX=base	mem	none	none	C6 00 00	none
VAR_CONSTRUCT_SOME_I64(lawet, base=RAX, val=RDX)	RAX=base, RDX=val	mem	none	none	C6 00 01 ; 48 89 90 pay_off32	none
BR_ON_TAG(lawet, imm, Lm, Ln)	EAX=tag	CFG	flags	none	83 F8 imm8 ; 0F 84 rel32 ; E9 rel32	Rel32_Jmp
MOVE_ADDR_RAX_TO_RBX	RAX	RBX	RBX	none	48 89 C3	none
(If we prefer base always in RAX, we can—this RBX convention just prevents address/value collisions during payload binding.)
________________________________________
6) Diagnostics: canonical errors for variants + match
These map cleanly to wer rane_diag_code_t set (from wer earlier diag header style).
Parse / AST errors
•	RANE_DIAG_PARSE_ERROR
o	“invalid variant alternative syntax”
o	“case pattern does not match any variant alternative”
o	“duplicate case alternative: Some”
o	“default not allowed on exhaustive match” (if we forbid it)
Name/type errors
•	RANE_DIAG_UNDEFINED_NAME
o	unknown alt name in pattern: case Som x:
•	RANE_DIAG_TYPE_MISMATCH
o	payload binding type mismatch: case Some x where x declared incompatible
o	constructing alt with wrong payload type: Some "hi" for Maybe<i64>
o	accessing payload when alt has none: case None x: (invalid)
•	RANE_DIAG_REDECLARED_NAME
o	binding introduces a name that already exists in scope (unless we allow shadowing)
Policy/capability errors (usually none for pure variants)
•	Variants themselves don’t require capabilities, but if pattern body calls capability-gated ops, those checks still apply there.
Internal errors (compiler bug class)
•	RANE_DIAG_INTERNAL_ERROR
o	missing lawet_id for monomorphized variant
o	unknown pay_off32 at emission time
o	trying to emit VAR_LOAD_PAYLOAD_I64 on a lawet whose payload isn’t i64
Exhaustiveness (if we enforce it)
•	RANE_DIAG_TYPE_MISMATCH or a dedicated diagnostic category later:
o	“non-exhaustive match on Maybe: missing case None”
•	If we allow non-exhaustive matches, the lowering must include a default trap or default branch.
________________________________________
7) OSW optimization hooks (variant-specific)
These are the exact OSW expectations that become safe with this lawet/op set:
1.	Construct then immediate match fold
o	VAR_CONSTRUCT_NONE followed by VAR_LOAD_TAG/compare can fold to constant branch.
2.	Dead payload stores
o	If tag is None, payload region is semantically uninitialized; if we still zero it for determinism, OSW may remove the scrub if not observed.
3.	Tag compare chain to jump table
o	For variants with many alts, OSW can transform chained compares into a jump table. (That would introduce a new opcode later, but it’s an OSW optimization.)
4.	Case block dominance for binds
o	Payload load is only valid on the Some dominated region; OSW must keep that dominance relationship.
________________________________________
8) CIAM lowering rules (surface → Typed CIL) for the exact syntax we used
8.1 Construction
Surface:
•	let m Maybe<i64> = Some 123
Lower:
•	allocate local m (stack slot)
•	ADDR_LOCAL(slot(m)) -> RAX
•	emit RDX = 123
•	VAR_CONSTRUCT_SOME_I64(lawet=Maybe<i64>, base=RAX, val=RDX)
Surface:
•	let m Maybe<i64> = None
Lower:
•	ADDR_LOCAL -> RAX
•	VAR_CONSTRUCT_NONE(lawet, RAX)
8.2 Match
Surface:
match m:
  case Some x: S1
  case None:   S2
end
Lower:
•	ADDR_LOCAL(m) -> RAX
•	MOVE_ADDR_RAX_TO_RBX
•	VAR_LOAD_TAG(lawet, RBX) -> EAX
•	dispatch chain using BR_ON_TAG (tag 1 for Some, tag 0 for None)
•	In Some block:
o	VAR_LOAD_PAYLOAD_I64(lawet, RBX) -> RAX
o	bind x to RAX (or store to local slot for x)
•	In None block: no bind
________________________________________

here is a **single merged Appendix C** that:
1. **Incorporates the Variant opcode family** we just defined, and
2. Provides the **matching “Typed CIL opcode → exact emission template(s)” full table** for **all opcodes in the current backend feature set** (core + calls + mmio + variants) in one consistent row format.
I’m going to be extremely strict and mechanical so we can wire this directly into an emitter without interpretation.

---

# Appendix C — Typed CIL Opcode Catalog (Current Feature Set)
## C.0 Canonical backend conventions (so templates are deterministic)
### C.0.1 Target + ABI
* **Target:** Windows x86-64 (MSVC ABI)
* **Arg regs:** `RCX, RDX, R8, R9`
* **Return reg:** `RAX`
* **Stack:** 16-byte aligned at *call sites*, 32 bytes shadow space reserved by caller for any call.
### C.0.2 Register role conventions (Typed CIL → x64)
To keep emission templates fixed:
* **Primary value reg:** `RAX`
* **Secondary value reg:** `RDX`
* **Address/base reg (preferred):** `RBX` (especially for loads/binds)
* **Scratch regs (optional):** `R10`, `R11` (caller-saved; use if needed)
* **Bool results:** `AL` (0/1), or `EAX` if widened
### C.0.3 Relocation record types (canonical names)
* `Rel32_Call(target)` — 32-bit relative call displacement patch
* `Rel32_Jmp(target)` — 32-bit relative jmp displacement patch
* `Rel32_Jcc(target)` — 32-bit relative conditional branch displacement patch
* `Rel32_RipDisp(target)` — RIP-relative disp32 for `lea/mov/call/jmp [rip+disp32]`
### C.0.4 Section model assumptions
* `.text` code
* `.rdata` for string literals / constants
* `.idata` for imports (IAT slots)
### C.0.5 “Current feature set” definition for this Appendix
This table covers the **emitter-visible** opcode set needed to compile wer demonstrated constructs:
* locals, addressing, loads/stores
* integer + boolean ops
* comparisons + branching
* function calls (imports + internal)
* returns
* trap/halt
* mmio reads/writes (volatile)
* variant construction + match dispatch + payload binding
> Concurrency primitives (`spawn/join/send/recv/mutex_lock`) are **treated as normal calls** to runtime imports in this backend catalog (the emitter does not need special opcodes beyond CALL_IMPORT + arg setup). Same for `print`, `open`, `eval`, etc.

---

# Appendix C.1 — Typed CIL Opcode → Exact Emission Template(s)
**Column meanings**
* **Inputs:** required locations (regs/stack)
* **Outputs:** produced location(s)
* **Clobbers:** regs overwritten
* **Flags:** whether instruction relies on or modifies flags
* **Bytes:** exact hex template (placeholders shown as `[imm32]`, `[imm64]`, `[rel32]`, `[disp32]`)
* **Relocs:** relocation records generated (if any)
* **Canonical Errors:** what can go wrong at emit-time/typecheck-time

---

## C.1.1 Prolog / Epilog / Stack
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

 | 

---

---

-- | 

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 |
| `PROLOG(frame_size_aligned)` | `frame_size_aligned` imm32 | stack frame active | `RSP` | none | `48 81 EC [imm32]` *(sub rsp, imm32)* | none | `INTERNAL_ERROR` if frame not 16-aligned or too large for chosen form |
| `EPILOG(frame_size_aligned)` | `frame_size_aligned` imm32 | stack restored | `RSP` | none | `48 81 C4 [imm32]` *(add rsp, imm32)* | none | same |
| `RET` | none | control flow | none | none | `C3` | none | none |
> If we use `push rbp / mov rbp,rsp` we can add those opcodes too, but the minimal prolog above is enough for leaf-ish codegens.

---

## C.1.2 Addressing / Moves / Constants
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

 | 

---

---

-- | 

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

- |
| `ADDR_LOCAL(slot_disp32) -> RAX` | `slot_disp32` | `RAX=&local` | `RAX` | none | `48 8D 84 24 [disp32]` *(lea rax,[rsp+disp32])* | none | `INTERNAL_ERROR` bad slot |
| `MOVE_RAX_TO_RBX` | `RAX` | `RBX=RAX` | `RBX` | none | `48 89 C3` | none | none |
| `MOVE_RAX_TO_RDX` | `RAX` | `RDX=RAX` | `RDX` | none | `48 89 C2` | none | none |
| `MOV_RAX_IMM64(imm64)` | `imm64` | `RAX=imm64` | `RAX` | none | `48 B8 [imm64]` | none | none |
| `MOV_RDX_IMM64(imm64)` | `imm64` | `RDX=imm64` | `RDX` | none | `48 BA [imm64]` | none | none |
| `MOV_EAX_IMM32(imm32)` | `imm32` | `EAX=imm32` | `EAX` | none | `B8 [imm32]` | none | none |
| `MOVZX_EAX_MEM8(base=RBX, off32)` | `RBX=base` | `EAX=zeroext byte` | `EAX` | none | `0F B6 83 [off32]` | none | type mismatch if not u8 |
| `MOV_RAX_MEM64(base=RBX, off32)` | `RBX=base` | `RAX=qword` | `RAX` | none | `48 8B 83 [off32]` | none | type mismatch if not i64/u64 |
| `MOV_MEM64_RDX(base=RBX, off32)` | `RBX=base`, `RDX=val` | mem updated | none | none | `48 89 93 [off32]` | none | none |
| `MOV_MEM8_IMM(base=RBX, off32, imm8)` | `RBX=base` | mem updated | none | none | `C6 83 [off32] [imm8]` | none | none |
| `LEA_RCX_RIPREL(symbol)` | symbol | `RCX=&symbol` | `RCX` | none | `48 8D 0D [rel32]` *(lea rcx,[rip+rel32])* | `Rel32_RipDisp(symbol)` | undefined symbol |

---

## C.1.3 Integer Arithmetic (i64/u64)
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

 | 

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `ADD_RAX_RDX` | `RAX, RDX` | `RAX=RAX+RDX` | `RAX` | sets | `48 01 D0` *(add rax, rdx)* | none | type mismatch |
| `SUB_RAX_RDX` | `RAX, RDX` | `RAX=RAX-RDX` | `RAX` | sets | `48 29 D0` *(sub rax, rdx)* | none | type mismatch |
| `IMUL_RAX_RDX` | `RAX, RDX` | `RAX=RAX*RDX` | `RAX` | sets | `48 0F AF C2` *(imul rax, rdx)* | none | type mismatch |
| `NEG_RAX` | `RAX` | `RAX=-RAX` | `RAX` | sets | `48 F7 D8` | none | type mismatch |
| `INC_RAX` | `RAX` | `RAX++` | `RAX` | sets | `48 FF C0` | none | none |
| `DEC_RAX` | `RAX` | `RAX--` | `RAX` | sets | `48 FF C8` | none | none |
| `IDIV_RDXRAX_BY_RBX` | `RAX=num`, `RBX=den` | `RAX=quot`, `RDX=rem` | `RAX,RDX` | sets | `48 99` *(cqo)* ; `48 F7 FB` *(idiv rbx)* | none | division by 0 (runtime), type mismatch |
| `MOD_RDXRAX_BY_RBX` | same | `RDX=rem` (then move to RAX if desired) | `RAX,RDX` | sets | same as idiv | none | same |
> For `/` and `%` we typically lower: move numerator to RAX, denom to RBX, do `cqo; idiv rbx`. Quotient in RAX, remainder in RDX.

---

## C.1.4 Bitwise ops + Shifts (i64/u64)
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors | |
| 

---

---

---

---

---

---

- | 

---

---

---

---

---

-- | 

---

---

---

---

---

- | 

---

---

---

 | 

---

-- | 

---

---

---

---

---

-- | 

---

---

---

- | 

---

---

---

---

---

---

---

 | 

---

---

---

---

- |
| `AND_RAX_RDX` | `RAX,RDX` | `RAX&=RDX` | `RAX` | sets | `48 21 D0` | none | type mismatch | |
| `OR_RAX_RDX` | `RAX,RDX` | `RAX | =RDX` | `RAX` | sets | `48 09 D0` | none | type mismatch |
| `XOR_RAX_RDX` | `RAX,RDX` | `RAX^=RDX` | `RAX` | sets | `48 31 D0` | none | type mismatch | |
| `NOT_RAX` | `RAX` | `RAX=~RAX` | `RAX` | sets | `48 F7 D0` | none | none | |
| `SHL_RAX_CL` | `RAX`, `CL=shift` | `RAX<<=CL` | `RAX` | sets | `48 D3 E0` | none | shift count not in CL | |
| `SHR_RAX_CL` | `RAX`, `CL=shift` | logical shift | `RAX` | sets | `48 D3 E8` | none | same | |
| `SAR_RAX_CL` | `RAX`, `CL=shift` | arithmetic shift | `RAX` | sets | `48 D3 F8` | none | same | |
| `MOV_CL_IMM8(imm8)` | imm8 | `CL=imm8` | `RCX` low | none | `B1 [imm8]` | none | none | |

---

## C.1.5 Comparisons + Bool materialization
These are the mechanical building blocks for `if`, `while`, `match`, `goto cond -> L1 L2`, etc.
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

-- | 

---

---

---

 | 

---

---

---

---

---

-- | 

---

---

-- | 

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

 | 

---

---

 | 

---

---

---

---

---

- |
| `CMP_RAX_RDX` | `RAX,RDX` | flags set | flags | sets | `48 39 D0` *(cmp rax, rdx)* | none | type mismatch |
| `CMP_EAX_IMM8(imm8)` | `EAX` | flags set | flags | sets | `83 F8 [imm8]` | none | none |
| `SETE_AL` | flags | `AL=ZF?1:0` | `AL` | reads flags | `0F 94 C0` | none | none |
| `SETNE_AL` | flags | `AL=!ZF` | `AL` | reads flags | `0F 95 C0` | none | none |
| `SETL_AL` | flags | signed `<` | `AL` | reads flags | `0F 9C C0` | none | none |
| `SETLE_AL` | flags | signed `<=` | `AL` | reads flags | `0F 9E C0` | none | none |
| `SETG_AL` | flags | signed `>` | `AL` | reads flags | `0F 9F C0` | none | none |
| `SETGE_AL` | flags | signed `>=` | `AL` | reads flags | `0F 9D C0` | none | none |
| `MOVZX_EAX_AL` | `AL` | `EAX=zeroext(AL)` | `EAX` | none | `0F B6 C0` | none | none |

---

## C.1.6 Control flow: Labels + Branches + Trap/Halt
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

---

---

 | 

---

---

 | 

---

---

---

---

---

- | 

---

---

-- | 

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

-- | 

---

---

---

---

---

- |
| `LABEL(L)` | none | defines location | none | none | *(no bytes)* | none | duplicate label |
| `JMP(L)` | label | control flow | none | none | `E9 [rel32]` | `Rel32_Jmp(L)` | undefined label |
| `JE(L)` | flags | control flow | flags | reads | `0F 84 [rel32]` | `Rel32_Jcc(L)` | undefined label |
| `JNE(L)` | flags | control flow | flags | reads | `0F 85 [rel32]` | `Rel32_Jcc(L)` | undefined label |
| `JL(L)` | flags | signed < | flags | reads | `0F 8C [rel32]` | `Rel32_Jcc(L)` | undefined |
| `JLE(L)` | flags | signed <= | flags | reads | `0F 8E [rel32]` | `Rel32_Jcc(L)` | undefined |
| `JG(L)` | flags | signed > | flags | reads | `0F 8F [rel32]` | `Rel32_Jcc(L)` | undefined |
| `JGE(L)` | flags | signed >= | flags | reads | `0F 8D [rel32]` | `Rel32_Jcc(L)` | undefined |
| `UD2_TRAP` | none | abort | none | none | `0F 0B` | none | none |
| `INT3_TRAP` *(optional)* | none | breakpoint | none | none | `CC` | none | none |
| `HALT` | none | program end | none | none | `C3` *(or `F4` if ring0; don’t do that)* | none | none |
> In user code we have `trap` and `trap 7`. We can either:
> (a) always emit `ud2` and ignore code, or
> (b) move code into a reg and call an imported `rane_rt_trap(code)` then `ud2`.
> The backend catalog keeps it simple: `UD2_TRAP`.

---

## C.1.7 Calls (imports + internal)
### Import call via IAT slot (Windows PE)
Canonical form: `call qword ptr [rip+disp32]`
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

---

---

- | 

---

---

---

- | 

---

---

---

 | 

---

---

---

---

---

-- | 

---

-- | 

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

- |
| `CALL_IMPORT(iat_symbol)` | IAT symbol | `RAX=ret` | caller-saved regs | none | `FF 15 [rel32]` | `Rel32_RipDisp(iat_symbol)` | undefined import |
| `CALL_REL32(proc_label)` | label | `RAX=ret` | caller-saved regs | none | `E8 [rel32]` | `Rel32_Call(proc_label)` | undefined proc |
### Arg setup helpers (mechanical)
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes | Relocs | Errors |
| 

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

---

-- | 

---

---

---

---

---

---

---

 | 

---

---

-- | 

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

 | 

---

---

-- |
| `ARG0_RCX_FROM_RAX` | `RAX` | `RCX=RAX` | `RCX` | none | `48 89 C1` | none | none |
| `ARG1_RDX_FROM_RAX` | `RAX` | `RDX=RAX` | `RDX` | none | `48 89 C2` | none | none |
| `ARG2_R8_FROM_RAX` | `RAX` | `R8=RAX` | `R8` | none | `49 89 C0` | none | none |
| `ARG3_R9_FROM_RAX` | `RAX` | `R9=RAX` | `R9` | none | `49 89 C1` | none | none |
| `SHADOW_AND_ALIGN(callsite_bytes)` | frame metadata | ensures rsp alignment | `RSP` | none | *(handled by Frame Planner + PROLOG; no direct bytes here unless we use callsite fixups)* | none | internal |
> The normal approach is: frame planner guarantees `rsp` alignment globally; each call uses shadow space reserved in frame (or dynamically with `sub rsp, 32` around call). If we want explicit per-call: add `SUB_RSP_32` / `ADD_RSP_32` opcodes.
Optional explicit shadow around call:
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes | Relocs | Errors |
| 

---

---

---

---

 | 

---

---

 | 

---

---

---

 | 

---

---

-- | 

---

-- | 

---

---

---

---

- | 

---

---

 | 

---

---

 |
| `SUB_RSP_32` | none | rsp -= 32 | `RSP` | none | `48 83 EC 20` | none | none |
| `ADD_RSP_32` | none | rsp += 32 | `RSP` | none | `48 83 C4 20` | none | none |

---

## C.1.8 Loads/Stores for scalar locals (common patterns)
(We can do everything with `ADDR_LOCAL + MOV`, but these are common convenience opcodes.)
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes | Relocs | Errors |
| 

---

---

---

---

---

---

---

---

---

---

---

---

- | 

---

---

---

-- | 

---

---

---

-- | 

---

---

-- | 

---

-- | 

---

---

---

---

---

---

---

- | 

---

---

 | 

---

---

---

---

- |
| `LOAD_LOCAL_I64(slot_disp32) -> RAX` | slot | `RAX=value` | `RAX` | none | `48 8B 84 24 [disp32]` | none | type mismatch |
| `STORE_LOCAL_I64(slot_disp32) <- RAX` | slot, `RAX` | mem updated | none | none | `48 89 84 24 [disp32]` | none | type mismatch |
| `LOAD_LOCAL_I32(slot_disp32) -> EAX` | slot | `EAX=value` | `EAX` | none | `8B 84 24 [disp32]` | none | type mismatch |
| `STORE_LOCAL_I32(slot_disp32) <- EAX` | slot, `EAX` | mem updated | none | none | `89 84 24 [disp32]` | none | type mismatch |

---

## C.1.9 MMIO (volatile) read/write
Wer surface:
```rane
mmio region REG from 4096 size 256
read32 REG 0 into x
write32 REG 4 123
```
Backend lowers to: absolute address = base(4096) + offset.
For user-mode PE, *absolute* addresses are valid only if the OS maps something there; but as a language feature, we still define emission.
### Absolute addressing form (canonical)
* Read32: `mov eax, dword ptr [imm64]` is not encodable directly in x64.
So we use:
1. `mov rbx, imm64`
2. `mov eax, dword ptr [rbx]`
Write32:
1. `mov rbx, imm64`
2. `mov dword ptr [rbx], imm32` (or via reg)
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Errors |
| 

---

---

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

---

---

 | 

---

---

---

-- | 

---

---

---

 | 

---

-- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

 | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- |
| `MMIO_READ32_ABS(imm64_addr) -> EAX` | imm64 | `EAX=value` | `RBX,EAX` | none | `48 BB [imm64]` *(mov rbx, imm64)* ; `8B 03` *(mov eax,[rbx])* | none | security/policy violation if mmio disallowed |
| `MMIO_WRITE32_ABS_IMM(imm64_addr, imm32)` | imm64, imm32 | mem updated | `RBX` | none | `48 BB [imm64]` ; `C7 03 [imm32]` *(mov dword [rbx], imm32)* | none | same |
| `MMIO_WRITE32_ABS_EAX(imm64_addr)` | imm64, `EAX` | mem updated | `RBX` | none | `48 BB [imm64]` ; `89 03` *(mov [rbx], eax)* | none | same |
> **CIAM/Resolver policy** should require `capability syscalls` or a specific `capability mmio` if we add it. Right now, treat it as capability-gated at compile time.

---

## C.1.10 Variants (merged from our last step)
This is the exact variant opcode family, merged into the catalog.
### Lawet reminder for `Maybe<i64>`
* `tag_off32 = 0`
* `pay_off32 = 8`
* size 16 align 8
| Opcode | Inputs | Outputs | Clobbers | Flags | Bytes template(s) | Relocs | Canonical Errors |
| 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

 | 

---

---

---

---

---

 | 

---

---

-- | 

---

---

---

- | 

---

---

---

---

---

---

---

---

---

---

---

---

---

---

---

-- | 

---

---

---

---

---

---

---

---

 | 

---

---

---

---

---

---

---

---

---

---

-- |
| `VAR_ADDR_LOCAL(lawet, slot_disp32) -> RAX` | slot | `RAX=&variant` | `RAX` | none | `48 8D 84 24 [disp32]` | none | internal slot error |
| `VAR_LOAD_TAG(lawet, base=RBX) -> EAX` | `RBX=base` | `EAX=tag` | `EAX` | none | `0F B6 03` *(movzx eax, byte [rbx])* | none | internal lawet error |
| `VAR_STORE_TAG_IMM(lawet, base=RAX, imm8)` | `RAX=base` | tag written | none | none | `C6 00 [imm8]` *(for off=0)* | none | internal lawet error |
| `VAR_LOAD_PAYLOAD_I64(lawet, base=RBX) -> RAX` | `RBX=base` | `RAX=payload` | `RAX` | none | `48 8B 83 [pay_off32]` | none | type mismatch if payload not i64 |
| `VAR_STORE_PAYLOAD_I64(lawet, base=RAX, val=RDX)` | `RAX=base`, `RDX=val` | payload written | none | none | `48 89 90 [pay_off32]` | none | type mismatch |
| `VAR_CONSTRUCT_NONE(lawet, base=RAX)` | `RAX=base` | variant init | none | none | `C6 00 00` | none | none |
| `VAR_CONSTRUCT_SOME_I64(lawet, base=RAX, val=RDX)` | `RAX=base`, `RDX=val` | variant init | none | none | `C6 00 01` ; `48 89 90 [pay_off32]` | none | type mismatch |
| `BR_ON_TAG(lawet, imm8, L_match, L_nomatch)` | `EAX=tag` | CFG | flags | sets/reads | `83 F8 [imm8]` ; `0F 84 [rel32]` ; `E9 [rel32]` | `Rel32_Jcc`, `Rel32_Jmp` | undefined labels |
| `MOVE_ADDR_RAX_TO_RBX` | `RAX` | `RBX=RAX` | `RBX` | none | `48 89 C3` | none | none |

---

# Appendix C.2 — “Typed CIL opcode → exact emission template(s)” (Single full table)
The catalog above *is* already in the required “one row per opcode” consistent format.
But to match wer wording exactly (“generate the matching full table”), here’s the **consolidated list** in one uninterrupted sequence, grouped by subsystem, with no prose between entries:
## C.2.1 Consolidated emission table (all opcodes)
### Stack
* `PROLOG(frame_size_aligned)` → `48 81 EC [imm32]`
* `EPILOG(frame_size_aligned)` → `48 81 C4 [imm32]`
* `RET` → `C3`
* *(optional)* `SUB_RSP_32` → `48 83 EC 20`
* *(optional)* `ADD_RSP_32` → `48 83 C4 20`
### Addressing / moves / constants
* `ADDR_LOCAL(slot_disp32)->RAX` → `48 8D 84 24 [disp32]`
* `MOVE_RAX_TO_RBX` → `48 89 C3`
* `MOVE_RAX_TO_RDX` → `48 89 C2`
* `MOV_RAX_IMM64(imm64)` → `48 B8 [imm64]`
* `MOV_RDX_IMM64(imm64)` → `48 BA [imm64]`
* `MOV_EAX_IMM32(imm32)` → `B8 [imm32]`
* `MOVZX_EAX_MEM8(RBX,off32)` → `0F B6 83 [off32]`
* `MOV_RAX_MEM64(RBX,off32)` → `48 8B 83 [off32]`
* `MOV_MEM64_RDX(RBX,off32)` → `48 89 93 [off32]`
* `MOV_MEM8_IMM(RBX,off32,imm8)` → `C6 83 [off32] [imm8]`
* `LEA_RCX_RIPREL(symbol)` → `48 8D 0D [rel32]` + `Rel32_RipDisp(symbol)`
### Arithmetic (i64)
* `ADD_RAX_RDX` → `48 01 D0`
* `SUB_RAX_RDX` → `48 29 D0`
* `IMUL_RAX_RDX` → `48 0F AF C2`
* `NEG_RAX` → `48 F7 D8`
* `INC_RAX` → `48 FF C0`
* `DEC_RAX` → `48 FF C8`
* `IDIV_RDXRAX_BY_RBX` → `48 99` ; `48 F7 FB`
* `MOD_RDXRAX_BY_RBX` → same (use `RDX` result)
### Bitwise/shifts
* `AND_RAX_RDX` → `48 21 D0`
* `OR_RAX_RDX` → `48 09 D0`
* `XOR_RAX_RDX` → `48 31 D0`
* `NOT_RAX` → `48 F7 D0`
* `MOV_CL_IMM8(imm8)` → `B1 [imm8]`
* `SHL_RAX_CL` → `48 D3 E0`
* `SHR_RAX_CL` → `48 D3 E8`
* `SAR_RAX_CL` → `48 D3 F8`
### Compare / bool
* `CMP_RAX_RDX` → `48 39 D0`
* `CMP_EAX_IMM8(imm8)` → `83 F8 [imm8]`
* `SETE_AL` → `0F 94 C0`
* `SETNE_AL` → `0F 95 C0`
* `SETL_AL` → `0F 9C C0`
* `SETLE_AL` → `0F 9E C0`
* `SETG_AL` → `0F 9F C0`
* `SETGE_AL` → `0F 9D C0`
* `MOVZX_EAX_AL` → `0F B6 C0`
### Control flow
* `JMP(L)` → `E9 [rel32]` + `Rel32_Jmp(L)`
* `JE(L)` → `0F 84 [rel32]` + `Rel32_Jcc(L)`
* `JNE(L)` → `0F 85 [rel32]` + `Rel32_Jcc(L)`
* `JL(L)` → `0F 8C [rel32]` + `Rel32_Jcc(L)`
* `JLE(L)` → `0F 8E [rel32]` + `Rel32_Jcc(L)`
* `JG(L)` → `0F 8F [rel32]` + `Rel32_Jcc(L)`
* `JGE(L)` → `0F 8D [rel32]` + `Rel32_Jcc(L)`
* `UD2_TRAP` → `0F 0B`
* `HALT` → `C3`
### Calls + arg setup
* `ARG0_RCX_FROM_RAX` → `48 89 C1`
* `ARG1_RDX_FROM_RAX` → `48 89 C2`
* `ARG2_R8_FROM_RAX` → `49 89 C0`
* `ARG3_R9_FROM_RAX` → `49 89 C1`
* `CALL_IMPORT(iat_symbol)` → `FF 15 [rel32]` + `Rel32_RipDisp(iat_symbol)`
* `CALL_REL32(proc_label)` → `E8 [rel32]` + `Rel32_Call(proc_label)`
### Local scalar loads/stores (convenience)
* `LOAD_LOCAL_I64(slot)->RAX` → `48 8B 84 24 [disp32]`
* `STORE_LOCAL_I64(slot)<-RAX` → `48 89 84 24 [disp32]`
* `LOAD_LOCAL_I32(slot)->EAX` → `8B 84 24 [disp32]`
* `STORE_LOCAL_I32(slot)<-EAX` → `89 84 24 [disp32]`
### MMIO (absolute)
* `MMIO_READ32_ABS(imm64)->EAX` → `48 BB [imm64]` ; `8B 03`
* `MMIO_WRITE32_ABS_IMM(imm64,imm32)` → `48 BB [imm64]` ; `C7 03 [imm32]`
* `MMIO_WRITE32_ABS_EAX(imm64)` → `48 BB [imm64]` ; `89 03`
### Variants
* `VAR_ADDR_LOCAL(lawet,slot)->RAX` → `48 8D 84 24 [disp32]`
* `MOVE_ADDR_RAX_TO_RBX` → `48 89 C3`
* `VAR_LOAD_TAG(lawet,RBX)->EAX` → `0F B6 03`
* `VAR_STORE_TAG_IMM(lawet,RAX,imm8)` → `C6 00 [imm8]` *(tag_off=0)*
* `VAR_LOAD_PAYLOAD_I64(lawet,RBX)->RAX` → `48 8B 83 [pay_off32]`
* `VAR_STORE_PAYLOAD_I64(lawet,RAX,RDX)` → `48 89 90 [pay_off32]`
* `VAR_CONSTRUCT_NONE(lawet,RAX)` → `C6 00 00`
* `VAR_CONSTRUCT_SOME_I64(lawet,RAX,RDX)` → `C6 00 01` ; `48 89 90 [pay_off32]`
* `BR_ON_TAG(lawet,imm8,Lm,Ln)` → `83 F8 [imm8]` ; `0F 84 [rel32]` ; `E9 [rel32]` + relocs

---

✅ “merge these variant opcodes into Appendix C” — done (C.1.10)
✅ “generate the matching Typed CIL opcode → exact emission template(s) full table for all opcodes (core + calls + mmio + variants) in the same consistent row format” — done (C.1.* tables + C.2 consolidated list)

---
