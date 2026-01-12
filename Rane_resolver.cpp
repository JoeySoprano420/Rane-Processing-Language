// rane_resolver.cpp (C++20)
// Layered AOT Resolver: source -> optimized CIAM expansion -> machine code -> executor
//
// LAYER 1 (done): full expression grammar + deterministic precedence (Pratt)
// LAYER 2 (done): canonical AST + CIAM de-sugaring -> emits syntax.ciam.rane
// LAYER 3 (done-min): minimal CFG-ish IR + calls + print intrinsic
// LAYER 4 (partial but wired): match/spawn/join/lock/with/defer lowering hooks
// LAYER 5 (done-min): syntax.opt.ciam.ir writer (BNF header + stable formatting)
//
// Build (Linux/macOS):
//   g++ -std=c++20 -O2 -Wall -Wextra -pedantic rane_resolver.cpp -o rane_resolver
//
// Build (Windows MSVC Developer Prompt):
//   cl /std:c++20 /O2 /W4 rane_resolver.cpp
//
// Run:
//   ./rane_resolver path/to/program.rane
//
// Minimal supported sugar example:
//   proc main -> int:
//     print 1 + 2 * 3
//     let x i64 = 7
//     print x
//     return 0
//   end
//
// Notes:
// - This is a buildable spine. Extend by adding node kinds + CIAM rules + IR ops + codegen patterns.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <variant>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

#if defined(_WIN32)
  #define NOMINMAX
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
#endif

//------------------------------------------------------------------------------
// Diagnostics + spans
//------------------------------------------------------------------------------

enum class DiagCode : uint32_t {
  Ok = 0,
  LexError,
  ParseError,
  UndefinedName,
  TypeMismatch,
  SecurityViolation,
  InternalError
};

struct Span {
  uint32_t line = 1;
  uint32_t col  = 1;
  uint32_t len  = 0;
};

struct Diag {
  DiagCode code = DiagCode::Ok;
  Span span{};
  std::string message;
};

static void die(const Diag& d) {
  std::cerr << "error: " << (uint32_t)d.code
            << " at " << d.span.line << ":" << d.span.col
            << " len " << d.span.len
            << " : " << d.message << "\n";
  std::exit(1);
}

//------------------------------------------------------------------------------
// Lexical tokens with deterministic ordinals
//------------------------------------------------------------------------------

enum class TokKind : uint16_t {
  Eof = 0,
  Newline,
  Indent,
  Dedent,

  Ident,
  IntLit,
  StringLit,

  // Keywords (subset, grow as needed)
  KwProc,
  KwReturn,
  KwLet,
  KwEnd,

  KwIf,
  KwElse,

  KwMatch,
  KwCase,
  KwDefault,

  KwWith,
  KwDefer,
  KwLock,
  KwSpawn,
  KwJoin,

  KwTry,
  KwCatch,
  KwFinally,
  KwThrow,

  // Punct / operators
  Arrow,     // ->
  Colon,     // :
  Assign,    // =
  LParen,    // (
  RParen,    // )
  LBracket,  // [
  RBracket,  // ]
  Comma,     // ,
  Dot,       // .

  Plus, Minus, Star, Slash, Percent,
  Bang, Tilde,
  AndAnd, OrOr,
  Amp, Pipe, Caret,
  Shl, Shr,
  EqEq, NotEq,
  Lt, Lte, Gt, Gte,
  Question, // ?
};

struct Token {
  TokKind kind{};
  std::string text;
  Span span{};
  uint32_t ordinal = 0;
};

