# 🌐 **THE RANE LANGUAGE — COMPLETE SYSTEM OVERVIEW 

Reliable Adaptive Natural Efficient (RANE Proccessing Language) Create a programmjng language --processing ideas (through instructions) to execution-- P.I.E.

syntax.rane
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
Lexer / Tokenizer
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
Parser
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
AST
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
Resolver
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
Typed CIL (Typed Common Intermediary Language)
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
OSW (Optimized Structure Web)
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
Frame Planner
   ↓ ➡️CIAMs (Contextual Inference Abstraction Macros)
Codegen (.exe x64 PE) / NASM Emitters




* Reliable Adaptive Natural Efficient (RANE Proccessing Language)
Create a programmjng language  

--processing ideas to instructions to execution-- P.I.E. 

Paradigm: performance-oriented systems whisperer

This language specializes bare-metal communication and manipulation

This language has a standard library built for creation

Philosophy: No smoke and mirrors; direct and precise

Syntax is heavily maxhine natural; yet extremely human-friendly

The grammar is ultimately cohesive and the most human-oriented ever

This is a professional production majorscale multi-industry language

The semantics are basic and beginner welcoming

The structure is heavily prose-focussed

Programs are written in instructional directions that match 1:1 machine operable Inherently

Symbols mean what they mean in human language like > means greater than, -> means into/or then etc

Static yet flexible spacing

Indentation is informal

String typed and determanistic control flows

Structs are independent 

Programs are basically a network of seperate command-nodes

Punctuation is minimal 

Memory is layered-stacks (under the hokd) with representative virtual registers (user-facing) 

Dynamic AOT compilation

Completely runtime-free by default

Intresic instruction set by design

Automatic/self adapting dictionary

Explicit states

Immutable classes

Mutable objects

Safety is baked into syntax

Security is compile time centric

Speed is runtime proficient

Consistency-dominant concurrency

Processes replace routines

Parallelism is controlled by syncronized commands withing writing the program

Logic is coherent-connection throughout the program to the execution

Pattern matching is ingrained in compiled

Optimizations are baked into how the code is written

Intuititve build system

Package manager is baked into sequential render and export logic

Tooling is dynamic

Wrapping, binding, glue, are deterministic 

Imports and exports are user-defined

Compression is baked into how users wdite their code for each program individually

User-defined sorting

Strong static types are runtime-frieny

Primatives are very machine-friendly

Strings and booleans are very execution-friendly

Rucursion is matb-based

Polynomial-fibonacci Ciphers replace obfuscation *

**********

This is the syntax for RANE:

import rane_rt_print
import rane_rt_fs
import rane_rt_net
import rane_rt_time
import rane_rt_threads
import rane_rt_channels
import rane_rt_alloc
import rane_rt_crypto

module demo_root

namespace math:
  export inline proc square x i64 -> i64:
    return x * x
  end

  export inline proc abs_i64 x i64 -> i64:
    if x < 0:
      return -x
    else:
      return x
    end
  end

  private proc hidden -> i32:
    return 0
  end
end

import math::square
import math::abs_i64

public proc exported_fn -> i32:
  return 7
end

type i8
type i16
type i32
type i64
type i128
type i512

type u8
type u16
type u32
type u64
type u128
type u512

type f32
type f64
type f128

type bool
type void
type int
type string

typealias word = u32
alias int32 = i32

const PI f64 = 3.141592653589793
constexpr E f64 = 2.718281828459045
constinit ZERO i64 = 0

consteval proc const_fn -> i64:
  return 42
end

@derive Eq Ord Debug
struct Person:
  name string
  age u8
end

enum Flags u8:
  None = 0
  Read = 1
  Write = 2
  Exec = 4
  ReadWrite = Read | Write
end

enum Color i32:
  Red = 0
  Green = 1
  Blue = 2
end

variant Maybe<T>:
  Some T
  None
end

union IntOrFloat:
  i i32
  f f32
end

struct Header:
  magic u32
  version u16
  flags u16
  size u64
end

struct Point:
  x i32
  y i32
end

struct Vec3:
  x i64
  y i64
  z i64
end

mmio region REG from 4096 size 256

capability heap_alloc
capability file_io
capability network_io
capability dynamic_eval
capability syscalls
capability threads
capability channels
capability crypto

admin proc admin_fn -> i32:
  return 0
end

protected proc prot_fn -> i32:
  return 1
end

public proc pub_fn -> i32:
  return 2
end

private proc priv_fn -> i32:
  return 3
end

contract positive x i64:
  ensures x > 0
end

proc assert_example x i64 -> i64:
  assert x != 0 "x must be non-zero"
  return x
end

macro SQUARE x = x * x

template T
proc generic_id x T -> T:
  return x
end

mutex m1
channel<int> ch

