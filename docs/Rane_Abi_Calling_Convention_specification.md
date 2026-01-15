# RANE ABI v1.0 — Calling Convention Specification

## 1. Overview

**Goals:**

- **Deterministic:** No ambiguity in how arguments, returns, and frames are laid out.
- **Native:** Conform to the platform’s standard C ABI (MSVC on Windows, SysV AMD64 on Unix‑like).
- **Auditable:** Every stack slot and register use is predictable and reconstructible from IR.
- **Optimizable:** RSP‑only by default, with optional frame pointer in debug builds.

RANE does **not** invent a new ABI; it **adopts and codifies** the platform ABI with explicit rules for:

- Register usage  
- Stack layout  
- Parameter passing  
- Return values  
- Prologue/epilogue  
- Variadic calls  
- Struct/enum layout  

---

## 2. Common x86‑64 conventions

### 2.1 Integer and pointer registers

- **General-purpose registers:** `rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8–r15`
- **Caller-saved (volatile):**
  - MSVC: `rax, rcx, rdx, r8, r9, r10, r11`
  - SysV: `rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11`
- **Callee-saved (non-volatile):**
  - MSVC: `rbx, rbp, rdi, rsi, r12–r15`
  - SysV: `rbx, rbp, r12–r15`

### 2.2 Floating-point / vector registers

- **XMM0–XMM15** used for scalar and vector FP arguments/returns.
- Caller-saved on both ABIs.

### 2.3 Stack alignment

- At any call site, **RSP must be 16‑byte aligned** *before* the `call` instruction.

---

## 3. Windows (MSVC) ABI for RANE

### 3.1 Parameter passing

**Integer/pointer arguments:**

- First four arguments: `rcx, rdx, r8, r9`
- Remaining arguments: passed on the stack, right‑to‑left, 8‑byte slots.

**Floating-point arguments:**

- First four FP arguments: `xmm0–xmm3`
- If more than four, remaining go on the stack in 8‑byte slots.

**Shadow space:**

- Every call reserves **32 bytes (4 × 8)** of “shadow space” on the stack for the callee’s use.
- This space is reserved by the **caller** immediately before the call, regardless of argument count.

### 3.2 Return values

- **Integer/pointer:** `rax`
- **Floating-point:** `xmm0`
- **Small structs/enums (≤ 16 bytes):**
  - Returned in `rax`/`rdx` or `xmm0`/`xmm1` according to MSVC rules.
- **Larger aggregates:**
  - Caller allocates space and passes a hidden pointer in `rcx`.
  - Callee writes into that buffer and returns normally (often `void`).

### 3.3 Stack frame layout (typical rsp‑only)

At function entry (after prologue):

- `rsp` points to:
  - Local variables (negative offsets from `rsp`)
  - Saved non‑volatile registers (if any)
  - Below that: caller’s shadow space and arguments

**Prologue (release mode, rsp‑only):**

```asm
; caller:
sub  rsp, 32 + <arg_stack_space>   ; shadow space + stack args
; ... move args into rcx, rdx, r8, r9 / stack ...
call target
add  rsp, 32 + <arg_stack_space>
```

```asm
; callee:
sub  rsp, <local_stack_size>       ; align to 16 bytes
; ... function body ...
add  rsp, <local_stack_size>
ret
```

**Prologue (debug mode, rbp frame):**

```asm
push rbp
mov  rbp, rsp
sub  rsp, <local_stack_size>
; ... body ...
mov  rsp, rbp
pop  rbp
ret
```

### 3.4 Caller vs callee responsibilities

**Caller:**

- Align `rsp` to 16 bytes before `call`.
- Reserve 32 bytes of shadow space.
- Place first four args in registers, rest on stack.
- Assume caller-saved registers may be clobbered.

**Callee:**

- Preserve callee-saved registers.
- Restore `rsp` to its entry value before `ret`.
- Use shadow space if desired for spills or home slots.

---

## 4. SysV AMD64 ABI for RANE

### 4.1 Parameter passing

**Integer/pointer arguments:**

