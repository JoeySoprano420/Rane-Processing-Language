# Onboarding: RANE Processing Language
**Last updated:** 2026-01-06

This document is the canonical onboarding guide for the RANE bootstrap compiler in this repository.

It covers:

- What RANE is (today vs planned)
- Who it’s for / where it fits
- Build + run instructions (Visual Studio 2026)
- How to compile `.rane` programs to EXEs and/or C
- Current language surface area (supported syntax)
- Native imports and MSVC link hints (`import … from "…";`, `link "…";`)
- How the compiler pipeline works internally
- Roadmap milestones and performance trajectory
- Where to look for the authoritative keyword/token list

> Scope note
>
> RANE is evolving rapidly. This doc is written to be **accurate and maintainable**:
> - “Current” sections reflect what this repo’s compiler can actually parse/lower/compile today.
> - “Planned” sections describe the trajectory but avoid claiming semantics that aren’t implemented yet.
> - The authoritative list of tokenized/reserved keywords is `rane_lexer.cpp` (`identifier_type()`).

---

## 1) What is RANE?

RANE is an experimental programming language and native code toolchain implemented in C/C++ (C++14-compatible build), currently targeting Windows x64.

### Current reality (bootstrap compiler)
RANE today is a working end-to-end bootstrap compiler that performs:
