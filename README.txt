# 🌐 THE RANE LANGUAGE — COMPLETE SYSTEM OVERVIEW

**Reliable Adaptive Natural Efficient (RANE Processing Language)**
**P.I.E. = Processing Ideas → Instructions → Execution**

RANE is designed as a *systems whisperer*: you write clear, human-shaped instructional prose, and the toolchain deterministically turns it into machine-operable structure and then into real Windows x86-64 PE executable code.

RANE’s identity is not “a syntax that compiles.” It’s an **execution civilization**: language + staged transformations + capability security + deterministic concurrency + inspectable IR + an emitter that can produce `.exe` output.

---

## 0) The spine: what RANE *is* in one sentence

**RANE is a deterministic, capability-gated systems language whose compilation pipeline is a sequence of audited structural transformations (CIAMs) from human-friendly instruction prose into a Typed IR web that lowers into Windows x64 machine code.**

---

## 1) P.I.E. — the mission statement (Processing Ideas → Instructions → Execution)

### 1.1 “Processing ideas”

This is the phase where you’re still thinking like a human: *What do I want done?*
RANE supports this by letting your code read like:

* “with open path as f”
* “defer close f”
* “start at node start”
* “go to node end_node”
* “choose max a b”
* “requires network_io”

RANE is intentionally shaped so your **intent** remains visible even when the compiler becomes aggressive.

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

### 2.1 Pipeline diagram (conceptual truth)

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
* debug/unwind metadata hooks (if you support it)

Output: frame map + per-proc layout + calling convention compliance plan.

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
* You don’t write “compiler-bait”; you write “instruction truth.”

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
* You get predictable performance while keeping readability.

### 4.7 “Dynamic AOT compilation”

Meaning:

* “Ahead-of-time” output is real native code.
* “Dynamic” means the pipeline can:

  * choose different lowering strategies based on context
  * apply CIAMs and optimizations based on target and code patterns
* But determinism is preserved: the same inputs + same configuration → same output.

### 4.8 “Completely runtime-free by default”

Meaning:

* If you don’t import runtime services, you don’t pay for them.
* You can compile a “freestanding-ish” subset:

  * no heap
  * no IO
  * no threads
* When you *do* want services, you explicitly import and require capabilities.

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

## 5) Core language surface (what your syntax demonstrates)

I’m going to treat your provided syntax as **canonical supported forms** and explain each category:

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
* `protected`: callable within module + friend scopes (depending on your rules)
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

You list:

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

* `typealias word = u32`: introduces a named alias that preserves type identity rules you define (could be “strong alias”)
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
  * or runtime-vtable-like hooks if you ever choose that (but your “runtime-free by default” suggests avoid vtables)

### 10.2 `struct Person: name string age u8 end`

* field list
* deterministic layout (C-like)
* no hidden padding decisions: layout is defined and inspectable

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

Your examples:

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

You define:

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

This is your “security is compile time centric” line made real:

* You can’t accidentally do IO.
* You can’t “accidentally” spawn threads.
* You can’t hide `eval` inside some helper; it contaminates the call chain unless explicitly permitted.

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
  * optionally runtime (if you choose), but your model leans compile-time-first

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
* type constraints can be added later (if you want)
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

  * scheduling must be explicit or stable per your `pragma "scheduling" "fair"`

### 15.2 `dedicate proc` + `spawn/join`

Your intent:

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

**Compile-time safety options (depending on how strict you want):**

* forbid `free p` while `q` is live
* or require `q` be dropped/ended before free
* or allow but mark as unsafe (you didn’t include `unsafe`, so best is to enforce structurally)

---

## 17) Control flow: if/else, loops, match/switch/decide, goto/label

### 17.1 Deterministic evaluation

* Expressions evaluate left-to-right.
* Conditions are explicit booleans.
* No “truthy” ambiguity unless you define it (you show `if (f & Flags.Write):`, so you likely define nonzero → true only for `bool` or provide an explicit rule for flags).

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

You keep distinct keywords because they can carry different semantic intent:

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
* requires capability `syscalls` or `unsafe_asm` (you used `syscalls`)

---

## 19) Exceptions: `try/catch/finally`, `throw`

RANE includes:

* recoverable flow (`throw`, `catch`)
* deterministic cleanup (`finally`)
* structural lowering into explicit control flow (not mystical stack unwinding unless you implement real SEH)

**Possible lowering strategies:**

1. **Zero-cost SEH-style** (harder)
2. **Explicit error-return + handler blocks** (simpler, deterministic, auditable)
3. **Trap-based** for “no exceptions mode”

Given your “runtime-free by default,” the simplest consistent approach is:

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

This is exactly how you keep “professional language” + “security compile-time centric.”

---

## 21) Operators: your full operator set, with rules

You demonstrate:

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

* into immediate IDs (integers) or pointers (if you build a symbol table blob)

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

This gives you:

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

If you skip a stable typed IR, you end up with:

* scattered lowering logic
* duplicated rules
* “optimization spaghetti”
* fragile codegen

Typed CIL is the “single truth” the rest of the compiler can trust.

---

## 25) OSW — Optimized Structure Web (why it’s a “web” not a “list”)

### 25.1 Why “web”

Because optimization is not one-dimensional. You have:

* prerequisites (type facts, alias facts, capability facts)
* mutual exclusions (can’t inline after some transforms if you want debug fidelity)
* multiple routes to the same end (jump table vs if-chain)
* target-driven decisions (size vs speed)

So OSW is:

* transformations as nodes
* dependencies as edges
* CIAMs can inject edges (“prefer pattern X if pragma says hot”)

### 25.2 What OSW *does* to your constructs

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

Output is a **map** from IR locals/temps → `[rsp+offset]` (or `[rbp-offset]` if you use frame pointers).

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

You can choose:

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

Your philosophy implies:

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
* show frame layout (why alignment changed)

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

## 31) Putting it all together: what your provided program demonstrates

Your program is effectively a **total coverage file**:

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

You write:

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