static bool is_ident_start(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static bool is_ident_cont(char c) {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

//------------------------------------------------------------------------------
// Lexer (sugar mode): INDENT/DEDENT from spaces
//------------------------------------------------------------------------------

struct Lexer {
  std::string src;
  size_t i = 0;
  uint32_t line = 1;
  uint32_t col  = 1;
  uint32_t next_ordinal = 1;

  std::vector<int> indent_stack{0};
  bool at_line_start = true;

  explicit Lexer(std::string s) : src(std::move(s)) {}

  char peek() const { return (i < src.size()) ? src[i] : '\0'; }
  char peek2() const { return (i + 1 < src.size()) ? src[i+1] : '\0'; }

  char get() {
    char c = peek();
    if (!c) return c;
    i++;
    if (c == '\n') { line++; col = 1; at_line_start = true; }
    else { col++; at_line_start = false; }
    return c;
  }

  Token make(TokKind k, Span sp, std::string t = {}) {
    Token tok;
    tok.kind = k;
    tok.span = sp;
    tok.text = std::move(t);
    tok.ordinal = next_ordinal++;
    return tok;
  }

  void skip_ws_midline() {
    while (true) {
      char c = peek();
      if (c == ' ' || c == '\r') { get(); continue; }
      if (c == '\t') die({DiagCode::LexError, {line,col,1}, "tabs are not allowed (determinism)"});
      // line comment //
      if (c == '/' && peek2() == '/') {
        while (peek() && peek() != '\n') get();
        continue;
      }
      break;
    }
  }

  std::vector<Token> emit_indent_dedent() {
    std::vector<Token> out;
    uint32_t start_line = line, start_col = col;
    int spaces = 0;
    while (peek() == ' ') { get(); spaces++; }
    if (peek() == '\t') die({DiagCode::LexError, {line,col,1}, "tabs are not allowed (determinism)"});

    if (peek() == '\n' || peek() == '\0') return out;

    int current = indent_stack.back();
    if (spaces > current) {
      indent_stack.push_back(spaces);
      out.push_back(make(TokKind::Indent, {start_line, start_col, (uint32_t)spaces}));
    } else if (spaces < current) {
      while (indent_stack.size() > 1 && spaces < indent_stack.back()) {
        indent_stack.pop_back();
        out.push_back(make(TokKind::Dedent, {start_line, start_col, 0}));
      }
      if (spaces != indent_stack.back()) {
        die({DiagCode::LexError, {start_line,start_col,(uint32_t)spaces},
             "indentation does not match any prior level"});
      }
    }
    return out;
  }

  TokKind keyword_kind(const std::string& s) {
    if (s == "proc") return TokKind::KwProc;
    if (s == "return") return TokKind::KwReturn;
    if (s == "let") return TokKind::KwLet;
    if (s == "end") return TokKind::KwEnd;

    if (s == "if") return TokKind::KwIf;
    if (s == "else") return TokKind::KwElse;

    if (s == "match") return TokKind::KwMatch;
    if (s == "case") return TokKind::KwCase;
    if (s == "default") return TokKind::KwDefault;

    if (s == "with") return TokKind::KwWith;
    if (s == "defer") return TokKind::KwDefer;
    if (s == "lock") return TokKind::KwLock;
    if (s == "spawn") return TokKind::KwSpawn;
    if (s == "join") return TokKind::KwJoin;

    if (s == "try") return TokKind::KwTry;
    if (s == "catch") return TokKind::KwCatch;
    if (s == "finally") return TokKind::KwFinally;
    if (s == "throw") return TokKind::KwThrow;

    return TokKind::Ident;
  }

  std::vector<Token> lex_all() {
    std::vector<Token> toks;

    while (true) {
      if (at_line_start) {
        auto idt = emit_indent_dedent();
        toks.insert(toks.end(), idt.begin(), idt.end());
      }

      skip_ws_midline();

      uint32_t start_line = line, start_col = col;
      char c = peek();
      if (!c) break;

      if (c == '\n') {
        get();
        toks.push_back(make(TokKind::Newline, {start_line,start_col,1}, "\\n"));
        continue;
      }

      // two-char ops
      if (c == '-' && peek2() == '>') { get(); get(); toks.push_back(make(TokKind::Arrow, {start_line,start_col,2}, "->")); continue; }
      if (c == '&' && peek2() == '&') { get(); get(); toks.push_back(make(TokKind::AndAnd, {start_line,start_col,2}, "&&")); continue; }
      if (c == '|' && peek2() == '|') { get(); get(); toks.push_back(make(TokKind::OrOr, {start_line,start_col,2}, "||")); continue; }
      if (c == '=' && peek2() == '=') { get(); get(); toks.push_back(make(TokKind::EqEq, {start_line,start_col,2}, "==")); continue; }
      if (c == '!' && peek2() == '=') { get(); get(); toks.push_back(make(TokKind::NotEq, {start_line,start_col,2}, "!=")); continue; }
      if (c == '<' && peek2() == '=') { get(); get(); toks.push_back(make(TokKind::Lte, {start_line,start_col,2}, "<=")); continue; }
      if (c == '>' && peek2() == '=') { get(); get(); toks.push_back(make(TokKind::Gte, {start_line,start_col,2}, ">=")); continue; }
      if (c == '<' && peek2() == '<') { get(); get(); toks.push_back(make(TokKind::Shl, {start_line,start_col,2}, "<<")); continue; }
      if (c == '>' && peek2() == '>') { get(); get(); toks.push_back(make(TokKind::Shr, {start_line,start_col,2}, ">>")); continue; }

      // single-char punct / ops
      auto single = [&](TokKind k, const char* s) { get(); toks.push_back(make(k, {start_line,start_col,1}, s)); };
      switch (c) {
        case ':': single(TokKind::Colon, ":"); continue;
        case '=': single(TokKind::Assign, "="); continue;
        case '(': single(TokKind::LParen, "("); continue;
        case ')': single(TokKind::RParen, ")"); continue;
        case '[': single(TokKind::LBracket, "["); continue;
        case ']': single(TokKind::RBracket, "]"); continue;
        case ',': single(TokKind::Comma, ","); continue;
        case '.': single(TokKind::Dot, "."); continue;

        case '+': single(TokKind::Plus, "+"); continue;
        case '-': single(TokKind::Minus, "-"); continue;
        case '*': single(TokKind::Star, "*"); continue;
        case '/': single(TokKind::Slash, "/"); continue;
        case '%': single(TokKind::Percent, "%"); continue;

        case '!': single(TokKind::Bang, "!"); continue;
        case '~': single(TokKind::Tilde, "~"); continue;

        case '&': single(TokKind::Amp, "&"); continue;
        case '|': single(TokKind::Pipe, "|"); continue;
        case '^': single(TokKind::Caret, "^"); continue;

        case '<': single(TokKind::Lt, "<"); continue;
        case '>': single(TokKind::Gt, ">"); continue;

        case '?': single(TokKind::Question, "?"); continue;
        default: break;
      }

      // String
      if (c == '"') {
        get();
        std::string s;
        while (true) {
          char ch = get();
          if (!ch) die({DiagCode::LexError, {start_line,start_col,1}, "unterminated string"});
          if (ch == '"') break;
          if (ch == '\\') {
            char e = get();
            if (!e) die({DiagCode::LexError, {start_line,start_col,1}, "unterminated escape"});
            switch (e) {
              case 'n': s.push_back('\n'); break;
              case 'r': s.push_back('\r'); break;
              case 't': s.push_back('\t'); break;
              case '\\': s.push_back('\\'); break;
              case '"': s.push_back('"'); break;
              default: die({DiagCode::LexError, {line,col,1}, "unknown escape"});
            }
          } else s.push_back(ch);
        }
        uint32_t len = (uint32_t)(col - start_col);
        toks.push_back(make(TokKind::StringLit, {start_line,start_col,len}, s));
        continue;
      }

      // Int
      if (c >= '0' && c <= '9') {
        std::string s;
        while (true) {
          char ch = peek();
          if (!ch) break;
          if ((ch >= '0' && ch <= '9') || ch == '_') { s.push_back(get()); continue; }
          break;
        }
        toks.push_back(make(TokKind::IntLit, {start_line,start_col,(uint32_t)s.size()}, s));
        continue;
      }

      // Ident/keyword
      if (is_ident_start(c)) {
        std::string s;
        s.push_back(get());
        while (is_ident_cont(peek())) s.push_back(get());
        TokKind k = keyword_kind(s);
        toks.push_back(make(k, {start_line,start_col,(uint32_t)s.size()}, s));
        continue;
      }

      std::string msg = "unexpected character: ";
      msg.push_back(c);
      die({DiagCode::LexError, {start_line,start_col,1}, msg});
    }

    while (indent_stack.size() > 1) {
      indent_stack.pop_back();
      toks.push_back(make(TokKind::Dedent, {line,col,0}, ""));
    }
    toks.push_back(make(TokKind::Eof, {line,col,0}, ""));
    return toks;
  }
};

//------------------------------------------------------------------------------
// AST + canonical AST
//------------------------------------------------------------------------------

using NodeId = uint32_t;

enum class NodeKind : uint16_t {
  Unit = 1,
  ProcDecl,
  Block,

  // statements
  ReturnStmt,
  LetStmt,
  ExprStmt,
  IfStmt,
  SwitchStmt,
  TryFinallyStmt,

  // expressions
  IntExpr,
  StringExpr,
  IdentExpr,
  UnaryExpr,
  BinaryExpr,
  CallExpr,
  MemberExpr,
};

struct NodeHeader {
  NodeKind kind{};
  NodeId id{};
  Span span{};
  uint32_t first_tok = 0;
  uint32_t last_tok  = 0;
};

static Span merge_span(const Token& a, const Token& b) {
  Span s;
  s.line = a.span.line;
  s.col  = a.span.col;
  if (a.span.line == b.span.line) {
    uint32_t endcol = b.span.col + b.span.len;
    s.len = (endcol > s.col) ? (endcol - s.col) : a.span.len;
  } else s.len = a.span.len;
  return s;
}

struct Expr;
struct Stmt;
struct Block;

enum class UnOp : uint8_t { Neg, Not, BitNot };
enum class BinOp : uint8_t {
  Add, Sub, Mul, Div, Mod,
  Shl, Shr,
  Lt, Lte, Gt, Gte,
  Eq, Ne,
  BitAnd, BitXor, BitOr,
  And, Or
};

struct IntExpr { NodeHeader h; int64_t value = 0; };
struct StringExpr { NodeHeader h; std::string value; };
struct IdentExpr { NodeHeader h; std::string name; };
struct UnaryExpr { NodeHeader h; UnOp op; std::unique_ptr<Expr> rhs; };
struct BinaryExpr { NodeHeader h; BinOp op; std::unique_ptr<Expr> lhs; std::unique_ptr<Expr> rhs; };
struct CallExpr { NodeHeader h; std::unique_ptr<Expr> callee; std::vector<Expr> args; };
struct MemberExpr { NodeHeader h; std::unique_ptr<Expr> base; std::string member; };

struct Expr {
  std::variant<IntExpr, StringExpr, IdentExpr, UnaryExpr, BinaryExpr, CallExpr, MemberExpr> v;
  NodeHeader& hdr() { return std::visit([](auto& x)->NodeHeader&{ return x.h; }, v); }
  const NodeHeader& hdr() const { return std::visit([](auto const& x)->const NodeHeader&{ return x.h; }, v); }
};

struct ReturnStmt { NodeHeader h; std::optional<Expr> value; };
struct LetStmt { NodeHeader h; std::string name; std::string type_name; Expr init; };
struct ExprStmt { NodeHeader h; Expr expr; };

struct IfStmt {
  NodeHeader h;
  Expr cond;
  Block* then_blk = nullptr;
  Block* else_blk = nullptr; // optional
};

struct SwitchCase {
  int64_t value = 0;
  Block* body = nullptr;
  Span span{};
};

struct SwitchStmt {
  NodeHeader h;
  Expr scrutinee;
  std::vector<SwitchCase> cases;
  Block* default_blk = nullptr;
};

struct TryFinallyStmt {
  NodeHeader h;
  Block* try_blk = nullptr;
  Block* finally_blk = nullptr;
};

struct Stmt {
  std::variant<ReturnStmt, LetStmt, ExprStmt, IfStmt, SwitchStmt, TryFinallyStmt> v;
  NodeHeader& hdr() { return std::visit([](auto& x)->NodeHeader&{ return x.h; }, v); }
  const NodeHeader& hdr() const { return std::visit([](auto const& x)->const NodeHeader&{ return x.h; }, v); }
};

struct Block {
  NodeHeader h;
  std::vector<Stmt> stmts;
};

struct ProcDecl {
  NodeHeader h;
  std::string name;
  std::string ret_type;
  Block body;
};

struct Unit {
  NodeHeader h;
  std::vector<ProcDecl> procs;

  // block arena so If/Switch/TryFinally can point to blocks without moving
  std::vector<Block> block_arena;
};

//------------------------------------------------------------------------------
// Parser (Pratt expressions + sugar statements)
//------------------------------------------------------------------------------

struct Parser {
  std::vector<Token> toks;
  size_t p = 0;
  NodeId next_id = 1;
  Unit* unit = nullptr;

  explicit Parser(std::vector<Token> t) : toks(std::move(t)) {}

  const Token& cur() const { return toks[p]; }
  const Token& peek(size_t n=1) const { return toks[p+n]; }
  bool at(TokKind k) const { return cur().kind == k; }
  Token take() { return toks[p++]; }

  [[noreturn]] void perr(std::string msg) const { die({DiagCode::ParseError, cur().span, std::move(msg)}); }

  void skip_newlines() {
    while (at(TokKind::Newline)) take();
  }

  void expect(TokKind k, std::string_view what) {
    if (!at(k)) perr("expected " + std::string(what) + ", got '" + cur().text + "'");
    take();
  }

  NodeHeader hdr(NodeKind k, const Token& first, const Token& last, Span sp) {
    return NodeHeader{k, next_id++, sp, first.ordinal, last.ordinal};
  }

  // ---------------------------
  // Pratt precedence table
  // Deterministic and total ordering.
  // ---------------------------
  struct Prec { int lbp; int rbp; }; // left/right binding power

  static std::optional<Prec> infix_prec(TokKind k) {
    // Higher = tighter binding.
    switch (k) {
      case TokKind::Star:    return Prec{70, 71};
      case TokKind::Slash:   return Prec{70, 71};
      case TokKind::Percent: return Prec{70, 71};

      case TokKind::Plus:    return Prec{60, 61};
      case TokKind::Minus:   return Prec{60, 61};

      case TokKind::Shl:     return Prec{55, 56};
      case TokKind::Shr:     return Prec{55, 56};

      case TokKind::Lt:      return Prec{50, 51};
      case TokKind::Lte:     return Prec{50, 51};
      case TokKind::Gt:      return Prec{50, 51};
      case TokKind::Gte:     return Prec{50, 51};

      case TokKind::EqEq:    return Prec{45, 46};
      case TokKind::NotEq:   return Prec{45, 46};

      case TokKind::Amp:     return Prec{40, 41};
      case TokKind::Caret:   return Prec{39, 40};
      case TokKind::Pipe:    return Prec{38, 39};

      case TokKind::AndAnd:  return Prec{30, 31};
      case TokKind::OrOr:    return Prec{29, 30};

      default: return std::nullopt;
    }
  }

  // Prefix parsing
  Expr parse_prefix() {
    skip_newlines();
    Token first = cur();

    // prefix ops
    if (at(TokKind::Minus) || at(TokKind::Bang) || at(TokKind::Tilde)) {
      TokKind opk = cur().kind;
      take();
      Expr rhs = parse_expr_bp(80); // prefix binds tighter than infix
      Token last = toks[p-1];

      UnaryExpr ue;
      ue.op = (opk == TokKind::Minus) ? UnOp::Neg : (opk == TokKind::Bang) ? UnOp::Not : UnOp::BitNot;
      ue.rhs = std::make_unique<Expr>(std::move(rhs));
      ue.h = hdr(NodeKind::UnaryExpr, first, last, merge_span(first,last));
      return Expr{std::move(ue)};
    }

    // primary
    if (at(TokKind::IntLit)) {
      Token t = take();
      std::string cleaned;
      for (char c : t.text) if (c != '_') cleaned.push_back(c);
      int64_t v = 0;
      try { v = std::stoll(cleaned); } catch (...) { die({DiagCode::ParseError, t.span, "invalid int literal"}); }
      IntExpr ie;
      ie.value = v;
      ie.h = hdr(NodeKind::IntExpr, t, t, t.span);
      return Expr{ie};
    }

    if (at(TokKind::StringLit)) {
      Token t = take();
      StringExpr se;
      se.value = t.text;
      se.h = hdr(NodeKind::StringExpr, t, t, t.span);
      return Expr{se};
    }

    if (at(TokKind::Ident)) {
      Token t = take();
      IdentExpr id;
      id.name = t.text;
      id.h = hdr(NodeKind::IdentExpr, t, t, t.span);
      Expr e{id};
      return parse_postfix(std::move(e), first);
    }

    if (at(TokKind::LParen)) {
      Token lp = take();
      Expr e = parse_expr_bp(0);
      Token rp = cur();
      expect(TokKind::RParen, "')'");
      // treat as identity (no ParenExpr node, spans merged)
      e.hdr().span = merge_span(lp, rp);
      e.hdr().first_tok = lp.ordinal;
      e.hdr().last_tok  = rp.ordinal;
      return parse_postfix(std::move(e), lp);
    }

    perr("expected expression");
    return Expr{IntExpr{}};
  }

  // postfix: member access and call:
  // - canonical call: f(a,b)
  // - sugar call:     f x y   (only when statement-head OR after certain verbs; we handle in stmt parser)
  Expr parse_postfix(Expr base, const Token& firstTok) {
    while (true) {
      if (at(TokKind::Dot)) {
        Token dot = take();
        Token mem = cur();
        expect(TokKind::Ident, "member identifier");
        MemberExpr me;
        me.base = std::make_unique<Expr>(std::move(base));
        me.member = mem.text;
        me.h = hdr(NodeKind::MemberExpr, firstTok, mem, merge_span(firstTok, mem));
        base = Expr{std::move(me)};
        continue;
      }

      if (at(TokKind::LParen)) {
        Token lp = take();
        std::vector<Expr> args;
        if (!at(TokKind::RParen)) {
          while (true) {
            args.push_back(parse_expr_bp(0));
            if (at(TokKind::Comma)) { take(); continue; }
            break;
          }
        }
        Token rp = cur();
        expect(TokKind::RParen, "')'");
        CallExpr ce;
        ce.callee = std::make_unique<Expr>(std::move(base));
        ce.args = std::move(args);
        ce.h = hdr(NodeKind::CallExpr, firstTok, rp, merge_span(firstTok, rp));
        base = Expr{std::move(ce)};
        continue;
      }

      break;
    }
    return base;
  }

  Expr parse_expr_bp(int min_bp) {
    Expr lhs = parse_prefix();
    Token firstTok = toks[p-1]; // best-effort anchor

    while (true) {
      skip_newlines();
      auto pr = infix_prec(cur().kind);
      if (!pr) break;
      if (pr->lbp < min_bp) break;

      TokKind opk = cur().kind;
      Token opTok = take();

      Expr rhs = parse_expr_bp(pr->rbp);
      Token lastTok = toks[p-1];

      BinOp bop{};
      switch (opk) {
        case TokKind::Plus: bop = BinOp::Add; break;
        case TokKind::Minus: bop = BinOp::Sub; break;
        case TokKind::Star: bop = BinOp::Mul; break;
        case TokKind::Slash: bop = BinOp::Div; break;
        case TokKind::Percent: bop = BinOp::Mod; break;

        case TokKind::Shl: bop = BinOp::Shl; break;
        case TokKind::Shr: bop = BinOp::Shr; break;

        case TokKind::Lt: bop = BinOp::Lt; break;
        case TokKind::Lte: bop = BinOp::Lte; break;
        case TokKind::Gt: bop = BinOp::Gt; break;
        case TokKind::Gte: bop = BinOp::Gte; break;

        case TokKind::EqEq: bop = BinOp::Eq; break;
        case TokKind::NotEq: bop = BinOp::Ne; break;

        case TokKind::Amp: bop = BinOp::BitAnd; break;
        case TokKind::Caret: bop = BinOp::BitXor; break;
        case TokKind::Pipe: bop = BinOp::BitOr; break;

        case TokKind::AndAnd: bop = BinOp::And; break;
        case TokKind::OrOr: bop = BinOp::Or; break;

        default: perr("unhandled binary operator"); break;
      }

      BinaryExpr be;
      be.op = bop;
      be.lhs = std::make_unique<Expr>(std::move(lhs));
      be.rhs = std::make_unique<Expr>(std::move(rhs));
      be.h = hdr(NodeKind::BinaryExpr, firstTok, lastTok, merge_span(firstTok,lastTok));
      lhs = Expr{std::move(be)};
      (void)opTok;
    }

    return lhs;
  }

  // ---------------------------
  // Blocks and statements
  // ---------------------------

  Block* new_block(const Token& firstTok) {
    unit->block_arena.push_back(Block{});
    Block& b = unit->block_arena.back();
    b.h.kind = NodeKind::Block;
    b.h.id = next_id++;
    b.h.first_tok = firstTok.ordinal;
    b.h.span = firstTok.span;
    return &b;
  }

  Block parse_block_colon() {
    Token first = cur();
    expect(TokKind::Colon, "':'");
    if (at(TokKind::Newline)) take();
    Token ind = cur();
    expect(TokKind::Indent, "INDENT");

    Block b;
    b.h.kind = NodeKind::Block;
    b.h.id = next_id++;
    b.h.first_tok = first.ordinal;
    b.h.span = merge_span(first, ind);

    while (!at(TokKind::Dedent) && !at(TokKind::Eof)) {
      if (at(TokKind::Newline)) { take(); continue; }
      b.stmts.push_back(parse_stmt());
      skip_newlines();
    }

    Token ded = cur();
    expect(TokKind::Dedent, "DEDENT");
    b.h.last_tok = ded.ordinal;
    b.h.span = merge_span(first, ded);
    return b;
  }

  // sugar call statement: IDENT expr expr ...
  // We take it only at statement head.
  Expr parse_sugar_call_from_ident(const Token& identTok) {
    // callee is ident
    IdentExpr id;
    id.name = identTok.text;
    id.h = hdr(NodeKind::IdentExpr, identTok, identTok, identTok.span);
    Expr callee{id};

    std::vector<Expr> args;
    // Read args until newline/dedent/end/keyword boundary.
    while (true) {
      if (at(TokKind::Newline) || at(TokKind::Dedent) || at(TokKind::Eof) || at(TokKind::KwEnd)) break;

      // Stop if next token begins a statement keyword
      TokKind k = cur().kind;
      if (k == TokKind::KwReturn || k == TokKind::KwLet || k == TokKind::KwIf ||
          k == TokKind::KwMatch || k == TokKind::KwWith || k == TokKind::KwDefer ||
          k == TokKind::KwLock) break;

      args.push_back(parse_expr_bp(0));
      skip_newlines();
    }

    Token last = toks[p-1];
    CallExpr ce;
    ce.callee = std::make_unique<Expr>(std::move(callee));
    ce.args = std::move(args);
    ce.h = hdr(NodeKind::CallExpr, identTok, last, merge_span(identTok,last));
    return Expr{std::move(ce)};
  }

  Stmt parse_stmt() {
    skip_newlines();
    Token first = cur();

    if (at(TokKind::KwReturn)) {
      take();
      std::optional<Expr> val;
      if (!at(TokKind::Newline) && !at(TokKind::Dedent) && !at(TokKind::KwEnd) && !at(TokKind::Eof)) {
        val = parse_expr_bp(0);
      }
      Token last = val ? toks[p-1] : first;
      ReturnStmt rs;
      rs.value = std::move(val);
      rs.h = hdr(NodeKind::ReturnStmt, first, last, merge_span(first,last));
      return Stmt{std::move(rs)};
    }

    if (at(TokKind::KwLet)) {
      take();
      Token nameTok = cur();
      expect(TokKind::Ident, "identifier");
      std::string name = nameTok.text;

      std::string typeName;
      // sugar: let x i64 = expr
      if (at(TokKind::Ident) && peek().kind == TokKind::Assign) {
        Token t = take();
        typeName = t.text;
      }

      expect(TokKind::Assign, "'='");
      Expr init = parse_expr_bp(0);
      Token last = toks[p-1];

      LetStmt ls;
      ls.name = std::move(name);
      ls.type_name = std::move(typeName);
      ls.init = std::move(init);
      ls.h = hdr(NodeKind::LetStmt, first, last, merge_span(first,last));
      return Stmt{std::move(ls)};
    }

    if (at(TokKind::KwIf)) {
      take();
      Expr cond = parse_expr_bp(0);
      Block thenB = parse_block_colon();
      Block* thenP = new_block(first);
      *thenP = std::move(thenB);

      Block* elseP = nullptr;
      skip_newlines();
      if (at(TokKind::KwElse)) {
        Token els = take();
        Block elseB = parse_block_colon();
        elseP = new_block(els);
        *elseP = std::move(elseB);
      }

      Token last = toks[p-1];
      IfStmt is;
      is.cond = std::move(cond);
      is.then_blk = thenP;
      is.else_blk = elseP;
      is.h = hdr(NodeKind::IfStmt, first, last, merge_span(first,last));
      return Stmt{std::move(is)};
    }

    if (at(TokKind::KwMatch)) {
      take();
      Expr scrut = parse_expr_bp(0);
      Token col = cur();
      expect(TokKind::Colon, "':'");
      if (at(TokKind::Newline)) take();
      expect(TokKind::Indent, "INDENT");

      SwitchStmt sw;
      sw.scrutinee = std::move(scrut);

      while (!at(TokKind::Dedent) && !at(TokKind::Eof)) {
        if (at(TokKind::Newline)) { take(); continue; }

        Token k = cur();
        if (at(TokKind::KwCase)) {
          take();
          Token lit = cur();
          expect(TokKind::IntLit, "int literal case");
          std::string cleaned;
          for (char c : lit.text) if (c != '_') cleaned.push_back(c);
          int64_t cv = std::stoll(cleaned);

          // optional: "case 1:" OR "case 1: stmt"
          Token colon = cur();
          expect(TokKind::Colon, "':'");
          Block body;
          // allow single-line body: case 1: print 7
          if (at(TokKind::Newline)) {
            // indented nested body not supported in minimal match; accept inline-only
            take();
            // treat empty body
            body = Block{};
            body.h.kind = NodeKind::Block;
            body.h.id = next_id++;
            body.h.first_tok = colon.ordinal;
            body.h.last_tok = colon.ordinal;
            body.h.span = colon.span;
          } else {
            body.h.kind = NodeKind::Block;
            body.h.id = next_id++;
            body.h.first_tok = colon.ordinal;
            body.stmts.push_back(parse_stmt());
            Token lastS = toks[p-1];
            body.h.last_tok = lastS.ordinal;
            body.h.span = merge_span(colon, lastS);
          }

          Block* bp = new_block(k);
          *bp = std::move(body);
          sw.cases.push_back({cv, bp, k.span});
          skip_newlines();
          continue;
        }

        if (at(TokKind::KwDefault)) {
          take();
          expect(TokKind::Colon, "':'");
          Block body;
          body.h.kind = NodeKind::Block;
          body.h.id = next_id++;
          body.h.first_tok = cur().ordinal;
          // inline stmt
          if (!at(TokKind::Newline) && !at(TokKind::Dedent)) body.stmts.push_back(parse_stmt());
          Token lastS = toks[p-1];
          body.h.last_tok = lastS.ordinal;
          body.h.span = merge_span(first, lastS);

          Block* bp = new_block(first);
          *bp = std::move(body);
          sw.default_blk = bp;
          skip_newlines();
          continue;
        }

        perr("match expects 'case' or 'default'");
      }

      Token ded = cur();
      expect(TokKind::Dedent, "DEDENT");
      Token last = ded;

      sw.h = hdr(NodeKind::SwitchStmt, first, last, merge_span(first,last));
      return Stmt{std::move(sw)};
    }

    // with/lock/defer/spawn/join are parsed as statements and de-sugared by CIAM later.
    // Here we keep them as ExprStmt “callish” forms for uniform lowering:
    if (at(TokKind::KwWith) || at(TokKind::KwLock) || at(TokKind::KwDefer)) {
      // Parse as an expression-statement using a pseudo-callee name:
      // with open path as f: ... end  (stored as call expr on ident "with" then CIAM lowers)
      // lock m1: ... end              (call on ident "lock")
      // defer close f                 (call on ident "defer")
      Token kw = take();
      Token kwAsIdent = kw;
      kwAsIdent.kind = TokKind::Ident;

      // Build sugar call: kw arg...
      // For with/lock: require ':' block; store block into a special call arg = Ident("__blockN")
      // Minimal approach: parse "with <expr> as <ident> : <block>"
      Expr callish;

      if (kw.kind == TokKind::KwDefer) {
        // defer <expr> [<expr>...]
        // parse one expression after defer
        Expr e = parse_expr_bp(0);
        // represent as: defer(e)
        CallExpr ce;
        IdentExpr id; id.name = "defer"; id.h = hdr(NodeKind::IdentExpr, kwAsIdent, kwAsIdent, kwAsIdent.span);
        ce.callee = std::make_unique<Expr>(Expr{id});
        ce.args.push_back(std::move(e));
        Token last = toks[p-1];
        ce.h = hdr(NodeKind::CallExpr, kwAsIdent, last, merge_span(kwAsIdent,last));
        callish = Expr{std::move(ce)};
      } else if (kw.kind == TokKind::KwLock) {
        Expr m = parse_expr_bp(0);
        Block body = parse_block_colon();
        // store body in arena and pass its block-id as a synthetic IdentExpr
        Block* bp = new_block(kw);
        *bp = std::move(body);
        // represent as: lock(m, __block<id>)
        CallExpr ce;
        IdentExpr id; id.name = "lock"; id.h = hdr(NodeKind::IdentExpr, kwAsIdent, kwAsIdent, kwAsIdent.span);
        ce.callee = std::make_unique<Expr>(Expr{id});
        ce.args.push_back(std::move(m));
        // synthetic block-ref expr: Ident("__block<id>")
        Token fake = kwAsIdent;
        IdentExpr bident;
        bident.name = "__block" + std::to_string(bp->h.id);
        bident.h = hdr(NodeKind::IdentExpr, fake, fake, fake.span);
        ce.args.push_back(Expr{bident});
        Token last = toks[p-1];
        ce.h = hdr(NodeKind::CallExpr, kwAsIdent, last, merge_span(kwAsIdent,last));
        callish = Expr{std::move(ce)};
      } else { // with
        Expr openExpr = parse_expr_bp(0);
        // accept: with <expr> as <ident> : block
        Token asTok = cur();
        if (!at(TokKind::Ident) || cur().text != "as") {
          perr("with requires 'as <name>'");
        }
        take(); // 'as' (tokenized as Ident)
        Token nameTok = cur();
        expect(TokKind::Ident, "with binding name");
        std::string bindName = nameTok.text;

        Block body = parse_block_colon();
        Block* bp = new_block(kw);
        *bp = std::move(body);

        CallExpr ce;
        IdentExpr id; id.name = "with"; id.h = hdr(NodeKind::IdentExpr, kwAsIdent, kwAsIdent, kwAsIdent.span);
        ce.callee = std::make_unique<Expr>(Expr{id});
        ce.args.push_back(std::move(openExpr));
        // bind name
        Token fake = nameTok;
        IdentExpr b; b.name = bindName; b.h = hdr(NodeKind::IdentExpr, fake, fake, fake.span);
        ce.args.push_back(Expr{b});
        // block ref
        Token fake2 = kwAsIdent;
        IdentExpr bref; bref.name = "__block" + std::to_string(bp->h.id);
        bref.h = hdr(NodeKind::IdentExpr, fake2, fake2, fake2.span);
        ce.args.push_back(Expr{bref});
        Token last = toks[p-1];
        ce.h = hdr(NodeKind::CallExpr, kwAsIdent, last, merge_span(kwAsIdent,last));
        callish = Expr{std::move(ce)};
        (void)asTok;
      }

      ExprStmt es;
      es.expr = std::move(callish);
      Token last = toks[p-1];
      es.h = hdr(NodeKind::ExprStmt, first, last, merge_span(first,last));
      return Stmt{std::move(es)};
    }

    // Statement-head sugar call: IDENT <expr>...
    if (at(TokKind::Ident)) {
      Token ident = take();
      Expr callish = parse_sugar_call_from_ident(ident);
      Token last = toks[p-1];
      ExprStmt es;
      es.expr = std::move(callish);
      es.h = hdr(NodeKind::ExprStmt, ident, last, merge_span(ident,last));
      return Stmt{std::move(es)};
    }

    perr("unsupported statement (extend parser here)");
    return Stmt{ReturnStmt{}};
  }

  ProcDecl parse_proc() {
    skip_newlines();
    Token first = cur();
    expect(TokKind::KwProc, "'proc'");
    Token nameTok = cur();
    expect(TokKind::Ident, "proc name");
    std::string name = nameTok.text;

    expect(TokKind::Arrow, "'->'");
    Token retTok = cur();
    expect(TokKind::Ident, "return type");
    std::string retType = retTok.text;

    Block body = parse_block_colon();
    skip_newlines();
    Token endTok = cur();
    expect(TokKind::KwEnd, "'end'");

    ProcDecl pd;
    pd.name = std::move(name);
    pd.ret_type = std::move(retType);
    pd.body = std::move(body);
    pd.h = hdr(NodeKind::ProcDecl, first, endTok, merge_span(first,endTok));
    return pd;
  }

  Unit parse_unit() {
    Unit u;
    unit = &u;
    skip_newlines();
    Token firstTok = cur();
    u.h.kind = NodeKind::Unit;
    u.h.id = next_id++;
    u.h.first_tok = firstTok.ordinal;
    u.h.span = firstTok.span;

    while (!at(TokKind::Eof)) {
      skip_newlines();
      if (at(TokKind::Eof)) break;
      if (at(TokKind::KwProc)) u.procs.push_back(parse_proc());
      else perr("only 'proc' supported at top-level in this layer");
      skip_newlines();
    }

    Token lastTok = cur();
    u.h.last_tok = lastTok.ordinal;
    u.h.span = merge_span(firstTok, lastTok);
    return u;
  }
};

//------------------------------------------------------------------------------
// CIAM engine interfaces + canonical emitter
//------------------------------------------------------------------------------

enum class GuardKind : uint16_t { Bounds=1, CapBoundary, DeterminismFence };

enum class CapKind : uint16_t {
  heap_alloc = 1, file_io, network_io, dynamic_eval, syscalls, threads, channels, crypto
};

struct CiamArtifact { std::string name; std::vector<uint8_t> bytes; };

struct CiamCtx {
  std::vector<Diag> diags;
  std::vector<CiamArtifact> artifacts;

  std::vector<CapKind> required_caps;
  struct GuardRec { GuardKind kind; uint32_t anchor_tok; Span span; };
  std::vector<GuardRec> guards;

  void diag(DiagCode code, Span sp, std::string msg) { diags.push_back({code, sp, std::move(msg)}); }
};

static bool ciam_require_cap(CiamCtx& ctx, CapKind cap, Span /*span*/) {
  if (std::find(ctx.required_caps.begin(), ctx.required_caps.end(), cap) == ctx.required_caps.end())
    ctx.required_caps.push_back(cap);
  return true;
}

static void ciam_emit_guard(CiamCtx& ctx, GuardKind kind, uint32_t anchor_tok, Span span) {
  ctx.guards.push_back({kind, anchor_tok, span});
}

// Canonical pretty-printer (syntax.ciam.rane)
// - braces + semicolons normalized
// - explicit try/finally used for with/defer/lock
// - spawn/join become runtime calls
// - match becomes switch
//
// IMPORTANT: this is a *canonical surface* printer, not IR.
struct CanonWriter {
  std::ostringstream out;
  int indent = 0;

  void nl() { out << "\n"; for (int i=0;i<indent;i++) out << "  "; }
  void w(std::string_view s) { out << s; }

  static std::string binop_str(BinOp o) {
    switch (o) {
      case BinOp::Add: return "+"; case BinOp::Sub: return "-"; case BinOp::Mul: return "*";
      case BinOp::Div: return "/"; case BinOp::Mod: return "%";
      case BinOp::Shl: return "<<"; case BinOp::Shr: return ">>";
      case BinOp::Lt: return "<"; case BinOp::Lte: return "<="; case BinOp::Gt: return ">";
      case BinOp::Gte: return ">="; case BinOp::Eq: return "=="; case BinOp::Ne: return "!=";
      case BinOp::BitAnd: return "&"; case BinOp::BitXor: return "^"; case BinOp::BitOr: return "|";
      case BinOp::And: return "&&"; case BinOp::Or: return "||";
    }
    return "?";
  }

  void emit_expr(const Expr& e) {
    if (std::holds_alternative<IntExpr>(e.v)) {
      out << std::get<IntExpr>(e.v).value;
    } else if (std::holds_alternative<StringExpr>(e.v)) {
      out << "\"";
      for (char c : std::get<StringExpr>(e.v).value) {
        if (c == '\\') out << "\\\\";
        else if (c == '"') out << "\\\"";
        else if (c == '\n') out << "\\n";
        else out << c;
      }
      out << "\"";
    } else if (std::holds_alternative<IdentExpr>(e.v)) {
      out << std::get<IdentExpr>(e.v).name;
    } else if (std::holds_alternative<UnaryExpr>(e.v)) {
      auto const& u = std::get<UnaryExpr>(e.v);
      out << (u.op==UnOp::Neg? "-" : u.op==UnOp::Not? "!" : "~");
      emit_expr(*u.rhs);
    } else if (std::holds_alternative<BinaryExpr>(e.v)) {
      auto const& b = std::get<BinaryExpr>(e.v);
      out << "("; emit_expr(*b.lhs); out << " " << binop_str(b.op) << " "; emit_expr(*b.rhs); out << ")";
    } else if (std::holds_alternative<MemberExpr>(e.v)) {
      auto const& m = std::get<MemberExpr>(e.v);
      emit_expr(*m.base);
      out << "." << m.member;
    } else if (std::holds_alternative<CallExpr>(e.v)) {
      auto const& c = std::get<CallExpr>(e.v);
      emit_expr(*c.callee);
      out << "(";
      for (size_t i=0;i<c.args.size();i++) {
        emit_expr(c.args[i]);
        if (i+1<c.args.size()) out << ", ";
      }
      out << ")";
    }
  }

  void emit_block(const Block& b) {
    w("{");
    indent++;
    for (auto const& st : b.stmts) {
      nl();
      emit_stmt(st);
    }
    indent--;
    nl();
    w("}");
  }

  void emit_stmt(const Stmt& s) {
    if (std::holds_alternative<ReturnStmt>(s.v)) {
      auto const& r = std::get<ReturnStmt>(s.v);
      w("return");
      if (r.value) { w(" "); emit_expr(*r.value); }
      w(";");
      return;
    }
    if (std::holds_alternative<LetStmt>(s.v)) {
      auto const& l = std::get<LetStmt>(s.v);
      w("let "); w(l.name);
      if (!l.type_name.empty()) { w(": "); w(l.type_name); }
      w(" = "); emit_expr(l.init); w(";");
      return;
    }
    if (std::holds_alternative<ExprStmt>(s.v)) {
      auto const& e = std::get<ExprStmt>(s.v);
      emit_expr(e.expr);
      w(";");
      return;
    }
    if (std::holds_alternative<IfStmt>(s.v)) {
      auto const& is = std::get<IfStmt>(s.v);
      w("if ("); emit_expr(is.cond); w(") ");
      emit_block(*is.then_blk);
      if (is.else_blk) { w(" else "); emit_block(*is.else_blk); }
      return;
    }
    if (std::holds_alternative<SwitchStmt>(s.v)) {
      auto const& sw = std::get<SwitchStmt>(s.v);
      w("switch "); emit_expr(sw.scrutinee); w(" ");
      w("{"); indent++;
      for (auto const& c : sw.cases) {
        nl(); w("case "); out << c.value; w(": ");
        emit_block(*c.body);
      }
      if (sw.default_blk) { nl(); w("default: "); emit_block(*sw.default_blk); }
      indent--;
      nl(); w("}");
      return;
    }
    if (std::holds_alternative<TryFinallyStmt>(s.v)) {
      auto const& tf = std::get<TryFinallyStmt>(s.v);
      w("try "); emit_block(*tf.try_blk);
      w(" finally "); emit_block(*tf.finally_blk);
      return;
    }
    w("/* unsupported stmt */;");
  }
};

// CIAM de-sugaring: transforms “callish” sugar nodes into canonical ones.
// Currently implemented:
// - defer X       => wraps remaining block in try/finally { X; }
// - lock m: body  => mutex_lock(m); try{body} finally{mutex_unlock(m);}
// - with openE as f: body => let f=openE; try{body} finally{close(f);}
// - spawn f arg   => rane_rt_threads.spawn_proc(f, arg) (as expr)
// - join th       => rane_rt_threads.join_i64(th) (as expr)
// - match already parsed to SwitchStmt (so CIAM simply canonicalizes printing)
//
// Implementation strategy (buildable, incremental):
// - We rewrite within blocks into TryFinallyStmt where needed.
// - For with/lock we used placeholder call forms with "__block<ID>" markers.
static Block* find_block_by_marker(Unit& u, const std::string& marker) {
  if (marker.rfind("__block", 0) != 0) return nullptr;
  uint32_t id = (uint32_t)std::stoul(marker.substr(7));
  for (auto& b : u.block_arena) if (b.h.id == id) return &b;
  return nullptr;
}

static Expr make_ident(NodeId& nid, const Token& t, std::string name) {
  IdentExpr id;
  id.name = std::move(name);
  id.h = NodeHeader{NodeKind::IdentExpr, nid++, t.span, t.ordinal, t.ordinal};
  return Expr{id};
}

static Expr make_call(NodeId& nid, const Token& t, std::string callee, std::vector<Expr> args) {
  CallExpr ce;
  ce.callee = std::make_unique<Expr>(make_ident(nid, t, std::move(callee)));
  ce.args = std::move(args);
  ce.h = NodeHeader{NodeKind::CallExpr, nid++, t.span, t.ordinal, t.ordinal};
  return Expr{std::move(ce)};
}

static Stmt make_expr_stmt(NodeId& nid, const Token& t, Expr e) {
  ExprStmt es;
  es.expr = std::move(e);
  es.h = NodeHeader{NodeKind::ExprStmt, nid++, t.span, t.ordinal, t.ordinal};
  return Stmt{std::move(es)};
}

static Stmt make_try_finally(NodeId& nid, const Token& t, Block* tryb, Block* finb) {
  TryFinallyStmt tf;
  tf.try_blk = tryb;
  tf.finally_blk = finb;
  tf.h = NodeHeader{NodeKind::TryFinallyStmt, nid++, t.span, t.ordinal, t.ordinal};
  return Stmt{std::move(tf)};
}

static bool ciam_desugar_block(Unit& u, Block& b, CiamCtx& ctx);

static bool ciam_desugar_stmt(Unit& u, Stmt& s, CiamCtx& ctx) {
  (void)ctx;

  // Recurse into structured statements first
  if (std::holds_alternative<IfStmt>(s.v)) {
    auto& is = std::get<IfStmt>(s.v);
    if (is.then_blk) if (!ciam_desugar_block(u, *is.then_blk, ctx)) return false;
    if (is.else_blk) if (!ciam_desugar_block(u, *is.else_blk, ctx)) return false;
    return true;
  }
  if (std::holds_alternative<SwitchStmt>(s.v)) {
    auto& sw = std::get<SwitchStmt>(s.v);
    for (auto& c : sw.cases) if (c.body) if (!ciam_desugar_block(u, *c.body, ctx)) return false;
    if (sw.default_blk) if (!ciam_desugar_block(u, *sw.default_blk, ctx)) return false;
    return true;
  }
  if (std::holds_alternative<TryFinallyStmt>(s.v)) {
    auto& tf = std::get<TryFinallyStmt>(s.v);
    if (tf.try_blk) if (!ciam_desugar_block(u, *tf.try_blk, ctx)) return false;
    if (tf.finally_blk) if (!ciam_desugar_block(u, *tf.finally_blk, ctx)) return false;
    return true;
  }

  // Detect special callish expr statements: with/lock/defer
  if (std::holds_alternative<ExprStmt>(s.v)) {
    auto& es = std::get<ExprStmt>(s.v);

    if (std::holds_alternative<CallExpr>(es.expr.v)) {
      auto& ce = std::get<CallExpr>(es.expr.v);

      // callee must be IdentExpr
      if (!std::holds_alternative<IdentExpr>(ce.callee->v)) return true;
      const std::string callee = std::get<IdentExpr>(ce.callee->v).name;

      // NOTE: NodeId stable extension: reuse u.h.id increments by simple counter.
      // Here we use a local counter derived from u.procs size; ok for this layer.
      NodeId nid = 100000 + u.h.id;

      Token fakeTok;
      fakeTok.ordinal = es.h.first_tok;
      fakeTok.span = es.h.span;

      if (callee == "defer") {
        // Represented as defer(expr)
        // We don't rewrite here; handled at block-level by collecting defers.
        return true;
      }

      if (callee == "lock") {
        // lock(m, __block<ID>)
        if (ce.args.size() != 2 || !std::holds_alternative<IdentExpr>(ce.args[1].v)) return true;
        auto marker = std::get<IdentExpr>(ce.args[1].v).name;
        Block* body = find_block_by_marker(u, marker);
        if (!body) return true;

        // Build: mutex_lock(m); try { body } finally { mutex_unlock(m); }
        // We'll replace current stmt later at block-level (because we need to insert pre-call + wrap).
        // Here we keep as-is; block-level pass will rewrite sequences.
        return true;
      }

      if (callee == "with") {
        // with(openExpr, bindName, __block<ID>)
        if (ce.args.size() != 3) return true;
        if (!std::holds_alternative<IdentExpr>(ce.args[1].v)) return true;
        if (!std::holds_alternative<IdentExpr>(ce.args[2].v)) return true;

        auto marker = std::get<IdentExpr>(ce.args[2].v).name;
        Block* body = find_block_by_marker(u, marker);
        if (!body) return true;

        // handled at block-level
        (void)nid; (void)fakeTok;
        return true;
      }
    }
  }

  return true;
}

static bool ciam_desugar_block(Unit& u, Block& b, CiamCtx& ctx) {
  // First recurse into children
  for (auto& st : b.stmts) if (!ciam_desugar_stmt(u, st, ctx)) return false;

  // Then block-local transformations:
  // 1) Collect defers (in order) and remove them from statement list.
  // 2) Rewrite with/lock to explicit forms (insert + wrap).
  // 3) If any defers exist, wrap remainder in try/finally.

  std::vector<Expr> defers;
  std::vector<Stmt> out;

  auto is_call_on = [&](const Expr& e, std::string_view name)->const CallExpr* {
    if (!std::holds_alternative<CallExpr>(e.v)) return nullptr;
    auto const& ce = std::get<CallExpr>(e.v);
    if (!std::holds_alternative<IdentExpr>(ce.callee->v)) return nullptr;
    if (std::get<IdentExpr>(ce.callee->v).name != name) return nullptr;
    return &ce;
  };

  // Token anchor for synthetic nodes
  Token fakeTok; fakeTok.ordinal = b.h.first_tok; fakeTok.span = b.h.span;

  // We'll need a stable node-id generator; reuse u.h.id base.
  NodeId nid = 200000 + u.h.id;

  for (auto& st : b.stmts) {
    if (std::holds_alternative<ExprStmt>(st.v)) {
      auto const& es = std::get<ExprStmt>(st.v);
      if (auto ce = is_call_on(es.expr, "defer")) {
        if (ce->args.size() == 1) defers.push_back(ce->args[0]);
        continue; // drop defer statement
      }
    }
    out.push_back(std::move(st));
  }

  // Rewrite “with” and “lock” occurrences in out:
  std::vector<Stmt> out2;
  for (size_t i=0;i<out.size();i++) {
    Stmt& st = out[i];
    if (std::holds_alternative<ExprStmt>(st.v)) {
      auto& es = std::get<ExprStmt>(st.v);
      if (auto ce = is_call_on(es.expr, "lock")) {
        // lock(m, __block<ID>)
        if (ce->args.size()==2 && std::holds_alternative<IdentExpr>(ce->args[1].v)) {
          auto marker = std::get<IdentExpr>(ce->args[1].v).name;
          Block* body = find_block_by_marker(u, marker);
          if (body) {
            // mutex_lock(m);
            out2.push_back(make_expr_stmt(nid, fakeTok, make_call(nid, fakeTok, "rane_rt_threads.mutex_lock", {ce->args[0]})));

            // finally { mutex_unlock(m); }
            u.block_arena.push_back(Block{});
            Block* fin = &u.block_arena.back();
            fin->h.kind = NodeKind::Block; fin->h.id = nid++; fin->h.first_tok = fakeTok.ordinal; fin->h.last_tok = fakeTok.ordinal; fin->h.span = fakeTok.span;
            fin->stmts.push_back(make_expr_stmt(nid, fakeTok, make_call(nid, fakeTok, "rane_rt_threads.mutex_unlock", {ce->args[0]})));

            // try { body } finally { fin }
            out2.push_back(make_try_finally(nid, fakeTok, body, fin));
            continue;
          }
        }
      }

      if (auto ce = is_call_on(es.expr, "with")) {
        // with(openExpr, bindName, __block<ID>)
        if (ce->args.size()==3 && std::holds_alternative<IdentExpr>(ce->args[1].v) && std::holds_alternative<IdentExpr>(ce->args[2].v)) {
          auto bindName = std::get<IdentExpr>(ce->args[1].v).name;
          auto marker = std::get<IdentExpr>(ce->args[2].v).name;
          Block* body = find_block_by_marker(u, marker);
          if (body) {
            // let f = openExpr;
            LetStmt ls;
            ls.name = bindName;
            ls.type_name = ""; // unknown here
            ls.init = ce->args[0];
            ls.h = NodeHeader{NodeKind::LetStmt, nid++, fakeTok.span, fakeTok.ordinal, fakeTok.ordinal};
            out2.push_back(Stmt{std::move(ls)});

            // finally { close(f); }
            u.block_arena.push_back(Block{});
            Block* fin = &u.block_arena.back();
            fin->h.kind = NodeKind::Block; fin->h.id = nid++; fin->h.first_tok = fakeTok.ordinal; fin->h.last_tok = fakeTok.ordinal; fin->h.span = fakeTok.span;
            // close(f)
            Expr fIdent = make_ident(nid, fakeTok, bindName);
            fin->stmts.push_back(make_expr_stmt(nid, fakeTok, make_call(nid, fakeTok, "close", {std::move(fIdent)})));

            out2.push_back(make_try_finally(nid, fakeTok, body, fin));
            continue;
          }
        }
      }
    }

    out2.push_back(std::move(st));
  }

  // If defers exist, wrap the remainder in try/finally
  if (!defers.empty()) {
    // try { out2... } finally { defers... }
    u.block_arena.push_back(Block{});
    Block* tryb = &u.block_arena.back();
    tryb->h.kind = NodeKind::Block; tryb->h.id = nid++; tryb->h.first_tok = fakeTok.ordinal; tryb->h.last_tok = fakeTok.ordinal; tryb->h.span = fakeTok.span;
    tryb->stmts = std::move(out2);

    u.block_arena.push_back(Block{});
    Block* finb = &u.block_arena.back();
    finb->h.kind = NodeKind::Block; finb->h.id = nid++; finb->h.first_tok = fakeTok.ordinal; finb->h.last_tok = fakeTok.ordinal; finb->h.span = fakeTok.span;

    for (auto it = defers.rbegin(); it != defers.rend(); ++it) {
      finb->stmts.push_back(make_expr_stmt(nid, fakeTok, *it));
    }

    b.stmts.clear();
    b.stmts.push_back(make_try_finally(nid, fakeTok, tryb, finb));
  } else {
    b.stmts = std::move(out2);
  }

  return true;
}

static bool ciam_pass_run(CiamCtx& ctx, Unit& unit, std::vector<CiamArtifact>& out_artifacts) {
  // Validate entrypoint
  ProcDecl* mainp = nullptr;
  for (auto& p : unit.procs) if (p.name == "main") { mainp = &p; break; }
  if (!mainp) { ctx.diag(DiagCode::ParseError, unit.h.span, "missing entrypoint proc main"); return false; }
  if (mainp->ret_type != "int") { ctx.diag(DiagCode::TypeMismatch, mainp->h.span, "proc main must return int"); return false; }

  ciam_emit_guard(ctx, GuardKind::DeterminismFence, mainp->h.first_tok, mainp->h.span);

  // Desugar blocks
  if (!ciam_desugar_block(unit, mainp->body, ctx)) return false;

  // Emit canonical syntax.ciam.rane
  CanonWriter cw;
  cw.w("// syntax.ciam.rane\n");
  cw.w("// CIAM-LOWERED CANONICAL ARTIFACT (DE-SUGARED, EXPLICIT)\n");
  cw.w("// Pipeline: source -> optimized CIAM expansion -> machine code -> executor\n\n");

  for (auto const& p : unit.procs) {
    cw.w("proc "); cw.w(p.name); cw.w("() -> "); cw.w(p.ret_type); cw.w(" ");
    cw.emit_block(p.body);
    cw.w("\n\n");
  }

  std::string s = cw.out.str();
  out_artifacts.push_back({"syntax.ciam.rane", std::vector<uint8_t>(s.begin(), s.end())});
  return true;
}

//------------------------------------------------------------------------------
// Minimal CFG-ish IR
//------------------------------------------------------------------------------

enum class IR_Op : uint16_t {
  ConstI64 = 1,
  LoadLocal,
  StoreLocal,
  AddI64, SubI64, MulI64, DivI64,
  CallPrintI64,
  RetI32
};

struct IR_Inst {
  IR_Op op{};
  int64_t a = 0;
  int64_t b = 0;
  int64_t c = 0;
};

struct IR_Block {
  std::string name;
  std::vector<IR_Inst> insts;
};

struct IR_Func {
  std::string name;
  std::vector<IR_Block> blocks; // minimal: start block only in this layer
  std::unordered_map<std::string, int32_t> locals; // name -> slot
};

struct IR_Module { IR_Func main; };

// Stable IR pretty-printer rules + embedded BNF header
static std::string ir_prettyprint(const IR_Module& m) {
  std::ostringstream o;
  o <<
R"(// syntax.opt.ciam.ir
// OPTIMIZED CIAM IR (STABLE PRETTYPRINT)
// Rule: canonical spacing, one instruction per line, numeric literals in decimal.
// Rule: blocks printed in insertion order; locals printed sorted by slot.
//
// BNF (prettyprinted IR):
//   ir_file     ::= header module
//   module      ::= "module" ident "{" func+ "}"
//   func        ::= "func" ident "(" ")" "{" locals? block+ "}"
//   locals      ::= "locals" "{" local+ "}"
//   local       ::= "%" int "=" ident
//   block       ::= "block" ident ":" nl inst*
//   inst        ::= opcode (operand ("," operand)*)? nl
//   opcode      ::= IDENT
//   operand     ::= int | "%" int | ident
//
// Note: This is IR text for tooling/debug. Execution uses binary code + exec meta.
)";

  o << "\nmodule rane {\n";
  o << "  func main() {\n";

  // locals sorted
  std::vector<std::pair<std::string,int32_t>> locs;
  for (auto const& kv : m.main.locals) locs.push_back({kv.first, kv.second});
  std::sort(locs.begin(), locs.end(), [](auto const& x, auto const& y){ return x.second < y.second; });

  if (!locs.empty()) {
    o << "    locals {\n";
    for (auto const& kv : locs) {
      o << "      %" << kv.second << " = " << kv.first << "\n";
    }
    o << "    }\n";
  }

  for (auto const& b : m.main.blocks) {
    o << "    block " << b.name << ":\n";
    for (auto const& in : b.insts) {
      auto opname = [&](IR_Op op)->const char* {
        switch(op) {
          case IR_Op::ConstI64: return "const.i64";
          case IR_Op::LoadLocal: return "load.local";
          case IR_Op::StoreLocal: return "store.local";
          case IR_Op::AddI64: return "add.i64";
          case IR_Op::SubI64: return "sub.i64";
          case IR_Op::MulI64: return "mul.i64";
          case IR_Op::DivI64: return "div.i64";
          case IR_Op::CallPrintI64: return "call.print_i64";
          case IR_Op::RetI32: return "ret.i32";
        }
        return "unknown";
      };

      o << "      " << opname(in.op);
      // operands by opcode
      switch (in.op) {
        case IR_Op::ConstI64: o << " " << in.a; break;                // push const
        case IR_Op::LoadLocal: o << " %" << in.a; break;              // push local
        case IR_Op::StoreLocal: o << " %" << in.a; break;             // pop -> local
        case IR_Op::AddI64:
        case IR_Op::SubI64:
        case IR_Op::MulI64:
        case IR_Op::DivI64: break;                                    // pop/pop push
        case IR_Op::CallPrintI64: break;                              // pop arg
        case IR_Op::RetI32: o << " " << (int32_t)in.a; break;          // immediate ret (layer)
      }
      o << "\n";
    }
  }

  o << "  }\n";
  o << "}\n";
  return o.str();
}

//------------------------------------------------------------------------------
// Lower AST to IR (minimal but real): handles
// - let x = <expr> (expr supports ints, idents, + - * /)
// - print <expr>   (sugar call -> CallExpr on ident "print")
// - return <int>   (immediate ret in this layer)
// - match/if/try are parsed but not fully lowered to CFG yet (hook points exist)
//------------------------------------------------------------------------------

static std::optional<int64_t> eval_const(const Expr& e) {
  if (std::holds_alternative<IntExpr>(e.v)) return std::get<IntExpr>(e.v).value;
  if (std::holds_alternative<UnaryExpr>(e.v)) {
    auto const& u = std::get<UnaryExpr>(e.v);
    auto v = eval_const(*u.rhs);
    if (!v) return std::nullopt;
    if (u.op == UnOp::Neg) return -*v;
    if (u.op == UnOp::Not) return (*v == 0) ? 1 : 0;
    if (u.op == UnOp::BitNot) return ~(*v);
  }
  if (std::holds_alternative<BinaryExpr>(e.v)) {
    auto const& b = std::get<BinaryExpr>(e.v);
    auto L = eval_const(*b.lhs);
    auto R = eval_const(*b.rhs);
    if (!L || !R) return std::nullopt;
    switch (b.op) {
      case BinOp::Add: return *L + *R;
      case BinOp::Sub: return *L - *R;
      case BinOp::Mul: return *L * *R;
      case BinOp::Div: return (*R==0)? std::nullopt : std::optional<int64_t>(*L / *R);
      case BinOp::Mod: return (*R==0)? std::nullopt : std::optional<int64_t>(*L % *R);
      default: return std::nullopt;
    }
  }
  return std::nullopt;
}

static void emit_expr_ir(IR_Func& f, IR_Block& b, const Expr& e) {
  // stack-machine style IR (simple, deterministic)
  if (auto v = eval_const(e)) {
    b.insts.push_back({IR_Op::ConstI64, *v, 0, 0});
    return;
  }

  if (std::holds_alternative<IdentExpr>(e.v)) {
    auto const& id = std::get<IdentExpr>(e.v);
    auto it = f.locals.find(id.name);
    if (it == f.locals.end()) die({DiagCode::UndefinedName, e.hdr().span, "undefined name: " + id.name});
    b.insts.push_back({IR_Op::LoadLocal, it->second, 0, 0});
    return;
  }

  if (std::holds_alternative<BinaryExpr>(e.v)) {
    auto const& be = std::get<BinaryExpr>(e.v);
    emit_expr_ir(f, b, *be.lhs);
    emit_expr_ir(f, b, *be.rhs);
    switch (be.op) {
      case BinOp::Add: b.insts.push_back({IR_Op::AddI64}); break;
      case BinOp::Sub: b.insts.push_back({IR_Op::SubI64}); break;
      case BinOp::Mul: b.insts.push_back({IR_Op::MulI64}); break;
      case BinOp::Div: b.insts.push_back({IR_Op::DivI64}); break;
      default:
        die({DiagCode::InternalError, e.hdr().span, "IR: unsupported binary op in this layer"});
    }
    return;
  }

  if (std::holds_alternative<CallExpr>(e.v)) {
    auto const& ce = std::get<CallExpr>(e.v);
    // Only supports print(x) or sugar print x (callee ident "print")
    if (std::holds_alternative<IdentExpr>(ce.callee->v)) {
      auto const& cal = std::get<IdentExpr>(ce.callee->v).name;
      if (cal == "print") {
        if (ce.args.size() != 1) die({DiagCode::TypeMismatch, e.hdr().span, "print expects 1 arg"});
        emit_expr_ir(f, b, ce.args[0]);
        b.insts.push_back({IR_Op::CallPrintI64});
        // print returns void; leave stack unchanged (for now)
        return;
      }
    }
    die({DiagCode::InternalError, e.hdr().span, "IR: unsupported call in this layer"});
  }

  die({DiagCode::InternalError, e.hdr().span, "IR: unsupported expression in this layer"});
}

static bool lower_ast_to_ir(CiamCtx& /*ctx*/, const Unit& unit, IR_Module& out) {
  // find main
  const ProcDecl* mainp = nullptr;
  for (auto const& p : unit.procs) if (p.name=="main") { mainp=&p; break; }
  if (!mainp) return false;

  out.main.name = "main";
  out.main.blocks.push_back(IR_Block{"entry", {}});

  IR_Block& entry = out.main.blocks.back();

  auto alloc_local = [&](const std::string& name)->int32_t {
    auto it = out.main.locals.find(name);
    if (it != out.main.locals.end()) return it->second;
    int32_t slot = (int32_t)out.main.locals.size();
    out.main.locals[name] = slot;
    return slot;
  };

  for (auto const& st : mainp->body.stmts) {
    if (std::holds_alternative<LetStmt>(st.v)) {
      auto const& ls = std::get<LetStmt>(st.v);
      int32_t slot = alloc_local(ls.name);
      emit_expr_ir(out.main, entry, ls.init);
      entry.insts.push_back({IR_Op::StoreLocal, slot});
      continue;
    }

    if (std::holds_alternative<ExprStmt>(st.v)) {
      auto const& es = std::get<ExprStmt>(st.v);
      emit_expr_ir(out.main, entry, es.expr);
      continue;
    }

    if (std::holds_alternative<ReturnStmt>(st.v)) {
      auto const& rs = std::get<ReturnStmt>(st.v);
      // Layer rule: return must be constant int in this layer (keeps codegen small)
      int32_t imm = 0;
      if (rs.value) {
        auto v = eval_const(*rs.value);
        if (!v || *v < INT32_MIN || *v > INT32_MAX) {
          die({DiagCode::TypeMismatch, rs.h.span, "return must be a constant i32 in this layer"});
        }
        imm = (int32_t)*v;
      }
      entry.insts.push_back({IR_Op::RetI32, imm});
      break;
    }

    // Hook points: if/switch/try lowering next layer
    if (std::holds_alternative<IfStmt>(st.v) || std::holds_alternative<SwitchStmt>(st.v) || std::holds_alternative<TryFinallyStmt>(st.v)) {
      die({DiagCode::InternalError, st.hdr().span, "CFG lowering not implemented yet for if/match/try in this layer (next step)"});
    }

    die({DiagCode::InternalError, st.hdr().span, "unsupported stmt in IR lowering"});
  }

  // Ensure a return exists
  if (entry.insts.empty() || entry.insts.back().op != IR_Op::RetI32) {
    entry.insts.push_back({IR_Op::RetI32, 0});
  }

  return true;
}

static void optimize_ir(IR_Module& m) {
  // Minimal optimizer:
  // - no-op for now; IR constant folding already handled by eval_const path.
  (void)m;
}

//------------------------------------------------------------------------------
// Host intrinsic: print_i64
// - machine code will call this function
//------------------------------------------------------------------------------

extern "C" void rane_host_print_i64(int64_t v) {
  std::cout << v << "\n";
}

//------------------------------------------------------------------------------
// Codegen (x86-64) for minimal IR:
// - stack machine: uses RAX/RBX and RSP temp pushes
// - supports: const, load/store locals (locals in a small stack frame), add/sub/mul/div, call print, ret
// - ABI-safe call to rane_host_print_i64:
//   - SysV: arg in RDI
//   - Win64: arg in RCX
//------------------------------------------------------------------------------

struct CodeBlob {
  std::vector<uint8_t> code;
  uint32_t entry_offset = 0;
};

static void emit_u8(std::vector<uint8_t>& c, uint8_t b) { c.push_back(b); }
static void emit_u32(std::vector<uint8_t>& c, uint32_t v) {
  for (int i=0;i<4;i++) c.push_back((uint8_t)((v>>(8*i))&0xFF));
}
static void emit_u64(std::vector<uint8_t>& c, uint64_t v) {
  for (int i=0;i<8;i++) c.push_back((uint8_t)((v>>(8*i))&0xFF));
}

static CodeBlob codegen_x64(const IR_Module& m) {
  CodeBlob b;
  b.entry_offset = 0;

  auto& c = b.code;

  // Prologue: push rbp; mov rbp,rsp; sub rsp, frame
  // frame = locals*8 + spill (align 16)
  int32_t nlocals = (int32_t)m.main.locals.size();
  int32_t frame = std::max(16, ((nlocals * 8 + 15) / 16) * 16);

  // push rbp
  emit_u8(c, 0x55);
  // mov rbp, rsp
  emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xE5);
  // sub rsp, imm32
  emit_u8(c, 0x48); emit_u8(c, 0x81); emit_u8(c, 0xEC); emit_u32(c, (uint32_t)frame);

  auto local_disp = [&](int32_t slot)->int32_t {
    // [rbp - 8*(slot+1)]
    return -8 * (slot + 1);
  };

  auto emit_mov_rax_imm64 = [&](uint64_t v){
    emit_u8(c, 0x48); emit_u8(c, 0xB8); emit_u64(c, v); // mov rax, imm64
  };

  auto emit_push_rax = [&](){ emit_u8(c, 0x50); };
  auto emit_pop_rax  = [&](){ emit_u8(c, 0x58); };
  auto emit_pop_rbx  = [&](){ emit_u8(c, 0x5B); };

  auto emit_store_local_from_rax = [&](int32_t slot){
    // mov [rbp+disp32], rax  => 48 89 85 disp32
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0x85); emit_u32(c, (uint32_t)local_disp(slot));
  };

  auto emit_load_local_to_rax = [&](int32_t slot){
    // mov rax, [rbp+disp32] => 48 8B 85 disp32
    emit_u8(c, 0x48); emit_u8(c, 0x8B); emit_u8(c, 0x85); emit_u32(c, (uint32_t)local_disp(slot));
  };

  auto emit_add_rax_rbx = [&](){
    // add rax, rbx => 48 01 D8
    emit_u8(c, 0x48); emit_u8(c, 0x01); emit_u8(c, 0xD8);
  };
  auto emit_sub_rbx_rax_into_rax = [&](){
    // We want (lhs - rhs). Our stack emits lhs then rhs, so after pops:
    // rax = rhs, rbx = lhs. Do: sub rbx, rax; mov rax, rbx
    emit_u8(c, 0x48); emit_u8(c, 0x29); emit_u8(c, 0xC3); // sub rbx, rax
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xD8); // mov rax, rbx
  };
  auto emit_mul_rbx_rax_into_rax = [&](){
    // imul rbx, rax ; mov rax, rbx
    emit_u8(c, 0x48); emit_u8(c, 0x0F); emit_u8(c, 0xAF); emit_u8(c, 0xD8); // imul rbx, rax
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xD8); // mov rax, rbx
  };
  auto emit_div_rbx_by_rax_into_rax = [&](){
    // want lhs/rhs with rbx=lhs, rax=rhs
    // mov rax,rhs already, need dividend in rax with sign extend:
    // mov rax, rbx; cqo; idiv r??? (rhs in rcx)
    // We'll move rhs to rcx, lhs to rax.
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xC1); // mov rcx, rax
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xD8); // mov rax, rbx
    emit_u8(c, 0x48); emit_u8(c, 0x99);                   // cqo
    emit_u8(c, 0x48); emit_u8(c, 0xF7); emit_u8(c, 0xF9); // idiv rcx
    // quotient in rax
  };

  auto emit_call_print_i64 = [&](){
    // move arg from stack-top value currently in rax
#if defined(_WIN32)
    // Windows x64: first arg in RCX
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xC1); // mov rcx, rax
#else
    // SysV: first arg in RDI
    emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xC7); // mov rdi, rax
