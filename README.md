---

# 🌐 **THE RANE LANGUAGE — COMPLETE SYSTEM OVERVIEW (All Updates Considered)**  
*A deterministic, capability‑aware, parallel systems language for explicit, analyzable, low‑level programming.*

---

# 1. **Identity & Philosophy**

RANE is a **deterministic, strongly typed, statically checked, procedural + expression‑oriented systems language** designed for:

- predictable execution  
- explicit memory control  
- safe parallelism  
- sandboxed computation  
- real‑time DSP  
- secure plugin architectures  
- portable AOT compilation  

Its core principles:

### **1. Determinism**
Same input → same output → same machine code.  
No hidden allocations, no implicit conversions, no exceptions.

### **2. Explicitness**
The programmer controls memory, effects, concurrency, and lifetimes.

### **3. Safety**
Spatial + temporal safety via:
- VM containers  
- bounds checks  
- capability enforcement  
- deterministic concurrency  

### **4. Transparency**
Readable compiler pipeline, inspectable IR, predictable runtime.

### **5. Portability**
Two backends:
- Native x86‑64 (PE64 executable writer)
- Portable C backend

---

# 2. **Language Surface (Syntax & Semantics)**

RANE’s syntax is defined by:

- **Hand‑written lexer** (`rane_lexer.cpp`)
- **Hand‑written parser** (`rane_parser.cpp`)
- **Formal ANTLR grammar** (`grammar.g4`)
- **Syntax coverage suite** (`syntax.rane`)

Together, these define the canonical language.

### **2.1 Top‑Level Constructs**
- `fn` — functions  
- `struct` — user‑defined types  
- `enum` — enumerations  
- `import` — module imports  
- `capability` — effect declarations  

### **2.2 Types**
Primitive:
- `int`, `i8`, `i16`, `i32`, `i64`
- `u8`, `u16`, `u32`, `u64`
- `f32`, `f64`
- `bool`
- `void`

Composite:
- pointers: `*T`
- arrays: `[N]T`
- structs
- function types: `fn(a: T, b: U) -> R`

### **2.3 Expressions**
- arithmetic  
- logical  
- comparison  
- function calls  
- indexing  
- pointer dereference  
- address-of  
- struct field access  

### **2.4 Statements**
- variable declarations  
- assignment  
- `if` / `else`  
- `while`  
- `for i in a..b`  
- `return`  
- blocks  

### **2.5 Entry Point**
```
fn main() -> int { ... }
```

---

# 3. **Compiler Architecture**

The compiler is a multi‑stage AOT pipeline:

### **3.1 Lexing**
`rane_lexer.cpp`  
- tokenizes source  
- handles keywords, identifiers, literals, operators  
- supports comments and whitespace skipping  

### **3.2 Parsing**
`rane_parser.cpp`  
- builds AST  
- enforces grammar rules  
- produces structured program representation  

### **3.3 Typechecking**
- verifies type correctness  
- enforces return types  
- checks struct field access  
- validates pointer usage  
- ensures capability requirements  

### **3.4 Capability Checking**
- functions may require capabilities  
- call sites must satisfy capability constraints  

### **3.5 TIR (Typed Intermediate Representation)**
- explicit types  
- SSA‑friendly  
- lowered control flow  
- explicit memory operations  

### **3.6 SSA Construction**
- variable renaming  
- phi insertion  
- dominance analysis  

### **3.7 Register Allocation**
- linear scan  
- spill management  
- x64 register mapping  

### **3.8 Backends**
#### **A. Native x86‑64 Backend**
`rane_x64.cpp`  
- emits machine code  
- uses PE64 writer  
- patches string literals  
- handles imports (e.g., `printf`)  
- produces Windows executables  

#### **B. C Backend**
`rane_c_backend.cpp`  
- emits portable C99/C11  
- allows cross‑platform compilation  
- useful for debugging and portability  

### **3.9 Driver**
`rane_driver.cpp`  
- orchestrates compilation  
- writes PE64 executables  
- writes C output  
- handles string patching  
- aligns sections  
- manages imports  

### **3.10 CLI**
`main.cpp`  
- parses command‑line arguments  
- selects compilation mode  
- dispatches to driver  

---

# 4. **Runtime Architecture**

The runtime includes:

### **4.1 VM Container System**
`rane_vm.cpp`  
Provides:
- safe memory regions  
- bounds‑checked reads/writes  
- subspans/slices  
- deterministic lifetime management  
- temporal safety  

This is the foundation for sandboxing and plugin safety.

### **4.2 Concurrency Runtime**
- threads  
- mutexes  
- channels  
- semaphores  
- barriers  
- atomics  
- once‑init  

Deterministic unless explicitly relaxed.

### **4.3 Optional GC Subsystem**
`rane_gc.cpp`  
- experimental  
- not default  
- used for high‑level or host‑scripting scenarios  
- mark/sweep prototype  

### **4.4 Standard Library**
Includes:

#### **Core**
- `print`, `strlen`, `memcmp`, math functions

#### **Containers**
- dynamic vectors  
- safe growth  
- bounds checking  