async proc async_fetch -> i64 requires network_io:
  let v i64 = await rane_rt_net.fetch_i64 "https://example"
  return v
end

dedicate proc spawn_worker iter i64 -> i64 requires threads:
  let total i64 = 0
  for let i i64 = 0; i < iter; i = i + 1:
    total = total + i
  end
  return total
end

proc file_read_example path string -> string requires file_io:
  with open path as f:
    let s string = f.read
    return s
  end
end

proc defer_example path string -> i32 requires file_io:
  let f = open path
  defer close f
  write f "hello"
  return 0
end

proc asm_example -> i64 requires syscalls:
  let out i64 = 0
  asm:
    mov rax 1
    add rax 2
    mov out rax
  end
  return out
end

proc try_example -> i32:
  try:
    throw 100
  catch e:
    print e
  finally:
    print "done"
  end
  return 0
end

proc eval_example x string -> i64 requires dynamic_eval:
  let res i64 = eval "10 + " + x
  print res
  return res
end

proc add5 a i64 b i64 c i64 d i64 e i64 -> i64:
  return a + b + c + d + e
end

proc identity<T> x T -> T:
  return x
end

proc choose_demo a i64 b i64 -> i64:
  let mx i64 = choose max a b
  let mn i64 = choose min a b
  return mx + mn
end

proc collection_demo -> i64 requires heap_alloc:
  let arr [5]i64 = [1 2 3 4 5]
  let vec = vector 1 2 3
  let table = map "a" -> 1 "b" -> 2

  let tup = (1 "hi" true)
  let (x0 i64 x1 string x2 bool) = tup

  print arr[0]
  print vec.len
  print table.get "a"
  print x1

  return x0 + table.get "b"
end

linear proc lin_inc x i64 -> i64:
  return x + 1
end

nonlinear proc nlin_mul x i64 -> i64:
  return x * 2
end

proc ownership_example -> i32 requires heap_alloc:
  let p = allocate i32 4
  mutate p[0] to 10
  let q = borrow p
  print q[0]
  free p
  return 0
end

proc match_example val i64 -> i32:
  match val:
    case 0: print "zero"
    case 1: print "one"
    default: print "other"
  end
  return 0
end

proc switch_example x i64 -> i32:
  switch x:
    case 0: print "zero"
    case 1: print "one"
    default: print "other"
  end
  return 0
end

proc decide_example x i64 -> i32:
  decide x:
    case 1: print "one"
    case 2: print "two"
    default: print "other"
  end
  return 0
end

proc loop_example -> i64:
  let i i64 = 0
  while i < 10:
    print i
    i = i + 1
  end
  return i
end

proc for_example -> i64:
  for let j i64 = 0; j < 5; j = j + 1:
    print j
  end
  return 0
end

proc loop_unroll_example -> i64:
  #pragma unroll 4
  for let i i64 = 0; i < 16; i = i + 1:
    print i
  end
  return 0
end

proc tail_recursive n i64 acc i64 -> i64:
  if n == 0:
    return acc
  end
  return tail_recursive n - 1 acc + n
end

#pragma profile "hot"
inline proc hot_add a i64 b i64 -> i64:
  return a + b
end

pragma "optimize" "speed"
pragma "lto" "on"
pragma "scheduling" "fair"
define BUILD_ID 0xDEADBEEF

proc mmio_demo -> u32:
  let x u32 = 0
  read32 REG 0 into x
  write32 REG 4 123
  return x
end

proc addr_load_store_demo -> i64:
  let p0 = addr 4096 4 8 16
  let y0 u32 = load u32 addr 4096 0 1 0
  let z0 u32 = store u32 addr 4096 0 1 8 7
  print y0
  print z0
  print p0
  return (y0 as i64) + (z0 as i64)
end

proc operator_coverage -> i64:
  let a i64 = 1
  let b i64 = 2

  let i_dec i64 = 123
  let i_underscore i64 = 1_000_000
  let i_hex i64 = 0xCAFE_BABE
  let i_bin i64 = 0b1010_0101

  let t bool = true
  let f bool = false

  let s0 string = "hello"
  let s1 string = "with \\n escape"
  let n = null

  let u0 i64 = -i_dec
  let u1 bool = not f
  let u2 bool = !f
  let u3 i64 = ~i_dec

  let ar0 = a + b
  let ar1 = a - b
  let ar2 = a * b
  let ar3 = 100 / b
  let ar4 = 100 % b

  let bw0 = a & b
  let bw1 = a | b
  let bw2 = a ^ b
  let bw3 = a xor b

  let sh0 = i_dec shl 2
  let sh1 = i_dec shr 1
  let sh2 = i_dec sar 1
  let sh3 = i_dec << 1
  let sh4 = i_dec >> 1

  let c0 = a < b
  let c1 = a <= b
  let c2 = a > b
  let c3 = a >= b
  let c4 = a == b
  let c5 = a != b

  let c6 = a = b

  let l0 = c0 and c5
  let l1 = c0 or c4
  let l2 = c0 && c5
  let l3 = c0 || c4

  let te0 = c0 ? a : b
  let te1 = (a < b) ? (a + 1) : (b + 1)

  print s0
  print s1
  print t
  print f
  print n

  return ar0 + ar1 + ar2 + ar3 + ar4 +
         bw0 + bw1 + bw2 + bw3 +
         sh0 + sh1 + sh2 + sh3 + sh4 +
         u0 + u3 + te0 + te1 +
         i_underscore + i_hex + i_bin