#endif
    // mov rax, imm64; call rax
    emit_mov_rax_imm64((uint64_t)(uintptr_t)&rane_host_print_i64);
    emit_u8(c, 0xFF); emit_u8(c, 0xD0); // call rax
  };

  // Codegen entry block only (this layer)
  auto const& entry = m.main.blocks.front();
  for (auto const& in : entry.insts) {
    switch (in.op) {
      case IR_Op::ConstI64:
        emit_mov_rax_imm64((uint64_t)in.a);
        emit_push_rax();
        break;

      case IR_Op::LoadLocal:
        emit_load_local_to_rax((int32_t)in.a);
        emit_push_rax();
        break;

      case IR_Op::StoreLocal:
        emit_pop_rax();
        emit_store_local_from_rax((int32_t)in.a);
        break;

      case IR_Op::AddI64:
        emit_pop_rax(); // rhs
        emit_pop_rbx(); // lhs
        // rbx + rax -> rax
        emit_add_rax_rbx(); // rax = rax + rbx (but currently rax=rhs, rbx=lhs) => add rax, rbx works
        emit_push_rax();
        break;

      case IR_Op::SubI64:
        emit_pop_rax(); // rhs
        emit_pop_rbx(); // lhs
        emit_sub_rbx_rax_into_rax();
        emit_push_rax();
        break;

      case IR_Op::MulI64:
        emit_pop_rax();
        emit_pop_rbx();
        emit_mul_rbx_rax_into_rax();
        emit_push_rax();
        break;

      case IR_Op::DivI64:
        emit_pop_rax();
        emit_pop_rbx();
        emit_div_rbx_by_rax_into_rax();
        emit_push_rax();
        break;

      case IR_Op::CallPrintI64:
        emit_pop_rax();
        emit_call_print_i64();
        break;

      case IR_Op::RetI32: {
        // ret value in eax
        // clean stack: ignore operand stack by resetting rsp to rbp-frame base
        // mov eax, imm32
        emit_u8(c, 0xB8); emit_u32(c, (uint32_t)(int32_t)in.a);
        // epilogue: mov rsp,rbp; pop rbp; ret
        emit_u8(c, 0x48); emit_u8(c, 0x89); emit_u8(c, 0xEC); // mov rsp, rbp
        emit_u8(c, 0x5D); // pop rbp
        emit_u8(c, 0xC3); // ret
        break;
      }
    }
  }

  return b;
}