#### **Algorithms**
- sorting  
- searching  
- Dijkstra  

#### **Concurrency**
- thread API  
- channel API  
- synchronization primitives  

#### **DSP**
- FFT  
- windows (Hann, Hamming, Blackman)  
- filters (biquad)  
- RMS, peak detection  
- mixing utilities  

---

# 5. **Safety Model**

RANE enforces:

### **5.1 Spatial Safety**
- bounds checks  
- pointer validity checks  
- safe container operations  

### **5.2 Temporal Safety**
- VM region expiration  
- no use‑after‑free in container APIs  

### **5.3 Capability Safety**
- functions declare required capabilities  
- call sites must satisfy them  
- runtime enforces capability boundaries  

### **5.4 Deterministic Concurrency**
- no data races in safe code  
- channels and atomics are explicit  

---

# 6. **Binary Output Model**

### **6.1 PE64 Executable Writer**
`rane_driver.cpp` + `rane_x64.cpp`  
- constructs DOS header  
- NT header  
- import tables  
- code section  
- data section  
- string literal patching  
- alignment rules  

### **6.2 C Backend**
`rane_c_backend.cpp`  
- emits portable C  
- useful for debugging  
- enables cross‑platform builds  

---

# 7. **Tooling & Ecosystem**

### **7.1 Syntax Coverage Suite**
`syntax.rane`  
- canonical examples  
- parser regression suite  
- language surface demonstration  

### **7.2 Formal Grammar**
`grammar.g4`  
- ANTLR grammar  
- defines tokens + rules  
- used for tooling and editor support  

### **7.3 Documentation**
- The RANE Book (in progress)  
- Language Specification (drafted)  
- Roadmap to 1.0  

---

# 8. **Current Strengths**

RANE is already strong in:

- deterministic systems programming  
- explicit memory control  
- safe parallelism  
- real‑time DSP  
- sandboxed execution  
- portable AOT compilation  
- low‑level binary emission  
- strong type system  
- clear, readable syntax  

---

# 9. **Current Limitations**

- no module system yet  
- no package manager  
- no generics  
- no pattern matching  
- limited debugging tools  
- GC is experimental  
- only PE64 native backend (SysV/ELF not yet implemented)  

---

# 10. **Overall Assessment**

RANE is now a **real, coherent, powerful systems language** with:

- a complete compiler pipeline  
- a deterministic runtime  
- a safety‑oriented VM  
- a robust stdlib  
- a formal grammar  
- a syntax coverage suite  
- two backends  
- a clear philosophy  
- a roadmap to 1.0  

It is no longer a prototype.  
It is a language with an identity, an architecture, and a future.

---

# 🌐 **RANE: Official Language Overview**  
*A deterministic, capability‑aware, parallel systems language for explicit, analyzable, low‑level programming.*

---

## **1. Introduction**

RANE is a modern systems programming language designed for developers who need **determinism**, **explicit control**, and **high‑performance native execution** without sacrificing safety or clarity. It combines a clean, approachable syntax with a rigorous execution model, making it suitable for:

- real‑time DSP  
- parallel compute workloads  
- secure plugin architectures  
- deterministic simulation  
- systems tooling  
- sandboxed execution environments  

RANE compiles ahead‑of‑time to either:

- **native x86‑64 executables**, or  
- **portable C code**  

This dual‑backend design ensures both performance and portability.

---

## **2. Language Philosophy**

RANE is built on five core principles:

### **Determinism**
Programs behave the same way every time.  
No hidden allocations, no implicit conversions, no exceptions.

### **Explicitness**
Memory, concurrency, and effects are always visible and controlled by the programmer.

### **Safety**
Spatial and temporal safety are enforced through:
- bounds checks  
- VM containers  
- capability restrictions  
- deterministic concurrency  

### **Transparency**
The compiler pipeline is readable and inspectable.  
The runtime is small, predictable, and auditable.

### **Portability**
Native x64 backend + C backend = broad platform reach.

---

## **3. Language Surface**

RANE’s syntax is defined by:

- a hand‑written lexer (`rane_lexer.cpp`)  
- a hand‑written parser (`rane_parser.cpp`)  
- a formal ANTLR grammar (`grammar.g4`)  
- a comprehensive syntax coverage suite (`syntax.rane`)  

Together, these form the canonical definition of the language.

### **3.1 Top‑Level Constructs**
- `fn` — functions  
- `struct` — user‑defined types  
- `enum` — enumerations  
- `import` — module imports  
- `capability` — effect declarations  

### **3.2 Types**
Primitive:
- `int`, `i8`, `i16`, `i32`, `i64`  
- `u8`, `u16`, `u32`, `u64`  
- `f32`, `f64`  
- `bool`, `void`

Composite:
- pointers: `*T`  
- arrays: `[N]T`  
- structs  
- function types: `fn(a: T, b: U) -> R`

### **3.3 Expressions**
- arithmetic  
- logical  
- comparison  
- function calls  
- indexing  
- pointer dereference  
- address‑of  
- struct field access  