end

proc symbol_demo -> i32:
  let sym0 = #rane_rt_print
  let sym1 = #REG
  print sym0
  print sym1
  return 0
end

module demo_struct

node start:
  set h Header to Header:
    magic 0x52414E45
    version 1
    flags 0
    size 4096
  end

  set m u32 to h.magic
  set h.version to 2
  add h.size by 512

  say "ok"
  go to node end_node
end

node end_node:
  say "goodbye"
  halt
end

start at node start

proc main -> int requires heap_alloc threads channels file_io network_io syscalls dynamic_eval crypto:
  let hdr Header = Header:
    magic 0x52414E45
    version 1
    flags 0
    size 4096
  end

  let p Point = Point: x 7 y 9 end
  let v Vec3  = Vec3: x 1 y 2 z 3 end

  print hdr.magic
  print p.x
  print v.z

  let f Flags = Flags.Read | Flags.Write
  if (f & Flags.Write):
    print "writable"
  end

  let c Color = Color.Green
  print c

  let m1 Maybe<i64> = Some 123
  let m2 Maybe<i64> = None

  match m1:
    case Some x: print x
    case None:   print "none"
  end

  match m2:
    case Some x: print x
    case None:   print "none"
  end

  let u IntOrFloat
  set u.i to 10
  print u.i
  set u.f to 3.14
  print u.f

  let sum i64 = add5 1 2 3 4 5
  let idv i64 = identity<i64> sum
  print sum
  print idv

  print choose_demo 9 2

  print mmio_demo
  print addr_load_store_demo

  print collection_demo
  print ownership_example

  print loop_example
  print for_example
  print loop_unroll_example
  print tail_recursive 10 0

  print match_example 1
  print switch_example 2
  print decide_example 3

  let ax i64 = assert_example 5
  print ax

  let text string = file_read_example "file.txt"
  print text
  print defer_example "out.txt"

  let netv i64 = await async_fetch
  print netv

  let th1 = spawn spawn_worker 100000
  let th2 = spawn spawn_worker 200000

  send ch 11
  send ch 22
  let r1 i64 = recv ch
  let r2 i64 = recv ch

  let a1 i64 = join th1
  let a2 i64 = join th2

  print r1 + r2 + a1 + a2

  lock m1:
    print "locked"
  end

  print asm_example
  print try_example
  print eval_example "3"

  print operator_coverage
  print symbol_demo

  goto (1 == 1) -> L_true L_false

label L_false:
  trap 7
  goto 1 -> L_end L_end

label L_true:
  trap

label L_end:
  halt

  return 0
end
*****

Typed Common Intermediary Language (Typed CIL):

import rane_rt_print;
import rane_rt_fs;
import rane_rt_net;
import rane_rt_time;
import rane_rt_threads;
import rane_rt_channels;
import rane_rt_alloc;
import rane_rt_crypto;

module demo_root;

namespace math {
  export inline proc square(x: i64) -> i64 { return x * x; }

  export inline proc abs_i64(x: i64) -> i64 {
    if (x < 0) { return -x; }
    else { return x; }
  }

  private proc hidden() -> i32 { return 0; }
}

import math::square;
import math::abs_i64;

public proc exported_fn() -> i32 { return 7; }

type i8;  type i16;  type i32;  type i64;  type i128;  type i512;
type u8;  type u16;  type u32;  type u64;  type u128;  type u512;
type f32; type f64;  type f128;
type bool; type void; type int; type string;

typealias word = u32;
alias int32 = i32;

const PI: f64 = 3.141592653589793;
constexpr E: f64 = 2.718281828459045;
constinit ZERO: i64 = 0;

consteval proc const_fn() -> i64 { return 42; }

@derive(Eq, Ord, Debug)
struct Person:
  name: string
  age: u8
end

enum Flags : u8 {
  None = 0,
  Read = 1,
  Write = 2,
  Exec = 4,
  ReadWrite = Read | Write
}

enum Color : i32 {
  Red = 0,
  Green = 1,
  Blue = 2
}

variant Maybe<T> = Some(T) | None

union IntOrFloat {
  i: i32
  f: f32
}

struct Header:
  magic: u32
  version: u16
  flags: u16
  size: u64
end

