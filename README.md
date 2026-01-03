# RANE Processing Language

## Overview

The **RANE Processing Language** is a cutting-edge, high-performance, secure programming language designed for deterministic, secure computing in production environments. It combines the efficiency of low-level systems programming with advanced compiler optimizations, comprehensive standard libraries, and robust security features. RANE is ideal for applications requiring high assurance, such as embedded systems, real-time processing, cryptography, and enterprise software.

RANE emphasizes:
- **Deterministic Execution**: Memory band enforcement, RWX policies, and crash diagnostics ensure predictable behavior.
- **High Performance**: Advanced optimizations, JIT compilation, and parallel execution for speed.
- **Security**: Built-in encryption, integrity checks, sandboxing, and access controls.
- **Scalability**: Supports complex programs with modular design, extensive libraries, and enterprise-level features.

The language compiles to x64 machine code via a sophisticated pipeline: Lexer ? Parser ? Type Checker ? TIR (Three-Address Intermediate Representation) ? Optimizations (SSA, Register Allocation, etc.) ? x64 Code Generation.

## Key Features

### Language Syntax
- **Variables and Types**: Support for integers (u64), booleans, and extensible types.
- **Control Flow**: `if-then-else`, `while-do`, jumps, markers.
- **Expressions**: Binary operations with precedence (`+`, `-`, `*`, `/`), function calls.
- **Functions**: Procedure definitions, calls, and inlining.
- **Concurrency**: Threads, mutexes, channels, barriers, futures.
- **Memory Management**: Reference counting GC, manual allocation, secure containers.

### Compiler Pipeline
1. **Lexer**: Tokenizes source code.
2. **Parser**: Builds AST from tokens.
3. **Type Checker**: Ensures type safety.
4. **TIR Generation**: Converts AST to intermediate representation.
5. **Optimizations**: SSA, register allocation, dead code elimination, inlining, vectorization, etc.
6. **Code Generation**: Emits x64 assembly.
7. **JIT/AOT**: Ahead-of-time or just-in-time compilation with hot path detection.

### Optimizations
RANE includes a full suite of compiler optimizations, supporting levels from -O0 (no optimization) to -Oz (minimum size), plus -Ofast and -flto. Key passes include:
- **IR-Level**: Constant folding, dead code elimination, inlining, GVN, LICM, loop unrolling, SCCP, etc.
- **Machine-Level**: Instruction scheduling, register allocation, peephole optimizations, branch relaxation.
- **Advanced**: Vectorization (SLP, interleaved), alias analysis, memory SSA, load/store forwarding, Mem2Reg.

### Standard Libraries
- **Core**: I/O (print, read), math (sin, cos, pow), string (strlen, strcmp), memory (memcpy, memset).
- **Data Structures**: Vectors, hash maps, binary search trees, heaps, graphs.
- **Algorithms**: Sorting (bubble, extensible), binary search, Dijkstra's shortest path.
- **Concurrency**: Thread pools, futures, barriers.
- **Networking**: TCP/UDP sockets, HTTP client/server.
- **Cryptography**: AES encryption, SHA256 hashing, secure random.
- **File I/O**: File operations, directory management.
- **Security**: Integrity checks, sandboxing, audit logging, resolvers.
- **Performance**: Profiling, benchmarking, parallel algorithms.

### Security Features
- **Memory Bands**: Enforced address spaces (CORE, AOT, JIT, META, HEAP, MMAP).
- **RWX Policies**: Prevents RWX pages, enforces W^X.
- **Crash Diagnostics**: Detailed records for violations.
- **Encryption**: Built-in AES and SHA for secure data handling.
- **Sandboxing**: Isolated execution environments.

### Runtime
- **JIT**: Dynamic compilation for hot paths, tiered execution.
- **GC**: Reference counting with mark-sweep extensions.
- **Exception Handling**: Try/catch/throw with stack unwinding.
- **Parallelism**: SIMD support, parallel loops, thread pools.

## Usage Examples

### Hello World
```
let msg = "Hello, RANE!";
print(msg);
```

### Fibonacci with Loops
```
let a = 0;
let b = 1;
while b < 100 do {
  let temp = a + b;
  a = b;
  b = temp;
  print(b);
}
```

### Concurrency
```
let pool = thread_pool_create(4);
thread_pool_submit(pool, fibonacci_task, NULL);
thread_pool_wait(pool);
```

### Networking
```
let sock = tcp_connect("example.com", 80);
tcp_send(sock, "GET / HTTP/1.0\r\n\r\n", 18);
let response = tcp_recv(sock, buf, 1024);
tcp_close(sock);
```

## Architecture

### Compiler Components
- **Lexer (rane_lexer.cpp)**: Tokenizes input.
- **Parser (rane_parser.cpp)**: Builds AST.
- **Type Checker (rane_typecheck.cpp)**: Validates types.
- **TIR (rane_tir.cpp)**: Intermediate representation.
- **SSA (rane_ssa.cpp)**: Static single assignment.
- **RegAlloc (rane_regalloc.cpp)**: Register allocation.
- **Optimize (rane_optimize.cpp)**: Optimization passes.
- **x64 Backend (rane_x64.cpp)**: Code generation.
- **AOT/JIT (rane_aot.cpp, rane_loader_impl.cpp)**: Compilation modes.

### Libraries
- **Stdlib (rane_stdlib.cpp)**: Core functions.
- **Data Structures**: HashMap, BST, Heap, Graph, Vector.
- **Extensions**: Net, Crypto, File, Thread, Security, Perf.

### Memory Model
- **Bands**: Reserved address ranges for different purposes.
- **GC**: Automatic memory management.
- **Containers**: Secure, lockable VM regions.

## Installation

### Prerequisites
- Windows 10+ (for VirtualAlloc-based memory management).
- Visual Studio 2017+ with C++14 support.
- CMake or direct MSBuild.

### Building
1. Clone the repository: `git clone https://github.com/example/rane-lang.git`
2. Open `Rane Processing Language.vcxproj` in Visual Studio.
3. Build the solution.
4. Run tests: `rane_test.exe`

### Command-Line Usage
- Compile: `rane_compiler.exe input.rane -o output.exe -O3`
- JIT: `rane_jit.exe hotpath.tir`
- Options: `-O0/-O1/-O2/-O3/-Os/-Oz/-Ofast/-flto/-march=native`

## Contributing

1. Fork the repo.
2. Create a feature branch.
3. Add tests.
4. Submit a PR.

Guidelines: Follow C++14, document code, ensure security.

## License

MIT License. See LICENSE file.