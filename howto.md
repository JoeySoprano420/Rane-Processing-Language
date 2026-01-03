# RANE Processing Language HOWTO

## Introduction

The **RANE Processing Language** is a high-performance, secure programming language designed for deterministic, secure computing. It compiles to x64 machine code with advanced optimizations, JIT compilation, and comprehensive security features. RANE is ideal for applications requiring high assurance, such as embedded systems, cryptography, and enterprise software.

This HOWTO guide walks you through installing, using, and developing with RANE.

## Installation and Setup

### Prerequisites
- **Operating System**: Windows 10 or later (for VirtualAlloc-based memory management).
- **Compiler**: Visual Studio 2017+ with C++14 support.
- **Build Tools**: CMake or MSBuild.
- **Hardware**: x64-compatible CPU.

### Building from Source
1. **Clone the Repository**:
   ```
   git clone https://github.com/example/rane-lang.git
   cd rane-lang
   ```

2. **Open in Visual Studio**:
   - Open `Rane Processing Language.vcxproj` in Visual Studio.

3. **Build the Project**:
   - Select Release or Debug configuration.
   - Build the solution (F7 or Build > Build Solution).
   - Output: `rane_compiler.exe`, `rane_jit.exe`, and libraries.

4. **Run Tests**:
   - Execute `rane_test.exe` to verify the build.

### Installation
- Copy `rane_compiler.exe` and libraries to your desired directory.
- Add to PATH for command-line access.

## Getting Started

### Writing Your First Program
Create a file `hello.rane`:

```
let msg = "Hello, RANE!";
print(msg);
```

### Compiling and Running
1. **Compile**:
   ```
   rane_compiler.exe hello.rane -o hello.exe -O2
   ```
   - `-O2`: Optimization level (O0-O3, Os, Oz, Ofast, flto, march=native).

2. **Run**:
   ```
   hello.exe
   ```
   Output: `Hello, RANE!`

### Basic Syntax
- **Variables**: `let x = 42;`
- **Types**: Integers (u64), booleans, extensible.
- **Expressions**: `x + y * 2`
- **Control Flow**: `if x > 0 then print("positive"); else print("non-positive");`
- **Loops**: `while x < 10 do { x = x + 1; }`
- **Functions**: `proc add(a, b) { return a + b; }`

## Language Features

### Data Types
- **Primitives**: `u64` (unsigned 64-bit int), `bool`.
- **Extensible**: Custom types via TIR.

### Control Flow
- **Conditional**: `if-then-else`
- **Loops**: `while-do`
- **Jumps**: `goto label;`

### Functions and Procedures
- Define: `proc func_name(params) { body }`
- Call: `func_name(args)`
- Inlining: Automatic at higher optimization levels.

### Concurrency
- **Threads**: `spawn thread_func;`
- **Mutexes**: `lock mutex; ... unlock mutex;`
- **Channels**: `send chan data; recv chan;`
- **Futures**: `let f = async task; wait f;`

### Memory Management
- **GC**: Reference counting with mark-sweep.
- **Manual**: `alloc(size)`, `free(ptr)`
- **Secure Containers**: Lockable VM regions.

## Advanced Topics

### Optimizations
RANE supports extensive optimizations:
- **Levels**: `-O0` (none) to `-O3` (aggressive), `-Os` (size), `-Oz` (min size), `-Ofast` (speed over standards), `-flto` (link-time), `-march=native` (CPU features).
- **Passes**: Constant folding, dead code elimination, inlining, GVN, LICM, loop unrolling, vectorization, etc.

Example:
```
rane_compiler.exe program.rane -O3 -march=native -o optimized.exe
```

### JIT Compilation
For dynamic code:
```
rane_jit.exe hotpath.tir -o jit_code.bin
```

### AOT Compilation
Ahead-of-time:
```
rane_compiler.exe module.rane -aot -o module.dll
```

### Security Features
- **Memory Bands**: Enforced address spaces (CORE, AOT, JIT, etc.).
- **RWX Policies**: Prevents RWX pages.
- **Crash Diagnostics**: Detailed violation records.
- **Encryption**: Built-in AES/SHA.