struct Point:
  x: i32
  y: i32
end

struct Vec3:
  x: i64
  y: i64
  z: i64
end

mmio region REG from 4096 size 256;

capability(heap_alloc);
capability(file_io);
capability(network_io);
capability(dynamic_eval);
capability(syscalls);
capability(threads);
capability(channels);
capability(crypto);

admin proc admin_fn() -> i32 { return 0; }
protected proc prot_fn() -> i32 { return 1; }
public proc pub_fn() -> i32 { return 2; }
private proc priv_fn() -> i32 { return 3; }

contract positive(x: i64) { ensures(x > 0); }

proc assert_example(x: i64) -> i64 {
  assert(x != 0, "x must be non-zero");
  return x;
}

macro SQUARE(x) = (x) * (x)

template <T>
proc generic_id(x: T) -> T { return x; }

mutex m1;
channel<int> ch;

async proc async_fetch() -> i64 requires(network_io) {
  let v: i64 = await rane_rt_net.fetch_i64("https://example");
  return v;
}

dedicate proc spawn_worker(iter: i64) -> i64 requires(threads) {
  let total: i64 = 0;
  for let i: i64 = 0; i < iter; i = i + 1 {
    total = total + i;
  }
  return total;
}

proc file_read_example(path: string) -> string requires(file_io) {
  let f = open(path);
  try {
    let s: string = f.read();
    return s;
  } finally {
    close(f);
  }
}

proc defer_example(path: string) -> i32 requires(file_io) {
  let f = open(path);
  try {
    write(f, "hello");
    return 0;
  } finally {
    close(f);
  }
}

proc asm_example() -> i64 requires(syscalls) {
  let out: i64 = 0;
  asm {
    mov rax, 1
    add rax, 2
    mov out, rax
  }
  return out;
}

proc try_example() -> i32 {
  try {
    throw 100;
  } catch (e) {
    print(e);
  } finally {
    print("done");
  }
  return 0;
}

proc eval_example(x: string) -> i64 requires(dynamic_eval) {
  let res: i64 = eval("10 + " + x);
  print(res);
  return res;
}

proc add5(a: i64, b: i64, c: i64, d: i64, e: i64) -> i64 {
  return a + b + c + d + e;
}

proc identity<T>(x: T) -> T { return x; }

proc choose_demo(a: i64, b: i64) -> i64 {
  let mx: i64 = rane_rt_math.max_i64(a, b);
  let mn: i64 = rane_rt_math.min_i64(a, b);
  return mx + mn;
}

proc collection_demo() -> i64 requires(heap_alloc) {
  let arr: [5]i64 = [1, 2, 3, 4, 5];
  let vec = vector(1, 2, 3);
  let table = map("a" -> 1, "b" -> 2);

  let tup = (1, "hi", true);
  let (x0: i64, x1: string, x2: bool) = tup;

  print(arr[0]);
  print(vec.len());
  print(table.get("a"));
  print(x1);

  return x0 + table.get("b");
}

linear proc lin_inc(x: i64) -> i64 { return x + 1; }
nonlinear proc nlin_mul(x: i64) -> i64 { return x * 2; }

proc ownership_example() -> i32 requires(heap_alloc) {
  let p = allocate(i32, 4);
  mutate p[0] to 10;
  let q = borrow p;
  print(q[0]);
  free p;
  return 0;
}

proc match_example(val: i64) -> i32 {
  switch val {
    case 0: print("zero");
    case 1: print("one");
    default: print("other");
  }
  return 0;
}

proc switch_example(x: i64) -> i32 {
  switch x {
    case 0: print("zero");
    case 1: print("one");
    default: print("other");
  }
  return 0;
}

proc decide_example(x: i64) -> i32 {
  decide x {
    case 1: print("one");
    case 2: print("two");
    default: print("other");
  }
  return 0;
}

proc loop_example() -> i64 {
  let i: i64 = 0;
  while i < 10 {
    print(i);
    i = i + 1;
  }
  return i;
}

proc for_example() -> i64 {
  for let j: i64 = 0; j < 5; j = j + 1 {
    print(j);
  }
  return 0;
}

proc loop_unroll_example() -> i64 {
  #pragma unroll(4)
  for let i: i64 = 0; i < 16; i = i + 1 {
    print(i);
  }
  return 0;
}

proc tail_recursive(n: i64, acc: i64) -> i64 {
  if n == 0 { return acc; }
  return tail_recursive(n - 1, acc + n);
}

#pragma profile("hot")
inline proc hot_add(a: i64, b: i64) -> i64 { return a + b; }

pragma("optimize", "speed");
pragma("lto", "on");
pragma("scheduling", "fair");
define BUILD_ID 0xDEADBEEF

proc mmio_demo() -> u32 {
  let x: u32 = 0;
  read32 REG, 0 into x;
  write32 REG, 4, 123;
  return x;
}

