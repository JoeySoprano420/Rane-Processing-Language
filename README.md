# RANE — Reference Picture (Complete README)

Reliable · Adaptive · Natural · Efficient

This README is the canonical, exhaustive reference picture for RANE — the deterministic execution grammar and pipeline that reduces human-shaped intent into ABI-correct Windows x64 PE binaries. Read it end-to-end to understand the system, the guarantees, all major components, the developer workflow, and the places to change or extend behavior.

Table of contents
- What RANE Is (high-level)
- One-sentence spine
- Core principles and rules
- The pipeline (complete)
- CIAMs: definition, properties, examples
- IRs and major artifacts (what lives where)
- Key implementation notes (recent changes you must know)
  - `TypedCilResolver` sizing & derives
  - `FramePlanner` per-variable placement notes
  - `NativeEmitter` context propagation
  - Tests added
- ABI / Struct layout model and derives
- Emission semantics: heap vs stack, annotations, determinism
- Developer workflow (build, test, emit, debug)
- Extending RANE: CIAMs, struct derives, emitter helpers
- Troubleshooting & deterministic build tips
- Quick reference: commands & file locations
- Contribution notes

---

What RANE Is (high-level)
RANE is not a language in the usual sense. It is a deterministic execution grammar and pipeline that treats surface text as mathematical structure — an auditable proof-of-execution. The system maps human intent to physical machine reality (Windows x64 PE) through deterministic, closed transformations. If it can't be expressed as machine code, it is not part of RANE.

One-sentence spine
RANE is a capability-gated execution system where human-shaped instruction prose is deterministically reduced—via CIAM-governed structural physics—into verifiable Windows x64 machine code.

Core principles and rules
- No smoke. No mirrors. Every lowering is auditable.
- Human-readable constructs have a single canonical structural representation.
- Memory and effects are explicit and deterministic.
- Capability imports are the only source of runtime cost.
- AOT output is deterministic: same input + same config = same bytes.
- Intrinsics are language primitives (e.g. `addr`, `load`, `store`, `mmio`, `trap`, `halt`, `asm`).
- If the system emits something, there must exist a structural proof (CIAM trace and annotations).

---

The Pipeline (the only one)
All transformations are closed (no bypass). The canonical pipeline:

- `syntax.rane` (surface)
  ↓ CIAM intent recognition → Lexer/Tokenizer
  ↓ CIAM contextual shaping → Parser
  ↓ CIAM grammatical normalization → Structural Tree (AST / SMD)
  ↓ CIAM semantic locking → Resolver (names, capabilities, ownership)
  ↓ CIAM semantic materialization → `TypedCilModule` (Typed CIL)
  ↓ CIAM structural optimization → OSW (Optimized Structure Web)
  ↓ CIAM ABI truth → FramePlanner (frames, prologue/epilogue templates)
  ↓ CIAM emission law → Native codegen / `NativeEmitter` → Windows x64 PE

Key invariant: nothing escapes CIAM governance.

---

CIAMs — Contextual Inference Abstraction Macros
What they are:
- CIAMs are auditable structural rewrite laws.
- They match shape + context, produce deterministic rewrites, record invariants and audit stamps.
- They operate at every pipeline stage.

Definition components:
- Pattern: the shape to match
- Context: required facts / annotations
- Rewrite: the new structure
- Invariants: constraints preserved
- Audit: machine-readable record explaining the rewrite

Examples of deterministic rewrites:
- `with` → `try/finally` lowering
- `defer` → guaranteed cleanup nodes
- `choose` → branchless selection or intrinsic call
- Async → explicit state machine nodes
- Ownership annotations → enforced at resolver stages

Everything is recorded; nothing is hidden.

---

IRs and major artifacts (where to look)
- `TypedCilModule` — typed, resolved IR used by the emitter (`src/CIAM/TypedCil.cs`).
- Per-proc annotations (dictionary) — used to carry resolver / planner hints:
  - `local.<name>` : `stack:<N>` | `heap`
  - `frame.allocs.heap` : `true` | absent
  - `opt.hints` : comma-separated optimization hints
  - `vectorize_loop_count` : integer
  - `resolved.diagnostics` : string
- Frame artifact: `{module}.frames.json` emitted by `FramePlanner.BuildAndWrite(...)`.
- Emitted C: `NativeEmitter.EmitCSource(...)` (string) — deterministic readable C for inspection.
- Native build: `NativeEmitter.EmitNativeExe(...)` — best-effort clang pipeline targeting `x86_64-w64-mingw32`.

Relevant code locations:
- Resolver: `src/CIAM/TypedCilResolver.cs`
- Frame planner: `src/CIAM/FramePlanner.cs`
- Emitter: `src/Backend/NativeEmitter.cs`
- Semantic materialization orchestration: `src/CIAM/SemanticMaterialization.cs`
- Tests: `tests/*` (see `FramePlannerEmitterTests.cs`, `StructLayoutTests.cs`)