### **3.4 Statements**
- variable declarations  
- assignment  
- `if` / `else`  
- `while`  
- `for i in a..b`  
- `return`  
- blocks  

### **3.5 Entry Point**
```
fn main() -> int { ... }
```

---

## **4. Compiler Architecture**

RANE uses a multi‑stage AOT pipeline:

### **4.1 Lexing**
`rane_lexer.cpp`  
Tokenizes source code into identifiers, literals, operators, and keywords.

### **4.2 Parsing**
`rane_parser.cpp`  
Builds the AST and enforces grammar rules.

### **4.3 Typechecking**
Ensures:
- type correctness  
- valid pointer usage  
- correct return types  
- capability requirements  

### **4.4 Capability Checking**
Functions may declare required capabilities.  
Call sites must satisfy them.

### **4.5 TIR (Typed Intermediate Representation)**
A typed, SSA‑friendly IR used for optimization and backend lowering.

### **4.6 SSA Construction**
- dominance analysis  
- phi insertion  
- variable renaming  

### **4.7 Register Allocation**
Linear‑scan allocator targeting x64.

### **4.8 Backends**

#### **Native x86‑64 Backend**
`rane_x64.cpp`  
- emits machine code  
- constructs PE64 executables  
- patches string literals  
- manages imports (e.g., `printf`)  

#### **C Backend**
`rane_c_backend.cpp`  
- emits portable C99/C11  
- useful for debugging and cross‑platform builds  

### **4.9 Driver**
`rane_driver.cpp`  
- orchestrates compilation  
- writes PE64 executables  
- writes C output  
- handles string patching and alignment  

### **4.10 CLI**
`main.cpp`  
- parses command‑line arguments  
- selects compilation mode  
- dispatches to the driver  

---

## **5. Runtime Architecture**

### **5.1 VM Container System**
`rane_vm.cpp`  
Provides:
- safe memory regions  
- bounds‑checked reads/writes  
- subspans/slices  
- deterministic lifetimes  
- temporal safety  

This is the foundation for sandboxing and plugin safety.

### **5.2 Concurrency Runtime**
Includes:
- threads  
- mutexes  
- channels  
- semaphores  
- barriers  
- atomics  
- once‑init  

Deterministic unless explicitly relaxed.

### **5.3 Optional GC Subsystem**
`rane_gc.cpp`  
- experimental  
- not enabled by default  
- used for high‑level or host‑scripting scenarios  

### **5.4 Standard Library**

#### **Core**
- `print`, `strlen`, `memcmp`, math functions

#### **Containers**
- dynamic vectors  
- safe growth  
- bounds checking  

#### **Algorithms**
- sorting  
- searching  
- Dijkstra  

#### **Concurrency**
- thread API  
- channel API  
- synchronization primitives  

#### **DSP**
- FFT  
- window functions  
- biquad filters  
- RMS, peak detection  
- mixing utilities  

---

## **6. Safety Model**

RANE enforces:

### **6.1 Spatial Safety**
- bounds checks  
- pointer validity checks  
- safe container operations  

### **6.2 Temporal Safety**
- VM region expiration  
- no use‑after‑free in container APIs  

### **6.3 Capability Safety**
- functions declare required capabilities  
- call sites must satisfy them  

### **6.4 Deterministic Concurrency**
- no data races in safe code  
- channels and atomics are explicit  

---

## **7. Binary Output Model**

### **7.1 PE64 Executable Writer**
`rane_driver.cpp` + `rane_x64.cpp`  
Constructs:
- DOS header  
- NT header  
- import tables  
- code section  
- data section  
- string literal patches  

### **7.2 C Backend**
`rane_c_backend.cpp`  
- emits portable C  
- enables cross‑platform builds  

---

## **8. Tooling & Ecosystem**

### **8.1 Syntax Coverage Suite**
`syntax.rane`  
- canonical examples  
- parser regression suite  
- language surface demonstration  

### **8.2 Formal Grammar**
`grammar.g4`  
- ANTLR grammar  
- used for tooling and editor support  

### **8.3 Documentation**
- The RANE Book (in progress)  
- Language Specification (drafted)  
- Roadmap to 1.0  

---

## **9. Current Strengths**

RANE excels in:

- deterministic systems programming  
- explicit memory control  
- safe parallelism  
- real‑time DSP  
- sandboxed execution  
- portable AOT compilation  
- low‑level binary emission  
- strong type system  
- clean, readable syntax  

---

## **10. Current Limitations**

- no module system yet  
- no package manager  
- no generics  
- no pattern matching  
- limited debugging tools  
- GC is experimental  
- only PE64 native backend  

---

## **11. Summary**

RANE is now a **coherent, powerful, fully realized systems language** with:

- a complete compiler pipeline  
- a deterministic runtime  
- a safety‑oriented VM  
- a robust standard library  
- a formal grammar  
- a syntax coverage suite  
- two backends  
- a clear philosophy  
- a roadmap to 1.0  

It is no longer a prototype.  
It is a language with an identity, an architecture, and a future.

---