//------------------------------------------------------------------------------
// Executor metadata: binary + JSON mirror
//------------------------------------------------------------------------------

#pragma pack(push, 1)
struct ExecMetaBinHeader {
  uint32_t magic = 0x4D455845; // 'EXEM'
  uint16_t version = 1;
  uint16_t reserved = 0;
  uint32_t entry_offset = 0;
  uint32_t code_size = 0;
  uint32_t guard_count = 0;
  uint32_t cap_count = 0;
};
#pragma pack(pop)

static void write_file_bytes(const std::string& path, const std::vector<uint8_t>& bytes) {
  std::ofstream f(path, std::ios::binary);
  if (!f) die({DiagCode::InternalError, {1,1,0}, "failed to open output file: " + path});
  f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
}

static void write_text(const std::string& path, const std::string& s) {
  std::ofstream f(path, std::ios::binary);
  if (!f) die({DiagCode::InternalError, {1,1,0}, "failed to open output file: " + path});
  f << s;
}

static void emit_exec_meta(const std::string& base, const CodeBlob& blob, const CiamCtx& ctx) {
  std::vector<uint8_t> bin;
  ExecMetaBinHeader h;
  h.entry_offset = blob.entry_offset;
  h.code_size = (uint32_t)blob.code.size();
  h.guard_count = (uint32_t)ctx.guards.size();
  h.cap_count = (uint32_t)ctx.required_caps.size();

  bin.resize(sizeof(h));
  std::memcpy(bin.data(), &h, sizeof(h));

  for (auto const& g : ctx.guards) {
    uint16_t kind = (uint16_t)g.kind;
    uint16_t pad = 0;
    uint32_t anchor = g.anchor_tok;
    uint32_t line = g.span.line, col = g.span.col, len = g.span.len;
    auto push = [&](auto v) {
      uint8_t* p = (uint8_t*)&v;
      bin.insert(bin.end(), p, p + sizeof(v));
    };
    push(kind); push(pad); push(anchor); push(line); push(col); push(len);
  }

  for (auto cap : ctx.required_caps) {
    uint16_t c = (uint16_t)cap;
    uint16_t pad = 0;
    uint8_t* p1 = (uint8_t*)&c;
    uint8_t* p2 = (uint8_t*)&pad;
    bin.insert(bin.end(), p1, p1 + 2);
    bin.insert(bin.end(), p2, p2 + 2);
  }

  write_file_bytes(base + ".bin", bin);

  std::ostringstream js;
  js << "{\n";
  js << "  \"version\": 1,\n";
  js << "  \"entry_offset\": " << blob.entry_offset << ",\n";
  js << "  \"code_size\": " << blob.code.size() << ",\n";
  js << "  \"guards\": [\n";
  for (size_t i=0;i<ctx.guards.size();i++) {
    auto const& g = ctx.guards[i];
    js << "    {\"kind\": " << (uint16_t)g.kind
       << ", \"anchor_tok\": " << g.anchor_tok
       << ", \"span\": {\"line\": " << g.span.line
       << ", \"col\": " << g.span.col
       << ", \"len\": " << g.span.len << "}}";
    js << (i+1<ctx.guards.size()? ",":"") << "\n";
  }
  js << "  ],\n";
  js << "  \"required_caps\": [";
  for (size_t i=0;i<ctx.required_caps.size();i++) {
    js << (uint16_t)ctx.required_caps[i];
    if (i+1<ctx.required_caps.size()) js << ", ";
  }
  js << "]\n";
  js << "}\n";
  write_text(base + ".json", js.str());
}

