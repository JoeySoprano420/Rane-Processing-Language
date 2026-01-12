This is my current plan for my custom language with a resolver (around 50,000 lines of code) instead of a 1million-line compiler.

RANE language, it's AOT Resolver (source → optimized CIAM (Contextual Inference Abstraction Macros) ‑processed expansion → machine code → executor).

Below is the full end-to-end picture of RANE (the language) and its AOT Resolver execution model (the “source → CIAM → machine code → executor” pipeline),


---

1. What RANE is



RANE is a deterministic, capability-aware, parallel systems language built for explicit, analyzable, low-level programming.
It is designed so you can get native performance while staying auditable, predictable, and safe-by-construction (not “safe by hope”). Its identity is anchored in five principles: Determinism, Explicitness, Safety, Transparency, Portability.

What “determinism” means in RANE

RANE aims for programs that behave the same way every time: no hidden allocations, no implicit conversions, no exceptions.
That theme repeats throughout the whole toolchain: compile stages, IR, lowering, and (when you enable the Resolver executor) the runtime contract as well.

Output targets

RANE compiles AOT to native x86-64 executables.


---

2. RANE’s surface language (what you write)



RANE’s “canonical definition” is the combination of:

RANE language, it's AOT Resolver (source → optimized CIAM (Contextual Inference Abstraction Macros) ‑processed expansion → machine code → executor).

a comprehensive syntax coverage suite (syntax.rane)

Top-level constructs

RANE’s core top-level forms include:

Proc (procedures)

Cap (capsules)

Dur (durations)

Qual (qualifiers)

Lit (literals)

Int (integers)

Asn (assignments)

Arg (arguments)

Dgn (delegations)

Ins (instances)

Init (initializers)

Pro (protocols)

Log (logs)

Gate ( logic gates)

Val (values)

Var (variables)

Ctrl (control flows)

Spec (specifiers)

Mod (modifiers/modifications)

Str (structures)

Rule (rules)

Seq (sequences)

Sel (selections)

Itr (iterations)

Data (data types)

Op (operators)

Namespace (namespaces)

Prim (primitives)

Arr (arrays)

List (lists)

Nes (nests)

I/o (input/output)

Eval (Evaluations)

Sch (schedulers)

Ste (states)

fn (functions)

struct (user-defined types)

enum (enumerations)

import (module imports)

capability (effect declarations)

Types (primitives + composites)




Primitive types include i8/i16/i32/i64, u8/u16/u32/u64, f32/f64, bool, void, plus int.
Composite types include:

pointers: *T

arrays: [N]T

structs

function types: fn(a: T, b: U) -> R

Statements + control flow

RANE supports variable declarations, assignment, blocks, if/else, loops (while, for i in a..b), and return.
Entry point is fn main() -> int { ... }.


---

3. The safety + capability model (the “why your resolver can say no”)



RANE’s safety model is explicitly layered:

Spatial safety

Bounds checks, pointer validity checks, and safe container operations.

Temporal safety

VM region expiration + container APIs structured to prevent use-after-free.

Capability safety (effects as permissions)

Functions declare required capabilities; call sites must satisfy them, and capability boundaries are enforced.
This is the bridge between “systems language power” and “auditable security”: you don’t infer effects from code review—you prove them from declarations + checks.

Deterministic concurrency

RANE’s concurrency model is designed to avoid data races in safe code, and make channels/atomics explicit.


---


RANE’s resolver architecture is a multi-stage AOT pipeline.

(source → optimized CIAM (Contextual Inference Abstraction Macros) ‑processed expansion → machine code → executor).

Compared to:

(Stage A — Lexing

rane_lexer.cpp: tokenizes identifiers, literals, operators, keywords.

Stage B — Parsing

rane_parser.cpp: builds the AST, enforces grammar rules.

Stage C — Typechecking

Ensures type correctness, correct returns, pointer usage, and capability requirements.

Stage D — Capability checking

Functions declare required capabilities; call sites must satisfy them.

Stage E — TIR (Typed Intermediate Representation)

A typed, SSA-friendly IR used for optimization and backend lowering.

Stage F — SSA construction

Dominance analysis, etc.

Stage G — Register allocation

Linear-scan allocator targeting x64.

Stage H — Backends

Native x86-64 backend (rane_x64.cpp): emits machine code, constructs PE64, patches literals, manages imports (e.g., printf).

Stage I — Driver + CLI

rane_driver.cpp orchestrates compilation/output; main.cpp parses CLI args and dispatches.)

Binary output model (PE64)

The PE writer constructs DOS header, NT header, import tables, code/data sections, literal patching, and alignment rules.


---

5. The runtime architecture (what exists around resolved code)



RANE’s runtime isn’t “big VM energy.” It’s focused utilities that enable safety + determinism where you want it:

VM container system

rane_vm.cpp provides safe memory regions, bounds checked reads/writes, slices/subspans, deterministic lifetimes, and temporal safety—foundation for sandboxing/plugin safety.

Concurrency runtime

Threads, mutexes, channels, semaphores, barriers, atomics, once-init—deterministic unless explicitly relaxed.

Optional GC

(only if zero-cost) and off by default—reserved for higher-level/host scripting scenarios.


---

6. The AOT Resolver (the “source → CIAM → machine code → executor” super-pipeline)



Now the main event: RANE’s Resolver model—my “compile + run” device that goes beyond a traditional compiler.

The Resolver pipeline is explicitly defined as:

source → optimized CIAM-processed expansion → machine code → executor

The doc positions the Resolver as a new category: context-aware compilation with deterministic execution.
It isn’t “a compiler,” “an interpreter,” or “a VM”—it’s a hybrid device that expands meaning at compile time and enforces ritual-level determinism at runtime.