proc addr_load_store_demo() -> i64 {
  let p0 = addr(4096, 4, 8, 16);
  let y0: u32 = load(u32, addr(4096, 0, 1, 0));
  let z0: u32 = store(u32, addr(4096, 0, 1, 8), 7);
  print(y0);
  print(z0);
  print(p0);
  return (y0 as i64) + (z0 as i64);
}

proc operator_coverage() -> i64 {
  let a: i64 = 1;
  let b: i64 = 2;

  let i_dec: i64 = 123;
  let i_underscore: i64 = 1_000_000;
  let i_hex: i64 = 0xCAFE_BABE;
  let i_bin: i64 = 0b1010_0101;

  let t: bool = true;
  let f: bool = false;

  let s0: string = "hello";
  let s1: string = "with \\n escape";
  let n = null;

  let u0: i64 = -i_dec;
  let u1: bool = not f;
  let u2: bool = !f;
  let u3: i64 = ~i_dec;

  let ar0 = a + b;
  let ar1 = a - b;
  let ar2 = a * b;
  let ar3 = 100 / b;
  let ar4 = 100 % b;

  let bw0 = a & b;
  let bw1 = a | b;
  let bw2 = a ^ b;
  let bw3 = a xor b;

  let sh0 = i_dec shl 2;
  let sh1 = i_dec shr 1;
  let sh2 = i_dec sar 1;
  let sh3 = i_dec << 1;
  let sh4 = i_dec >> 1;

  let c0 = a < b;
  let c1 = a <= b;
  let c2 = a > b;
  let c3 = a >= b;
  let c4 = a == b;
  let c5 = a != b;

  let c6 = a = b;

  let l0 = c0 and c5;
  let l1 = c0 or c4;
  let l2 = c0 && c5;
  let l3 = c0 || c4;

  let te0 = c0 ? a : b;
  let te1 = (a < b) ? (a + 1) : (b + 1);

  print(s0);
  print(s1);
  print(t);
  print(f);
  print(n);

  return ar0 + ar1 + ar2 + ar3 + ar4 + bw0 + bw1 + bw2 + bw3
       + sh0 + sh1 + sh2 + sh3 + sh4 + u0 + u3 + te0 + te1
       + i_underscore + i_hex + i_bin;
}

proc symbol_demo() -> i32 {
  let sym0 = #rane_rt_print;
  let sym1 = #REG;
  print(sym0);
  print(sym1);
  return 0;
}

module demo_struct;

node start:
  set h: Header to Header{
    magic: 0x52414E45
    version: 1
    flags: 0
    size: 4096
  }
  set m: u32 to h.magic
  set h.version to 2
  add h.size by 512
  say "ok"
  go to node end_node
end

node end_node:
  say "goodbye"
  halt
end

start at node start

proc main() -> int requires(heap_alloc, threads, channels, file_io, network_io, syscalls, dynamic_eval, crypto) {

  let hdr: Header = Header{
    magic: 0x52414E45
    version: 1
    flags: 0
    size: 4096
  };

  let p: Point = Point{ x: 7, y: 9 };
  let v: Vec3 = Vec3{ x: 1, y: 2, z: 3 };

  print(hdr.magic);
  print(p.x);
  print(v.z);

  let f: Flags = Flags.Read | Flags.Write;
  if (f & Flags.Write) { print("writable"); }

  let c: Color = Color.Green;
  print(c);

  let m1: Maybe<i64> = Some(123);
  let m2: Maybe<i64> = None;

  match m1 {
    case Some(x): print(x);
    case None: print("none");
  }
  match m2 {
    case Some(x): print(x);
    case None: print("none");
  }

  let u: IntOrFloat;
  u.i = 10;
  print(u.i);
  u.f = 3.14;
  print(u.f);

  let sum: i64 = add5(1, 2, 3, 4, 5);
  let idv: i64 = identity<i64>(sum);
  print(sum);
  print(idv);

  print(choose_demo(9, 2));

  print(mmio_demo());
  print(addr_load_store_demo());

  print(collection_demo());
  print(ownership_example());

  print(loop_example());
  print(for_example());
  print(loop_unroll_example());
  print(tail_recursive(10, 0));

  print(match_example(1));
  print(switch_example(2));
  print(decide_example(3));

  let ax: i64 = assert_example(5);
  print(ax);

  let text: string = file_read_example("file.txt");
  print(text);
  print(defer_example("out.txt"));

  let netv: i64 = await async_fetch();
  print(netv);

  let th1 = rane_rt_threads.spawn_proc(spawn_worker, 100000);
  let th2 = rane_rt_threads.spawn_proc(spawn_worker, 200000);

  send(ch, 11);
  send(ch, 22);
  let r1: i64 = recv(ch);
  let r2: i64 = recv(ch);

  let a1: i64 = rane_rt_threads.join_i64(th1);
  let a2: i64 = rane_rt_threads.join_i64(th2);

  print(r1 + r2 + a1 + a2);

  rane_rt_threads.mutex_lock(m1);
  try {
    print("locked");
  } finally {
    rane_rt_threads.mutex_unlock(m1);
  }

  print(asm_example());
  print(try_example());
  print(eval_example("3"));

  print(operator_coverage());
  print(symbol_demo());

  goto (1 == 1) -> L_true, L_false;

label L_false;
  trap 7;
  goto 1 -> L_end, L_end;

label L_true;
  trap;

label L_end;
  halt;

  return 0;
}
*****

