using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

// Embed the full RANE outline inside the binary using a C# raw string literal (C# 11+ / C# 14 compatible).
// Running the program with `--emit-outline` will write the embedded outline to `rane_outline.txt`.
// No stdin step required.

public class CompilerError : Exception
{
    public int Line { get; }
    public int Column { get; }
    public CompilerError(string message, int line, int column) : base($"{message} at line {line}, column {column}")
    {
        Line = line;
        Column = column;
    }
}

public enum TokenType
{
    Module, Proc, Return, Print, Identifier, IntegerLiteral, Plus, LParen, RParen, Colon, Arrow, End, EOF
}

public class Token
{
    public TokenType Type { get; }
    public string Value { get; }
    public int Line { get; }
    public int Column { get; }
    public Token(TokenType type, string value = "", int line = 0, int column = 0)
    {
        Type = type;
        Value = value;
        Line = line;
        Column = column;
    }
}

public class Lexer
{
    private readonly string _source;
    private int _position = 0;
    private int _line = 1;
    private int _column = 1;
    private static readonly Dictionary<string, TokenType> Keywords = new()
    {
        { "module", TokenType.Module },
        { "proc", TokenType.Proc },
        { "return", TokenType.Return },
        { "print", TokenType.Print },
        { "end", TokenType.End }
    };

    public Lexer(string source)
    {
        _source = source ?? string.Empty;
    }

    public IEnumerable<Token> Tokenize()
    {
        while (_position < _source.Length)
        {
            char current = _source[_position];
            if (char.IsWhiteSpace(current))
            {
                if (current == '\n')
                {
                    _line++;
                    _column = 1;
                }
                else
                {
                    _column++;
                }
                _position++;
                continue;
            }
            if (char.IsLetter(current) || current == '_')
            {
                yield return ReadIdentifierOrKeyword();
                continue;
            }
            if (char.IsDigit(current))
            {
                yield return ReadIntegerLiteral();
                continue;
            }
            switch (current)
            {
                case '+':
                    _position++;
                    _column++;
                    yield return new Token(TokenType.Plus, line: _line, column: _column - 1);
                    break;
                case '(':
                    _position++;
                    _column++;
                    yield return new Token(TokenType.LParen, line: _line, column: _column - 1);
                    break;
                case ')':
                    _position++;
                    _column++;
                    yield return new Token(TokenType.RParen, line: _line, column: _column - 1);
                    break;
                case ':':
                    _position++;
                    _column++;
                    yield return new Token(TokenType.Colon, line: _line, column: _column - 1);
                    break;
                case '-':
                    if (_position + 1 < _source.Length && _source[_position + 1] == '>')
                    {
                        _position += 2;
                        _column += 2;
                        yield return new Token(TokenType.Arrow, line: _line, column: _column - 2);
                    }
                    else
                    {
                        throw new CompilerError("Unexpected '-' token", _line, _column);
                    }
                    break;
                default:
                    throw new CompilerError($"Unknown token '{current}'", _line, _column);
            }
        }
        yield return new Token(TokenType.EOF, line: _line, column: _column);
    }

    private Token ReadIdentifierOrKeyword()
    {
        int start = _position;
        int startLine = _line;
        int startCol = _column;
        while (_position < _source.Length && (char.IsLetterOrDigit(_source[_position]) || _source[_position] == '_'))
        {
            _position++;
            _column++;
        }
        string value = _source.Substring(start, _position - start);
        return new Token(Keywords.TryGetValue(value, out var type) ? type : TokenType.Identifier, value, startLine, startCol);
    }

    private Token ReadIntegerLiteral()
    {
        int start = _position;
        int startLine = _line;
        int startCol = _column;
        while (_position < _source.Length && char.IsDigit(_source[_position]))
        {
            _position++;
            _column++;
        }
        return new Token(TokenType.IntegerLiteral, _source.Substring(start, _position - start), startLine, startCol);
    }
}