---

Key implementation notes — what we recently changed
These are the precise, auditable changes you must know (and where they affect reasoning):

1. `TypedCilResolver` now computes per-let sizes with ABI-aware layout
   - New function: `GetTypeSizeAndAlign(string typeName, TypedCilModule module)` performs a field-by-field struct layout, returns `(size, align)`.
   - Supports primitive widths and single-dimensional array sizing (`T[N]`).
   - Recognizes struct definitions in the module and computes nested sizes (with padding).
   - Exposes per-struct derive parsing: `align=NN`, `pack=NN`, `abi=windows|linux`.
   - Resolver emits `local.<name> = "stack:<N>"` when size known and small, or `"heap"` when large/unsafe.

2. `FramePlanner` records per-variable local placement
   - Honors `local.<name>` if provided by resolver.
   - Otherwise uses conservative heuristics to mark `local.<name>` and `frame.allocs`.
   - Emits `{module}.frames.json` containing deterministic per-proc frame decisions including `local.*` notes.

3. `NativeEmitter` now uses `currentProc` context for lowering
   - `EmitStmt` and auxiliary lowering functions accept an optional `TypedCilProc? currentProc` parameter.
   - `EmitPatternMatchLowering`, `EmitVariantCType` accept and propagate `currentProc`.
   - This avoids module-wide scans to locate the owning proc for a `let`, improving determinism and performance.
   - When `local.<name> = stack:<N>` present, emitter emits exact stack arrays: `char name[N]; /* planner stack placement */`.
   - When `local.<name> = heap` or `frame.allocs.heap=true`, emitter emits runtime allocation: `void* name = rane_rt_alloc(size);`.

4. Tests added
   - `tests/FramePlannerEmitterTests.cs` — verifies `rane_rt_alloc` usage when `frame.allocs.heap` present and `char buf[]` is *not* emitted.
   - `tests/StructLayoutTests.cs` — validates nested struct sizes, array sizes, derive-driven alignment padding.

Why this matters
- The system now provides provable local placement decisions, auditable outputs (`.frames.json`), and deterministic emitter behavior that exactly follows resolver/planner annotations.
- Developers can force placements in tests or adjust `Derive` strings on structs to tune ABI layout.

---

ABI / Struct layout model and derives
- Default ABI:
  - Pointer = 8 bytes, default alignment = 8.
  - Primitive sizes: `u8=1`, `i16=2`, `i32=4`, `i64=8`, `f32=4`, `f64=8`.
- Struct layout algorithm (implemented):
  - Walk fields in declared order.
  - For each field, compute its `(size, align)` (recursively).
  - Optionally apply `pack` (upper bound on alignment) and `align` override (min alignment).
  - Align field offset to field alignment, accumulate size.
  - Final struct size is padded to the computed struct alignment.
- `Derive` string on `TypedCilStruct` supports:
  - `align=NN` — increase struct alignment floor to `NN`.
  - `pack=NN` — maximum field alignment clamp (like `#pragma pack`).
  - `abi=windows|linux` — (reserved) hook for target-specific rules (currently parsed; future tweaks can alter struct packing policy).
- Use-case examples:
  - `TypedCilStruct("Foo", fields, Derive: "align=16")` → pads to 16.
  - `Derive: "pack=4"` → forces field alignments ≤ 4.

---

Emission semantics: stack vs heap decisions (complete)
- Resolver attempts to determine exact sizes:
  - String literals → length+1
  - `T[N]` → element size × N
  - Named struct → full struct layout via `GetTypeSizeAndAlign`
- Resolver emits `local.<name>=stack:N` if `N <= 128` (configurable threshold in heuristics), else `heap`.
- FramePlanner consumes those and writes `notes` (audit keys) including `local.<name>` and `frame.allocs`.
- `NativeEmitter` consults `currentProc.Annotations` first for `local.<name>`:
  - If `stack:N` present and function-level constraints allow, emit `char name[N];`.
  - If `heap` or function marked `frame.allocs.heap=true`, emitter emits `rane_rt_alloc` (size inferred or conservative default).
- Determinism:
  - All decisions are deterministic given the same `TypedCilModule` and environment (`SOURCE_DATE_EPOCH` for timestamps).
  - `FramePlanner` writes `frames.json` to make decisions auditable.

---

Developer workflow (complete)
1. Prepare environment
   - Install .NET SDK 8.x and Visual Studio 2026 workloads.
   - If you plan to produce native PE, install `clang`, `lld`, `llvm-objcopy`, and ensure cross-targeting for `x86_64-w64-mingw32`.

