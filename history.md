# RANE Processing Language History

This file tracks notable milestones for the codebase in this repository.

## Milestones (repository-focused)

### Early bootstrap
- Initial implementation of a simple compiler pipeline: lexer ? parser ? AST ? TIR ? x64 emission.
- Added a minimal PE writer to produce a Windows x64 executable.

### Control-flow and comparison correctness
- Fixed `if` lowering to always generate a valid `else` flow (explicit `then/else/end` labels).
- Added comparison lowering so `< <= > >= == !=` materialize a boolean `0/1` value.
- Added conditional jump variant support in the x64 emitter (`je/jne/jl/jle/jg/jge`).

### Deterministic import call patching
- Removed heuristic “patch the first call” behavior.
- Added import call-site fixups recorded during codegen and returned via AOT compilation so the driver can patch imported calls deterministically.

### Loader demo stability
- The in-process demo uses a strict band-reserving loader prototype.
- Updated demo layout defaults to provide valid band base/size values so `rane_loader_init` does not fail due to missing layout fields.

## Notes

This history intentionally avoids speculative roadmap claims and focuses on changes that exist in the code.