Enable policies in code or via flags.

## Standard Library

### Core Functions
- **I/O**: `print(str)`, `read_int()`
- **Math**: `abs(x)`, `sqrt(x)`, `sin(x)`, `cos(x)`, `pow(base, exp)`
- **String**: `strlen(s)`, `strcmp(s1, s2)`, `strcpy(dst, src)`, `strcat(dst, src)`
- **Memory**: `memcpy(dst, src, n)`, `memset(s, c, n)`, `memcmp(s1, s2, n)`

### Data Structures
- **Vectors**: Dynamic arrays.
- **Hash Maps**: Key-value stores.
- **Binary Search Trees**: Ordered data.
- **Heaps**: Priority queues.
- **Graphs**: Nodes and edges.

### Algorithms
- **Sorting**: Bubble sort (extensible).
- **Searching**: Binary search.
- **Graph**: Dijkstra's shortest path.

### Concurrency
- **Thread Pools**: `thread_pool_create(n)`, `thread_pool_submit(pool, task)`
- **Futures**: `async(fn)`, `future_get(f)`
- **Barriers**: `barrier_create(count)`, `barrier_wait(b)`

### Networking
- **Sockets**: `tcp_connect(host, port)`, `tcp_send(sock, data)`, `tcp_recv(sock, buf)`
- **HTTP**: `http_get(url, response)`

### Cryptography
- **AES**: `aes_encrypt(key, in, out)`, `aes_decrypt(key, in, out)`
- **SHA256**: `sha256(data, len, hash)`
- **Random**: `secure_random(buf, len)`

### File I/O
- **Files**: `file_open(path, mode)`, `file_read(f, buf, size)`, `file_write(f, buf, size)`, `file_close(f)`
- **Directories**: `dir_create(path)`, `dir_list(path, entries)`

### Security
- **Integrity**: `security_check_integrity(data, hash)`
- **Sandbox**: `security_sandbox_enter()`
- **Audit**: `security_audit_log(event)`
- **Resolvers**: `resolve_symbol(name)`, `resolve_address(symbol)`

### Performance
- **Profiling**: `profiler_start()`, `profiler_stop(p)`, `profiler_elapsed_ns(p)`
- **Parallel**: `parallel_sort(arr, n)`, `parallel_for(start, end, fn)`

## Development

### Extending the Compiler
- **Add Passes**: Modify `rane_optimize.cpp` for new optimizations.
- **New Libraries**: Create `.h` and `.cpp` files, include in `main.cpp`.
- **TIR Extensions**: Update `rane_tir.h` for new opcodes.

### Contributing
1. Fork the repo.
2. Create a branch: `git checkout -b feature/new-feature`
3. Commit changes.
4. Push and create a PR.
5. Guidelines: C++14, document code, ensure security.

### Testing
- Add tests in `rane_test.cpp`.
- Run: `rane_test.exe`

## Examples

### Fibonacci with Concurrency
```
proc fib(n) {
  if n <= 1 return n;
  let f1 = async fib(n-1);
  let f2 = fib(n-2);
  return future_get(f1) + f2;
}

let result = fib(10);
print(result);
```

### Networking Server
```
let sock = tcp_listen(8080);
while true {
  let client = tcp_accept(sock);
  spawn handle_client(client);
}
```

### Cryptography
```
let key = aes_key_new("secretkey", 9);
let encrypted = aes_encrypt(key, "plaintext", out);
print(encrypted);
```

## Troubleshooting

### Common Issues
- **Build Errors**: Ensure C++14 support in VS.
- **Runtime Crashes**: Check memory bands and RWX policies.
- **Optimization Failures**: Use lower levels like `-O1`.
- **JIT Issues**: Verify TIR validity.

### Debugging
- Use `rane_diag_publish_block()` for diagnostics.
- Logs: Check console output.

### Support
- Issues: GitHub repo.
- Docs: README.md, this HOWTO.

---

This HOWTO provides a complete guide to RANE. For API details, see header files. Happy coding!