from there it gets converted to Windows x86-64 PE executable machine code (.exe)


flowchart LR
    A[source] --> B[Syntax Map Diagram (SMD)]
    B --> C[Typed Common Intermediary Language (Typed CIL)]
    C --> D[Optimized Structure Web]
    D --> E[Codegen (.exe x64 PE)]
    F[CIAMs (Contextual Inference Abstraction Macros)] -.-> B
    F -.-> C
    F -.-> D
    F -.-> E

*****

I’m building a whole civilization here, not just a compiler. Here is the spine. 

---

## 1. Formal language specification (RANE)

### 1.1 Lexical structure
- **Characters:** Unicode source, ASCII subset required for core syntax.
- **Tokens:**  
  **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*`  
  **Literals:** integers (dec/hex/bin with `_`), floats, strings with escapes, `true`, `false`, `null`.  
  **Keywords:** `module`, `namespace`, `proc`, `struct`, `enum`, `variant`, `union`, `type`, `typealias`, `alias`, `const`, `constexpr`, `constinit`, `contract`, `requires`, `async`, `dedicate`, `linear`, `nonlinear`, `match`, `switch`, `decide`, `while`, `for`, `if`, `else`, `return`, `try`, `catch`, `finally`, `throw`, `with`, `defer`, `mutex`, `channel`, `mmio`, `capability`, `admin`, `protected`, `public`, `private`, `goto`, `label`, `trap`, `halt`, `start`, `node`, `say`.
  **Operators:** `+ - * / % & | ^ xor shl shr sar << >> < <= > >= == != = && || and or ? : ! ~ not`.

### 1.2 Declarations
- **Modules/namespaces:** hierarchical, single `module` per file; `namespace` for logical grouping.
- **Types:** primitive (`i8`…`u512`, `f32`…`f128`, `bool`, `void`, `int`, `string`), user types (`struct`, `enum`, `variant`, `union`), aliases (`typealias`, `alias`).
- **Functions:**  
  `proc name [generic] params -> ret [qualifiers] [requires capabilities]: body end`  
  Qualifiers: `inline`, `async`, `dedicate`, `linear`, `nonlinear`, visibility (`public`, `protected`, `private`, `admin`).
- **Capabilities:** `capability name` defines a capability symbol; `requires` on procs expresses capability preconditions.
- **Contracts:** `contract name(params): ensures predicate end` attachable to functions/values.

### 1.3 Statements and expressions
- **Statements:** `let`, `return`, `if/else`, `while`, `for`, `match`, `switch`, `decide`, `try/catch/finally`, `throw`, `with`, `defer`, `asm`, `goto`, `label`, `trap`, `halt`, `lock`.
- **Expressions:** arithmetic, logical, ternary, function calls, method-like calls (`vec.len`), indexing, tuple construction/destructuring, variant construction (`Some 123`), struct literals (`Point: x 7 y 9 end`), casts (`as`).
- **Ownership:** `allocate`, `free`, `borrow`, `mutate` are primitive operations with defined aliasing rules.

### 1.4 Semantics (high-level)
- **Determinism:** no hidden global state; all side effects explicit via capabilities and runtime calls.
- **Evaluation order:** left-to-right for expressions; no unspecified order.
- **Error model:** `trap` for unrecoverable errors; `throw`/`try` for recoverable ones.
- **Concurrency:** `async` + `await`, `threads`, `channels`, `mutex` with well-defined blocking semantics.

---

## 2. SMD node taxonomy

Think: “fully desugared syntax tree, still source-shaped.”

### 2.1 Top-level nodes
- **Module:** `Module(name, imports, decls)`
- **Import:** `Import(path, alias?, symbols?)`
- **Namespace:** `Namespace(name, decls)`
- **CapabilityDecl, TypeDecl, AliasDecl, ConstDecl, ContractDecl`

### 2.2 Type-level nodes
- **StructDecl:** `Struct(name, attrs, fields, derives)`
- **EnumDecl:** `Enum(name, reprType?, variants)`
- **VariantDecl:** `Variant(name, typeParams, cases)`
- **UnionDecl:** `Union(name, fields)`
- **TypeExpr:** `NamedType`, `GenericType(name, args)`, `ArrayType(elem, size)`, `TupleType(elems)`, `FunctionType(params, ret)`