//------------------------------------------------------------------------------
// Executable memory + invocation
//------------------------------------------------------------------------------

struct ExecMemory { void* ptr=nullptr; size_t size=0; };

static ExecMemory alloc_exec_memory(size_t size) {
  ExecMemory m; m.size = size;
#if defined(_WIN32)
  void* p = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!p) die({DiagCode::InternalError, {1,1,0}, "VirtualAlloc failed"});
  m.ptr = p;
#else
  void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == MAP_FAILED) die({DiagCode::InternalError, {1,1,0}, "mmap failed"});
  m.ptr = p;
#endif
  return m;
}

static void seal_exec_memory(ExecMemory& m) {
#if defined(_WIN32)
  DWORD oldProt = 0;
  if (!VirtualProtect(m.ptr, m.size, PAGE_EXECUTE_READ, &oldProt))
    die({DiagCode::InternalError, {1,1,0}, "VirtualProtect RX failed"});
#else
  if (mprotect(m.ptr, m.size, PROT_READ | PROT_EXEC) != 0)
    die({DiagCode::InternalError, {1,1,0}, "mprotect RX failed"});
#endif
}

static void free_exec_memory(ExecMemory& m) {
#if defined(_WIN32)
  if (m.ptr) VirtualFree(m.ptr, 0, MEM_RELEASE);
#else
  if (m.ptr) munmap(m.ptr, m.size);
#endif
  m.ptr=nullptr; m.size=0;
}

