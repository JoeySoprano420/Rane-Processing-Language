#include "resolver.cpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace rane_syntax {

// ---------------------------------------------------------------------------
// 0) Reserved / tokenized keywords (kept as an in-code coverage list)
// ---------------------------------------------------------------------------
static const std::vector<std::string> k_reserved_keywords = {
  "let","if","then","else","while","do","for","break","continue","return","ret",
  "proc","def","call","import","export","include","exclude","decide","case","default",
  "jump","goto","mark","label","guard","zone","hot","cold","deterministic","repeat","unroll",
  "not","and","or","xor","shl","shr","sar","try","catch","throw","define","ifdef","ifndef",
  "pragma","namespace","enum","struct","class","public","private","protected","static","inline",
  "extern","virtual","const","volatile","constexpr","consteval","constinit","new","del","cast",
  "type","typealias","alias","mut","immutable","mutable","null","match","pattern","lambda",
  "handle","target","splice","split","difference","increment","decrement","dedicate","mutex",
  "ignore","bypass","isolate","separate","join","declaration","compile","score","sys","admin",
  "plot","peak","point","reg","exception","align","mutate","string","literal","linear","nonlinear",
  "primitives","tuples","member","open","close","module","node","start","set","to","add","by",
  "say","go","halt","mmio","region","read32","write32","trap","choose","addr","load","store",
  "print","vector","map","channel","spawn","join","lock","with","using","defer","macro","template",
  "generic","asm","syscall","tailcall","profile","optimize","lto"
};

// Helper: print a small sample of reserved keywords
void print_reserved_sample(std::size_t n = 8) {
  std::cout << "Reserved keywords (sample " << n << "): ";
  for (std::size_t i = 0; i < n && i < k_reserved_keywords.size(); ++i) {
    if (i) std::cout << ", ";
    std::cout << k_reserved_keywords[i];
  }
  std::cout << '\n';
}

// ---------------------------------------------------------------------------
// 1) Imports (simulated) - in C++ these would be includes / linkage.
//    We keep a small wrapper to demonstrate the same intent.
// ---------------------------------------------------------------------------
// (No-op placeholder in C++: the import is represented by the wrapper above.)

// ---------------------------------------------------------------------------
// 2) MMIO region decl + read32/write32
//    Simple fixed-size mmio simulation for demo/testing.
// ---------------------------------------------------------------------------
class MmioRegion {
public:
  MmioRegion(std::uintptr_t base_addr, std::size_t size_bytes)
    : base(base_addr), size(size_bytes) {
    mem.fill(0);
  }

  uint32_t read32(std::size_t offset) const {
    if (offset + sizeof(uint32_t) > size) return 0;
    uint32_t v = 0;
    // little-endian load
    for (std::size_t i = 0; i < 4; ++i)
      v |= static_cast<uint32_t>(mem[offset + i]) << (8 * i);
    return v;
  }