### 2.3 Function-level nodes
- **ProcDecl:**  
  `Proc(name, vis, attrs, qualifiers, typeParams, params, retType, requiresCaps, body)`
- **Param:** `Param(name, type, mode)` (by value, borrow, etc.)
- **Block:** `Block(stmts)`

### 2.4 Statement nodes
- **LetStmt:** `Let(name, type?, initExpr?)`
- **AssignStmt:** `Assign(target, expr)`
- **ReturnStmt:** `Return(expr?)`
- **IfStmt:** `If(cond, thenBlock, elseBlock?)`
- **WhileStmt, ForStmt**
- **MatchStmt:** `Match(expr, cases, default?)`
- **SwitchStmt, DecideStmt** (distinct node kinds, lowered similarly) 
- **TryStmt:** `Try(tryBlock, catchVar?, catchBlock?, finallyBlock?)`
- **WithStmt:** `With(resourceExpr, name, body)` (desugars to try/finally)
- **DeferStmt:** `Defer(expr)` (desugars to finally)
- **AsmStmt:** `Asm(instrs)`
- **GotoStmt, LabelStmt, TrapStmt, HaltStmt**
- **LockStmt:** `Lock(mutexExpr, body)`

### 2.5 Expression nodes
- **LiteralExpr, VarExpr, FieldExpr, IndexExpr, CallExpr, TupleExpr, TupleDestructureExpr**
- **UnaryExpr, BinaryExpr, TernaryExpr**
- **CastExpr:** `Cast(expr, type)`
- **StructInitExpr:** `StructInit(type, fields)`
- **VariantInitExpr:** `VariantInit(variant, payload?)`
- **IntrinsicExpr:** `AllocateExpr`, `FreeExpr`, `BorrowExpr`, `MutateExpr`, `AddrExpr`, `LoadExpr`, `StoreExpr`, `MMIOReadExpr`, `MMIOWriteExpr`
- **AwaitExpr, SpawnExpr, SendExpr, RecvExpr**

Macros and templates are *not* SMD nodes—they’re expanded before or during SMD construction.

---

## 3. Typed CIL type system

### 3.1 Kinds and types
- **Kinds:** `*` (value types), `* -> *` (unary generics), etc.
- **Types:**
  - **Primitives:** `iN`, `uN`, `fN`, `bool`, `void`, `string`.
  - **Structs:** `Struct(name, fields, layoutInfo)`.
  - **Enums:** `Enum(name, reprType, variants)`.
  - **Variants:** lowered to tagged unions: `(tag: reprType, payload: union{...})`.
  - **Unions:** `Union(name, fields, maxSize, maxAlign)`.
  - **Arrays:** `[N]T`.
  - **Tuples:** `(T1, T2, ...)`.
  - **Pointers/Refs:** `&T` (borrow), `*T` (raw).
  - **Function types:** `(T1, ..., Tn) -> Tr` with effect/capability set `E`.
  - **Channels, Mutexes, Threads:** opaque runtime types with parameterization where needed (e.g. `Channel<T>`).

### 3.2 Capabilities and effects
- **Effect set:** attached to function type: `fn(T1..Tn) -> Tr [E]`.
- **Capabilities:** elements of `E` (e.g. `heap_alloc`, `file_io`).
- **Typing rule:** call `f` only if caller’s effect set ⊇ callee’s effect set.

### 3.3 Ownership and linearity
- **Linear types:** certain resources (e.g. `Owned<T>`) must be used exactly once; enforced at CIL level.
- **Borrowing:** `&T` non-owning; lifetime constraints enforced structurally (no use-after-free in CIL).

### 3.4 Well-typedness
- **Progress/preservation:**  
  - No uninitialized reads.  
  - No capability-violating calls.  
  - No type-unsafe casts except via explicit `unsafe` (if you introduce it later).  

---

## 4. OSW optimization passes

Think of OSW as a graph of passes over Typed CIL.

### 4.1 Early passes
- **Constant folding & propagation:** arithmetic, boolean, simple control flow.
- **Dead code elimination:** unreachable blocks, unused variables.
- **Inliner:** guided by `inline`, `#pragma profile("hot")`, size heuristics.

### 4.2 Structural passes
- **Pattern match lowering:** `match`, `switch`, `decide` → decision trees → jump tables/if-chains.
- **Async lowering:** `async/await` → state machines (struct + dispatch loop).
- **Defer/with lowering:** `defer`, `with` → explicit `try/finally` blocks.
- **Tail recursion elimination:** `tail_recursive` → loops where safe.

### 4.3 Memory and ownership passes
- **Escape analysis:** stack vs heap allocation decisions.
- **Borrow/alias analysis:** remove redundant borrows, optimize loads/stores.
- **Linear usage check:** verify linear resources are consumed exactly once.