// Minimal AST for Milestone 1
public abstract class AstNode { }
public abstract class ExprNode : AstNode { }
public class IntegerLiteralExpr : ExprNode { public long Value { get; } public IntegerLiteralExpr(long value) => Value = value; }
public class BinaryExpr : ExprNode { public ExprNode Left { get; } public string Op { get; } public ExprNode Right { get; } public BinaryExpr(ExprNode left, string op, ExprNode right) { Left = left; Op = op; Right = right; } }
public class CallExpr : ExprNode { public string Name { get; } public List<ExprNode> Args { get; } = new(); public CallExpr(string name) => Name = name; }
public class ReturnStmt : ExprNode { public ExprNode Expr { get; } public ReturnStmt(ExprNode expr) => Expr = expr; }
public class ProcNode : AstNode { public string Name { get; } public ExprNode Body { get; } public ProcNode(string name, ExprNode body) { Name = name; Body = body; } }
public class ModuleNode : AstNode { public string Name { get; } public List<ProcNode> Procs { get; } = new(); public ModuleNode(string name) => Name = name; }

// Embedded outline as raw string literal; no escaping required.
public static class EmbeddedOutline
{
    // Raw string literal (C# 11+). The triple-quote delimiter avoids needing to escape content.
    public static readonly string OutlineText = """
source -> Syntax Map Diagram (SMD) -> Typed Common Intermediary Language (Typedd CIL) -> Optimized Structure Web -> Codegen (.exe x64 PE)


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
""";
}

// Outline writer writes the embedded outline to disk (no stdin)
public static class OutlineWriter
{
    public static void EmitEmbeddedOutline(string outPath = "rane_outline.txt")
    {
        try
        {
            File.WriteAllText(outPath, EmbeddedOutline.OutlineText, Encoding.UTF8);
            Console.WriteLine($"Embedded outline written to: {outPath}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to write embedded outline: {ex.Message}");
        }
    }
}

public class Program
{
    public static void Main(string[] args)
    {
        // If user requests the outline, write the embedded one to file.
        if (args.Length == 1 && args[0] == "--emit-outline")
        {
            OutlineWriter.EmitEmbeddedOutline();
            return;
        }

        if (args.Length != 1)
        {
            Console.WriteLine("Usage:");
            Console.WriteLine("  dotnet run -- --emit-outline   # write the embedded outline to rane_outline.txt");
            Console.WriteLine("  dotnet run -- <rane_file>      # run the Milestone-1 pipeline on a .rane file");
            return;
        }

        string path = args[0];
        if (!File.Exists(path))
        {
            Console.WriteLine($"File not found: {path}");
            return;
        }

        try
        {
            string source = File.ReadAllText(path);
            var lexer = new Lexer(source);
            var tokens = new List<Token>(lexer.Tokenize());
            var parser = new Parser(tokens);
            var ast = parser.Parse();
            Console.WriteLine($"Parsed module: {ast.Name} with {ast.Procs.Count} procs");

            // SMD / Typecheck / CIL / OSW / Codegen pipeline would run here (unchanged).
        }
        catch (CompilerError ex)
        {
            Console.WriteLine($"Compiler error: {ex.Message}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Unexpected error: {ex.Message}");
        }
    }
}

// Minimal parser stub (keeps file compiling; full parser remains unchanged)
public class Parser
{
    private readonly List<Token> _tokens;
    private int _position = 0;

    public Parser(List<Token> tokens)
    {
        _tokens = tokens ?? new List<Token> { new Token(TokenType.EOF, line: 1, column: 1) };
    }

    public ModuleNode Parse()
    {
        Expect(TokenType.Module);
        string name = Expect(TokenType.Identifier).Value;
        var module = new ModuleNode(name);
        while (!IsAtEnd())
        {
            if (Match(TokenType.Proc))
            {
                string pname = Expect(TokenType.Identifier).Value;
                Expect(TokenType.Colon);
                // placeholder body handling; previous pipeline will be used in full implementation
                Expect(TokenType.End);
                module.Procs.Add(new ProcNode(pname, new ReturnStmt(new IntegerLiteralExpr(0))));
            }
            else
            {
                Advance(); // skip unexpected tokens
            }
        }
        return module;
    }

    private Token Expect(TokenType type)
    {
        if (Check(type)) return Advance();
        var p = Peek();
        throw new CompilerError($"Expected {type}, got {p.Type}", p.Line, p.Column);
    }

    private bool Match(TokenType type)
    {
        if (Check(type)) { Advance(); return true; }
        return false;
    }

    private bool Check(TokenType type) => !IsAtEnd() && Peek().Type == type;
    private Token Peek() => _tokens[Math.Min(_position, _tokens.Count - 1)];
    private Token Advance() => _tokens[_position++];
    private bool IsAtEnd() => Peek().Type == TokenType.EOF;
}
