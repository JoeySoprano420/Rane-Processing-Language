# Contributing

Thanks for helping improve **Rane Processing Language**.

This repository contains a C/C++ implementation of the RANE language toolchain (lexer/parser/AST, typecheck, IR/TIR, runtime, and tests). The goal of these guidelines is to keep changes easy to review and keep the project building in Visual Studio.

## Build & Run (Visual Studio)

### Prerequisites
- Visual Studio 2026 with **Desktop development with C++**
- Windows SDK (installed with the workload)

### Build
1. Open the solution in Visual Studio.
2. Select configuration **Debug** or **Release**.
3. Build using __Build Solution__.

### Run
- Set the startup project (if applicable) and run with __Start Without Debugging__.

## Tests

Tests live in `tests/` (for `.rane` programs) and in C++ test sources (e.g. `rane_gc_tests.cpp`).

Typical workflows:
- Build the solution and run the test executable(s) from Visual Studio.
- If a test is a `.rane` file, ensure the driver path that executes `.rane` sources is covered by the build/run configuration.

When adding features:
- Add at least one focused test under `tests/`.
- Prefer small, single-purpose `.rane` fixtures that demonstrate the new syntax/behavior.
- Large feature work must be split into small, reviewable vertical slices (parser→lowering→codegen) with tests per slice.

## Coding Standards

### General
- Keep changes minimal and focused; avoid unrelated refactors in the same PR.
- Prefer clear, straightforward code over cleverness.
- Avoid introducing new dependencies unless necessary.

### C/C++ Style
- Match the existing style in the surrounding files.
- Use `nullptr` in C++ where applicable.
- Keep functions `static` when they are translation-unit local.
- Prefer explicit initialization (`memset`/`calloc`/value-init) as used in the codebase.
- Use the project’s existing error/diagnostic types (`rane_error_t`, `rane_diag_t`, `rane_diag_code_t`).

### Diagnostics & Errors
- When introducing new parse/typecheck errors, prefer:
  - Setting a specific `rane_diag_code_t` (or extending it in a follow-up if needed)
  - Accurate span information (line/col/length)
  - A short, actionable message

### Memory Management
- This codebase frequently uses `malloc`/`free` and manual ownership.
- Document ownership for any new heap allocations.
- Avoid leaking allocations on early returns; prefer a clear cleanup path.

## Pull Requests

### Before opening a PR
- Ensure the project builds in both **Debug** and **Release**.
- Run relevant tests.
- Update or add tests for new language features.

### PR scope
- Keep PRs small and reviewable.
- If a change spans parsing + AST + typecheck + lowering, consider landing in thin vertical slices.

### Commit messages
- Use imperative, descriptive messages (e.g. "Parse enum declarations", "Lower match to TIR").

## Reporting Issues

When reporting a bug, include:
- The smallest `.rane` program that reproduces the issue
- Expected vs actual behavior
- If applicable, the diagnostic message and span
- Visual Studio build configuration and any relevant logs
