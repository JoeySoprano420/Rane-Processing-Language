# RANE Processing Language HOWTO

This HOWTO reflects the **current bootstrap implementation** in this repository (C++14, Windows x64).

## Prerequisites

- Windows 10/11
- Visual Studio (C++14 toolset)

## Build

1. Open `Rane Processing Language.vcxproj` in Visual Studio.
2. Select `Release|x64`.
3. Build.

## Run / CLI

The built binary accepts an input `.rane` file and produces a minimal PE executable:

- `Rane Processing Language.exe hello.rane -o hello.exe -O2`

Supported optimization flags (currently parsed): `-O0 -O1 -O2 -O3`.

## Language subset (currently implemented)

### Hello World

`hello.rane`

```
let msg = "Hello, RANE!";
print(msg);
```

### Control flow

- `if` statements are supported and always lower with a valid `else` path.
- `while` loops are supported.

### Expressions

- Integer literals, pointer/string literals (used internally for strings).
- Binary arithmetic (basic).
- Comparisons: `< <= > >= == !=` produce `0/1` boolean values.

### I/O

- `print(x)` is a builtin lowered to an imported `printf` call.

## Output format

The compiler emits a minimal Windows x64 PE executable containing:

- `.text`: generated machine code
- `.rdata`: format string + user string data
- `.idata`: import table for `msvcrt.dll!printf`

Imported calls are patched using recorded **call fixups** (no heuristic call patching).

## Known limitations

- The language and runtime are incomplete; many “roadmap” features may be stubbed or not yet wired.
- The in-process loader demo (`rane_loader_impl.*`) reserves bands at exact addresses; this is a strict prototype.

## Troubleshooting

- If you see `Init failed: 6` in the demo path, that corresponds to `RANE_E_RESERVE_BAND_FAIL` and indicates band reservation failed.
  (The CLI compile path does not require loader init.)

- If `hello.exe` runs but prints nothing, verify `msvcrt.dll` is available and that the import patching logic is unchanged.