### 4.4 Low-level passes
- **MMIO lowering:** `read32/write32` → volatile loads/stores with barriers.
- **Addr/load/store lowering:** compute concrete addresses, fold constant offsets.
- **Loop optimizations:** unrolling (`#pragma unroll`), strength reduction, induction variable simplification.

### 4.5 Backend prep
- **SSA construction (if you choose SSA):** for easier register allocation.
- **Register allocation:** graph coloring or linear scan.
- **Instruction selection:** map CIL ops to x86‑64 instructions.

---

## 5. Runtime ABI design

### 5.1 Calling convention
- **Platform:** Windows x86‑64 (MSVC ABI).
- **Registers:**  
  - Args: `RCX, RDX, R8, R9` (then stack).  
  - Return: `RAX` (and `RDX` for large values if needed).  
  - Caller/callee-saved per ABI.
- **Stack:** 16-byte aligned at call sites; shadow space honored.

### 5.2 Type layout
- **Primitives:** follow C ABI sizes/alignments.
- **Structs:** C-like layout: fields in order, padding for alignment.
- **Enums/variants:** tag + payload; tag size = reprType; payload union aligned to max field.
- **Tuples:** treated as anonymous structs.
- **Generics:** monomorphized at compile time (per type instantiation).

### 5.3 Runtime services
- **Threads:** `rane_rt_threads.spawn_proc`, `join_*`, `mutex_lock/unlock`.
- **Channels:** `send`, `recv` with blocking semantics.
- **Alloc:** `rane_rt_alloc` for heap; all `allocate/free` calls go through it.
- **IO:** `rane_rt_fs`, `rane_rt_net`, `rane_rt_crypto`, `rane_rt_time` as capability-gated APIs.

---

## 6. Compiler architecture diagram (textual)

```text
[Lexer] -> [Parser] -> [SMD Builder] -> [Macro/Template Expander]
      -> [Name Resolver] -> [Type Checker] -> [Typed CIL Builder]
      -> [OSW Optimizer Pipeline]
      -> [Backend: SSA + RegAlloc + Instruction Selection]
      -> [PE Emitter (.exe x64)]
```

Cross-cutting:
- **CIAMs (Contextual Inference Abstraction Macros):**
  - Hook into SMD building (syntactic sugar, context-aware rewrites).
  - Hook into Typed CIL (type-directed macros, effect inference).
  - Hook into OSW (optimization hints, profile-guided rewrites).
  - Hook into Codegen (layout hints, section placement).

---

## 7. Reference manual skeleton

1. **Introduction**
   - Goals, philosophy, determinism, capability model.
2. **Lexical Structure**
3. **Types**
   - Primitives, structs, enums, variants, unions, generics.
4. **Expressions**
5. **Statements**
6. **Functions and Procedures**
   - Visibility, qualifiers, async, linearity.
7. **Modules, Namespaces, Imports**
8. **Capabilities and Effects**
9. **Memory and Ownership**
10. **Concurrency**
    - Threads, async, channels, mutexes.
11. **Low-Level Features**
    - MMIO, addr/load/store, inline asm.
12. **Attributes, Pragmas, Macros, Templates**
13. **Contracts and Assertions**
14. **Runtime Library Overview**
15. **Compilation Pipeline and Tooling**

Each section: formal grammar + examples + lowering notes.

---

## 8. Test suite design

### 8.1 Layers
- **Lexing/parsing tests:** golden files for tokens and SMD trees.
- **Type system tests:** success/failure cases for generics, variants, ownership, capabilities.
- **Lowering tests:** SMD → CIL snapshots; CIL → OSW snapshots.
- **Runtime tests:** threads, channels, IO, MMIO (with mocks).
- **Optimization tests:** ensure passes preserve semantics; check specific transformations.
- **ABI tests:** interop with C; call RANE from C and vice versa.

### 8.2 Strategy
- **Doctests:** examples in the reference manual are executable tests.
- **Property tests:** e.g. `identity<T>` round-trips, `lin_inc` respects linearity.
- **Fuzzing:** random programs within a safe subset; check determinism and no crashes.

---

## 9. Industrial-production framing

You can position RANE as:

- **Deterministic Systems Language:** “What if C, Rust, and Zig were rebuilt around explicit capabilities and analyzable IR?”
- **Audit-First Compiler Stack:** every stage is inspectable: SMD, Typed CIL, OSW graph, final PE.
- **Capability-Secured Runtime:** IO, threads, crypto, eval—all gated and statically checked.
- **IR-Centric Platform:** SMD + Typed CIL + OSW are stable targets for tools, analyzers, and alternative backends.

Taglines: 
- **“Deterministic by design, explicit by default.”**
- **“Every effect is a contract.”**
- **“From syntax to silicon, all accounted for.”**

---