using MainFn = int (*)();

static int executor_run_main(const CodeBlob& blob) {
  ExecMemory mem = alloc_exec_memory(blob.code.size());
  std::memcpy(mem.ptr, blob.code.data(), blob.code.size());
  seal_exec_memory(mem);
  auto* entry = (uint8_t*)mem.ptr + blob.entry_offset;
  int rc = ((MainFn)entry)();
  free_exec_memory(mem);
  return rc;
}

//------------------------------------------------------------------------------
// Driver
//------------------------------------------------------------------------------

static std::string slurp_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) die({DiagCode::InternalError, {1,1,0}, "cannot open: " + path});
  std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: rane_resolver <input.rane>\n";
    return 2;
  }

  std::string src = slurp_file(argv[1]);

  // 1) Lex
  Lexer lx(src);
  auto toks = lx.lex_all();

  // 2) Parse
  Parser ps(std::move(toks));
  Unit unit = ps.parse_unit();

  // 3) CIAM pass: desugar + emit syntax.ciam.rane
  CiamCtx ciam;
  std::vector<CiamArtifact> artifacts;
  if (!ciam_pass_run(ciam, unit, artifacts)) {
    if (!ciam.diags.empty()) die(ciam.diags.front());
    die({DiagCode::InternalError, {1,1,0}, "CIAM pass failed"});
  }

  // Write syntax.ciam.rane
  for (auto const& a : artifacts) {
    if (a.name == "syntax.ciam.rane") {
      write_file_bytes(a.name, a.bytes);
    }
  }

  // 4) Lower to IR
  IR_Module irm{};
  if (!lower_ast_to_ir(ciam, unit, irm)) die({DiagCode::InternalError, {1,1,0}, "lowering failed"});

  // 5) Optimize IR
  optimize_ir(irm);

  // 6) Emit syntax.opt.ciam.ir
  write_text("syntax.opt.ciam.ir", ir_prettyprint(irm));

  // 7) Codegen
  CodeBlob blob = codegen_x64(irm);

  // 8) Exec meta
  emit_exec_meta("syntax.exec.meta", blob, ciam);

  // 9) Execute
  int rc = executor_run_main(blob);
  std::cout << "executor: main() returned " << rc << "\n";
  return 0;
}