---

6.1 Stage 1: CIAM-Processed Expansion (Source → CIAM-Expanded IR)

What CIAMs are (and what they are not)

CIAMs (Contextual Inference Abstraction Macros) are not textual macros. They’re semantic transforms that are:

context-aware

invariant-preserving

capability-checked

deterministic

optimization-friendly

They exist to take what a human means (“intent”) and expand it into explicit, machine-lowerable operations without losing safety and determinism.

Inputs to CIAM expansion

Source program

Context metadata (roles, capabilities, invariants)

Domain rules

Optional prior traces

Responsibilities

CIAM expansion must:

parse source into an IntentGraph

apply CIAMs to expand intent into deterministic actions

validate invariants

enforce capability constraints

produce a CIAM-expanded IR

Output

An expanded IR that is context-aware, deterministic, semantically enriched.
The doc explicitly frames this IR as richer than LLVM IR in semantic meaning, but still simpler than an entire “ActionPlan.”

Mental model:

AST answers: “what did they write?”

TIR answers: “what does it mean in typed ops?”

CIAM Expanded IR answers: “what must actually happen, given the context, rules, and allowed powers?”


---

6.2 Stage 2: Optimization (CIAM-Expanded IR → Optimized CIAM IR)

After expansion, you optimize—but not like a compiler. The doc explicitly includes “invariant-aware optimization” and “capability-safe lowering,” meaning optimization itself must preserve the semantic and security contracts.

Optimization passes include:

constant folding

algebraic simplification

dead-code elimination

control-flow simplification

invariant-aware optimization

capability-safe lowering

peephole optimizations

Output is an optimized CIAM IR ready for machine code generation.


---

6.3 Stage 3: Machine Code Generation (Optimized CIAM IR → native code + metadata)

The optimized CIAM IR is lowered deterministically and context-preservingly to a target ISA (x86-64 / ARM64 / RISC-V / etc.).

Resolver codegen guarantees

The machine code generation stage is framed with strong guarantees:

no undefined behavior

no type punning

no untracked side effects

no nondeterministic scheduling

And crucially: output is native machine code with embedded metadata for the executor.
This “metadata” is what makes the executor supervision possible without turning the system into a VM.


---

6.4 Stage 4: Executor (native code → supervised deterministic execution)

The executor is explicitly described as:

not a VM

a deterministic ritual supervisor for native code

Executor responsibilities

It must:

enforce invariants (only if zero-cost)

enforce capability boundaries 

provide deterministic I/O

provide deterministic scheduling

generate execution traces (only if zero-cost)

allow replay and audit (only if zero-cost)

Runtime overhead target

Minimal overhead: “lightweight guards + trace hooks + deterministic scheduling.”
The system must not drop below 98% of native performance for “RANE CIAM expansion → machine code → executor.”


---

7. Putting it together as an implementable “AOT Resolver” architecture



Here’s the complete Resolver build as a concrete pipeline with the real “things” you implement.

7.1 Resolver front end (source → IntentGraph)

Goal: produce an intent representation that can accept context.

Inputs

tokens (lexer)

AST (parser)

type info + capability info (typecheck)

context metadata and domain rules

Output

IntentGraph (nodes = intents/actions; edges = flow + dependency + capability gates)

Why this exists Because CIAM expansion is defined as “intent → deterministic actions,” not “text → text.”

7.2 CIAM engine (IntentGraph → CIAM-Expanded IR)

Goal: expand “what the developer meant” into explicit operations that preserve invariants and capability rules.

What a CIAM must be able to see

roles, capabilities, invariants, environmental metadata, domain constraints

What a CIAM must guarantee

deterministic expansion

invariant preservation

capability enforcement

Output

CIAM-Expanded IR: deterministic, context-aware, capability-checked, invariant-annotated, optimization-friendly, machine-lowerable

7.3 Optimizer (CIAM-Expanded IR → Optimized CIAM IR)

Goal: optimize without violating meaning/contracts.

Must include invariant-aware and capability-safe transformations, not just classic peepholes.

7.4 Codegen (Optimized CIAM IR → ISA code + executor metadata)

Goal: emit native machine code + embed metadata required for enforcement/tracing.

Must preserve: determinism, tracked side-effects, and prevent nondeterministic scheduling in emitted code.

7.5 Executor (native code + metadata → deterministic supervised run)

Goal: provide a “ritual contract” at runtime:

deterministic scheduling + deterministic I/O

capability boundaries enforced

invariants monitored

traces produced for replay/audit


---

8. How the Resolver relates to “regular RANE compilation”

Mode: Resolver mode (AOT Resolver)

RANE → IntentGraph → CIAM expansion → optimized CIAM IR → native code (+ metadata) → executor.

Resolver mode is where “context becomes a compile-time semantic object” and determinism becomes an explicit runtime contract.


---

9. Practical “what this enables” (why this pipeline matters)



The doc targets domains where you need:

deterministic systems programming

capability-safe OS-like components

audit-ready finance

safety-critical robotics

formalizable cyber-physical systems

sandbox/plugin safety

onboarding-friendly runtimes

And it explicitly frames the Resolver as: compile-time contextual intelligence + native machine code performance + runtime deterministic enforcement.


---

10. Current strengths)



RANE is already strong in deterministic systems programming, explicit memory control, safe parallelism, real-time DSP, sandboxed execution, portable AOT compilation, and low-level binary emission.

I am willing to trade some determinism to avoid dropping under the 98% performance floor.

And

the CIAM domain rules I prefer to be written in a sugary abstraction-heavy Language within RANE
