# About RANE

RANE is an experimental programming language and native toolchain implemented in C++14.

This repository currently contains a **bootstrap compiler** that targets **Windows x64** and emits a small PE executable. The goal is to keep the system small enough to iterate on quickly while still exercising real compiler plumbing: parsing, type checking, IR lowering, fixups, and machine code emission.

## What’s in this repository

### Language (current subset)
- `let` bindings
- Integer and string literals (strings are currently used primarily for `print`)
- Expressions with basic arithmetic
- Bitwise operators: `& | ^`, plus word-forms `xor` and unary `not` / `~`
- Shift operators (word-form): `shl`, `shr`, `sar`
- Comparisons: `< <= > >= == !=` (materialize a `0/1` boolean)
- Control flow: `if` and `while`
- Builtin `print(x)`

### Compiler pipeline
- Lexer ? Parser ? AST
- Type checking (bootstrap level)
- AST ? Typed IR (TIR)
- TIR ? x64 machine code emission
- Minimal PE writer (`.text + .rdata + .idata`) that imports `msvcrt.dll!printf`

### Fixups (correctness work)
- Label fixups for rel32 branches
- Import call-site fixups for deterministic patching of imported calls (no call “heuristics”)

### Loader prototype (demo path)
The repo also includes a Windows-specific loader prototype that reserves fixed “bands” of virtual address space (CORE/AOT/JIT/META/HEAP/MMAP). This is primarily used for experimentation around W^X and executable-memory policy.

## What RANE is not (yet)

Some parts of the codebase include placeholders for a larger roadmap (advanced optimization passes, richer runtime, larger standard library, more complete IR). Those are not all fully wired today.

If you’re evaluating the project, treat it as a **work-in-progress compiler and runtime sandbox**, not a finished production language.

## Quick start

Example program (`hello.rane`):

```
let msg = "Hello, RANE!";
print(msg);
```

Build the solution (Visual Studio, C++14), then run:

- `Rane Processing Language.exe hello.rane -o hello.exe -O2`

## Design goals (near-term)

- Keep the bootstrap pipeline easy to modify.
- Improve correctness in lowering and codegen (especially control flow + call/import handling).
- Grow the optimizer incrementally (constant folding/propagation, CFG-aware DCE).
- Expand language coverage and tighten diagnostics.

## License

MIT (see `LICENSE` if present in the repo).