  void write32(std::size_t offset, uint32_t value) {
    if (offset + sizeof(uint32_t) > size) return;
    for (std::size_t i = 0; i < 4; ++i)
      mem[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
  }

  std::uintptr_t base_address() const { return base; }
  std::size_t region_size() const { return size; }

private:
  const std::uintptr_t base;
  const std::size_t size;
  std::array<uint8_t, 4096> mem{}; // backing store (larger than typical region)
};

// ---------------------------------------------------------------------------
// 3) Proc definitions (core surface)
// ---------------------------------------------------------------------------
inline int add5(int a, int b, int c, int d, int e) {
  return a + b + c + d + e;
}

template <typename T>
inline T identity(T x) { return x; }

// ---------------------------------------------------------------------------
// 4) Main proc exercises expressions + statements
//    We provide a function that mirrors the examples and prints results.
// ---------------------------------------------------------------------------
int rane_main_example() {
  // 4.1) let bindings
  int a = 1;
  int b = 2;

  // 4.2) literals
  int i_dec = 123;
  int i_underscore = 1'000'000;
  unsigned int i_hex = 0xCAFE'BABE;
  unsigned int i_bin = 0b1010'0101;

  bool t = true;
  bool f = false;

  std::string s0 = "hello";
  std::string s1 = "with \\n escape";
  std::nullptr_t n = nullptr;

  // 4.3) unary
  int u0 = -i_dec;
  bool u1 = !f;                // 'not f' -> !
  bool u2 = !f;
  int u3 = ~i_dec;

  // 4.4) binary arithmetic / bitwise / shifts
  int ar0 = a + b;
  int ar1 = a - b;
  int ar2 = a * b;
  int ar3 = 100 / b;
  int ar4 = 100 % b;

  int bw0 = a & b;
  int bw1 = a | b;
  int bw2 = a ^ b;
  int bw3 = a ^ b; // 'xor' word form -> ^

  int sh0 = i_dec << 2; // 'shl'
  int sh1 = static_cast<int>(i_dec >> 1); // 'shr'
  int sh2 = static_cast<int>(i_dec >> 1); // 'sar' (arithmetic right shift)
  int sh3 = i_dec << 1;
  int sh4 = static_cast<int>(i_dec >> 1);

  // 4.5) comparisons (bootstrap bool: 0/1)
  bool c0 = a < b;
  bool c1 = a <= b;
  bool c2 = a > b;
  bool c3 = a >= b;
  bool c4 = a == b;
  bool c5 = a != b;

  // v1 compatibility: single '=' treated as equality in expr parsing
  bool c6 = (a == b);

  // 4.6) logical ops (short-circuit)
  bool l0 = c0 && c5;  // 'and'
  bool l1 = c0 || c4;  // 'or'
  bool l2 = c0 && c5;
  bool l3 = c0 || c4;

  // 4.7) ternary
  int te0 = c0 ? a : b;
  int te1 = (a < b) ? (a + 1) : (b + 1);

  // 4.8) choose max/min (use std::max/min)
  int ch0 = std::max(a, b);
  int ch1 = std::min(a, b);

  // 4.9) addr / load / store expression forms (simulated)
  auto addr = [](std::uintptr_t base, std::size_t off, std::size_t idx, std::size_t scale) {
    // produce a simulated pointer-like value (not dereferenced)
    return base + off + idx * scale;
  };
  std::uintptr_t p0 = addr(4096, 4, 8, 16);

  // Removed invalid placeholder instantiation:
  // MmioRegion mmio(REGION_BASE_PLACEHOLDER, 256); // REGION_BASE_PLACEHOLDER undefined

  // 4.10) mmio sugar statements: instantiate region and perform reads/writes
  MmioRegion reg(4096, 256);
  int x = 0;
  reg.write32(4, 123);
  uint32_t r0 = reg.read32(4);
  (void)r0; // silence unused

  // 4.11) ident literal -> represented as string_view or symbol id in C++
  std::string sym0 = "rane_rt_print";
  std::string sym1 = "REG";

  // 4.12) calls (expression calls)
  int sum = add5(1, 2, 3, 4, 5);
  int idv = identity(sum);

  // print is builtin lowered from EXPR_CALL named "print" -> use std::cout
  std::cout << s0 << '\n';
  std::cout << sum << '\n';
  std::cout << idv << '\n';

  // 4.13) statement-form call + goto (we'll simulate simple control flow)
  // simulate call identity(123) into slot 1
  int slot1 = identity(123);
  (void)slot1;

  // conditional goto: emulate with branches
  if (a < b) {
    // L_true
    // trap -> simulate by printing
    std::cout << "trap\n";
  } else {
    // L_false
    std::cout << "trap 7\n";
  }
  // goto L_end -> halt simulated by returning
  std::cout << "halt\n";

  // unreachable; kept for coverage
  return 0;
}

// ---------------------------------------------------------------------------
// 5) v1 node/prose surface + v1 struct surface (parse-only coverage)
// ---------------------------------------------------------------------------

struct Header {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint64_t size;
};

void demo_node_start() {
  Header h{
    0x52414E45u, // magic 'RANE' in ASCII
    1,
    0,
    4096
  };

  uint32_t m = h.magic;
  (void)m;

  h.version = 2;
  h.size += 512;

  std::cout << "ok\n";

  // go to node end_node -> call end function
}

void demo_node_end() {
  std::cout << "goodbye\n";
}

// ---------------------------------------------------------------------------
// 7.2/7.14) Structs, variants, enums and attributes
// ---------------------------------------------------------------------------
struct Point {
  int32_t x;
  int32_t y;
};

struct Person {
  std::string name;
  uint8_t age;
  // @derive(Eq, Ord, Debug) -> rely on default comparisons if needed
};

union IntOrFloat {
  int32_t i;
  float f;
  IntOrFloat() { i = 0; }
  ~IntOrFloat() = default;
};

// Maybe<T> variant using std::optional for simplicity
template <typename T>
using Maybe = std::optional<T>;

// ---------------------------------------------------------------------------
// 8.1) eval: very small runtime evaluator (coverage, extreme-minimal)
// ---------------------------------------------------------------------------
std::optional<int64_t> runtime_eval_arith(std::string_view expr) {
  // Very small evaluator supporting single integer or integer + integer.
  // Examples handled: "1 + 2", "42"
  std::istringstream iss{std::string(expr)}; // avoid most-vexing-parse
  int64_t lhs = 0;
  if (!(iss >> lhs)) return std::nullopt;
  char op = 0;
  if (!(iss >> op)) return std::optional<int64_t>{lhs}; // single value
  int64_t rhs = 0;
  if (!(iss >> rhs)) return std::nullopt;
  switch (op) {
    case '+': return lhs + rhs;
    case '-': return lhs - rhs;
    case '*': return lhs * rhs;
    case '/': if (rhs != 0) return lhs / rhs; return std::nullopt;
    default: return std::nullopt;
  }
}

// ---------------------------------------------------------------------------
// 8.2) enums: typed enums and helper usage
// ---------------------------------------------------------------------------
enum class Flags : uint8_t {
  None = 0,
  Read = 1,
  Write = 2,
  Exec = 4,
  ReadWrite = Read | Write
};

inline Flags operator|(Flags a, Flags b) {
  return static_cast<Flags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline Flags operator&(Flags a, Flags b) {
  return static_cast<Flags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

enum class Color : int32_t {
  Red = 0,
  Green = 1,
  Blue = 2
};

int enum_usage_example() {
  Flags f = Flags::Read | Flags::Write;
  if (static_cast<uint8_t>(f & Flags::Write)) {
    std::cout << "writable\n";
  }
  Color c = Color::Green;
  std::cout << static_cast<int>(c) << '\n';
  return 0;
}

// ---------------------------------------------------------------------------
// Small driver to exercise the above when compiled standalone.
// Call rane_syntax::run_demo() from test harnesses or a temporary main.
// ---------------------------------------------------------------------------
int run_demo() {
  print_reserved_sample();
  rane_main_example();
  demo_node_start();
  demo_node_end();

  if (auto v = runtime_eval_arith("1 + 2")) {
    std::cout << "eval 1 + 2 -> " << *v << '\n';
  }

  enum_usage_example();

  // demonstrate maybe (variant) usage
  Maybe<int> some = 3;
  if (some) std::cout << "maybe: " << *some << '\n';

  return 0;
}

} // namespace rane_syntax

// If this file is compiled standalone for quick testing define RANE_SYNTAX_STANDALONE
#ifdef RANE_SYNTAX_STANDALONE
int main() {
  return rane_syntax::run_demo();
}
#endif