2. Restore & build
   - `dotnet restore`
   - `dotnet build -c Release`

3. Run tests
   - `dotnet test -c Release`
   - Per-test filter: `dotnet test --filter FullyQualifiedName~Namespace.ClassName.MethodName`

4. Inspect emitted C (no native toolchain needed)
   - Materialize a module (example via existing tests or orchestration) — ensure `TypedCilResolver.AnalyzeAndAnnotate(module)` and `EmissionLaw.PrepareModuleForEmission(...)` run (the materialization stage now does this automatically).
   - `var c = NativeEmitter.EmitCSource(preparedModule);`
   - Inspect the returned string; search for `rane_rt_alloc` or `char <name>[N];` to verify placement.

5. Produce a native PE (best-effort)
   - `NativeEmitter.EmitNativeExe(module, typedCilPbPath, outputExePath, workDir, clangPath, objcopyPath, preferNative:true, skipToolchain:false, enableLto:true, enablePgo:false)`
   - If toolchain not available, a placeholder file embedding the generated C will be written.

6. Audit frame decisions
   - Look for `{module}.frames.json` created by `FramePlanner.BuildAndWrite(...)`.
   - This contains `FrameFunction.Notes` with `local.<name>`, `locals.bytes`, `spills.count`, and `frame.allocs`.

---

Extending RANE: CIAMs, struct derives, emitter helpers
- Adding a CIAM
  - Add rule under `pipeline/` CIAM description files.
  - Implement corresponding rewrite in `src/CIAM/*` (follow pattern: match → rewrite → annotate → audit).
  - Add unit tests that assert the structural rewrite and audit record exist.

- Adding struct derive behavior
  - Add parsing in `TypedCilResolver.ParseStructDerive` (already present).
  - Implement target-specific ABI tweaks in `GetTypeSizeAndAlign` if `abi=windows` or `abi=linux` required.
  - Add tests in `tests/StructLayoutTests.cs`.

- Emission changes
  - Keep `EmitStmt`, `EmitPatternMatchLowering`, and `EmitVariantCType` accepting `TypedCilProc? currentProc`.
  - Propagate `currentProc` into any new lowering helpers to ensure per-proc annotations are consulted deterministically.

---

Troubleshooting & deterministic build tips
- If stack/heap placements differ between runs:
  - Ensure `SOURCE_DATE_EPOCH` set if timestamps influence determinism in your pipeline.
  - Re-run resolver and examine `proc.Annotations` for `local.<name>` entries.
  - Compare `{module}.frames.json` across builds.

- If native build fails:
  - Confirm `clang --version` and `llvm-objcopy --version` are on `PATH`.
  - Ensure cross-targeting libraries for `x86_64-w64-mingw32` are installed when building on Linux/macOS.
  - Use `skipToolchain=true` to obtain embeddable C and debug emitter output.

- Debugging pattern-match lowering:
  - `NativeEmitter.EmitPatternMatchLowering(...)` throws when patterns mismatch variant schemas. Add tests covering variant payload arities.

---

Quick reference — commands & file locations
- Build & test
  - `dotnet restore`
  - `dotnet build -c Release`
  - `dotnet test -c Release`

- Important files
  - `src/CIAM/TypedCilResolver.cs` — resolver, layout pass, derive parsing
  - `src/CIAM/FramePlanner.cs` — frame decisions and `*.frames.json` emission
  - `src/Backend/NativeEmitter.cs` — C emitter and native build pipeline
  - `src/CIAM/SemanticMaterialization.cs` — wires resolver + emission law in materialization
  - `tests/FramePlannerEmitterTests.cs`, `tests/StructLayoutTests.cs` — new tests demonstrating placement/layout

- Per-proc annotations you can set in tests:
  - `proc.Annotations["local.<name>"] = "stack:256"`
  - `proc.Annotations["local.<name>"] = "heap"`
  - `proc.Annotations["frame.allocs.heap"] = "true"`

---

Contribution notes
- Keep CI deterministic: pin dotnet SDK and clang versions in CI images when doing native builds.
- Add unit tests for any new CIAM, derive semantics, or emitter lowering paths.
- Every structural rewrite must produce an audit trace — add tests asserting audit content.
- Prefer small, auditable commits describing the invariant enforced.

---

Final words — the reference picture
RANE is the equilibrium of intent, structure, and machine reality. This repository encodes that equilibrium: deterministic CIAMs, ABI-aware layout, auditable frame and placement decisions, and an emitter that carries proc context through every lowering path. The system is precise, explainable, and provably tied to produced bytes.

If you want, I can:
- Generate a concise `CONTRIBUTING.md` from this README.
- Add example end-to-end scripts that run materialization → emit C → optional native build with reproducible flags.
- Add a small visualization tool that compares `frames.json` across runs.