- First six: `rdi, rsi, rdx, rcx, r8, r9`
- Remaining: on stack, right‑to‑left, 8‑byte slots.

**Floating-point arguments:**

- First eight FP arguments: `xmm0–xmm7`
- Remaining: on stack.

No shadow space concept in SysV.

### 4.2 Return values

- **Integer/pointer:** `rax` (and `rdx` for some multi‑word returns)
- **Floating-point:** `xmm0` (and `xmm1` for some multi‑word FP returns)
- **Small structs/enums:** returned in registers according to SysV classification (INTEGER, SSE, etc.).
- **Larger aggregates:** caller passes a hidden pointer in `rdi`; callee writes into it.

### 4.3 Stack frame layout (rsp‑only)

At function entry:

- `rsp` points to return address.
- Below that: caller’s stack arguments.
- Callee subtracts space for locals and spills.

**Prologue (release mode, rsp‑only):**

```asm
sub  rsp, <local_stack_size>   ; ensure 16-byte alignment
; ... body ...
add  rsp, <local_stack_size>
ret
```

**Prologue (debug mode, rbp frame):**

```asm
push rbp
mov  rbp, rsp
sub  rsp, <local_stack_size>
; ... body ...
mov  rsp, rbp
pop  rbp
ret
```

### 4.4 Caller vs callee responsibilities

**Caller:**

- Align `rsp` to 16 bytes before `call`.
- Place first six integer/pointer args in registers, rest on stack.
- Place first eight FP args in `xmm0–xmm7`.
- Assume caller-saved registers may be clobbered.

**Callee:**

- Preserve callee-saved registers (`rbx, rbp, r12–r15`).
- Restore `rsp` before `ret`.

---

## 5. RANE-specific rules on top of the platform ABI

### 5.1 Deterministic lowering

- The compiler must produce a **single, canonical calling sequence** for any given function signature on a given target.
- No ad‑hoc calling conventions per function unless explicitly annotated (e.g., `extern "C"` vs `extern "sysv"` vs `extern "win64"`).

### 5.2 Struct and enum layout

- Layout follows the platform C ABI:
  - Field order is declaration order.
  - Alignment and padding follow the platform’s rules.
- Enums:
  - Backed by a fixed integer type (e.g., `i32` by default) unless otherwise specified.
  - Tagged unions / variants are lowered to:
    - A tag field (integer)  
    - A payload area, padded and aligned appropriately.

### 5.3 Variadic functions

- RANE variadic functions (if supported) must follow the platform’s C variadic ABI:
  - MSVC: all variadic arguments passed in registers and/or stack as per C rules.
  - SysV: classification rules for integer/SSE, with `va_list` semantics.

### 5.4 Tail calls

- Tail-call optimization is allowed only when:
  - The callee uses the same calling convention.
  - Argument layout is compatible.
  - Stack alignment is preserved.
- When performed, the compiler:
  - Adjusts `rsp` to the caller’s state.
  - Jumps directly to the callee (`jmp` instead of `call`/`ret`).

### 5.5 Exception / unwind model

- RANE itself does not define language-level exceptions in this ABI.
- If interop with C++ exceptions or SEH is needed, those are treated as **foreign mechanisms** and must be explicitly annotated and handled.

---

## 6. Debug vs release modes

### Release mode

- **rsp‑only** frames by default.
- Frame pointer (`rbp`) treated as a general-purpose register.
- Full use of caller-saved registers.
- Optional DWARF/Windows unwind info for stack walking.

### Debug mode

- **rbp frame pointer** enabled by default.
- Simpler prologue/epilogue for easier debugging and stack traces.
- Optional reduction in inlining and tail calls for clarity.

---

## 7. Summary

- RANE’s ABI is **platform-native**:
  - MSVC x64 on Windows.
  - SysV AMD64 on Unix‑like systems.
- It is **rsp‑only by default**, with optional `rbp` frames in debug builds.
- It defines clear, deterministic rules for:
  - Register usage  
  - Stack layout  
  - Parameter passing  
  - Return values  
  - Struct/enum layout  
  - Tail calls and variadics  

