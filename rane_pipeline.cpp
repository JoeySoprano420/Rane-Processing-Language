// rane_pipeline.cpp (C++20)
// End-to-end: syntax.rane or Typed_Common_Intermediary_Language.rane
//   -> Lexer (CIAM token shaping)
//   -> Parser (CIAM grammar rewrites + sugar)
//   -> AST
//   -> Resolver (names/scopes/caps + basic types)
//   -> Typed CIL (CIAM semantic insertion)
//   -> OSW optimizer (CIAM shaping)
//   -> Frame planner (Win64 ABI, shadow space, align)
//   -> x64 emission + minimal PE writer
//
// This is intentionally coverage-first, structured to be grown into your full backend.

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

////////////////////////////////////////////////////////////////
// Diagnostics
////////////////////////////////////////////////////////////////
enum class DiagCode : uint32_t {
  OK = 0,
  LEX_ERROR,
  PARSE_ERROR,
  UNEXPECTED_TOKEN,
  UNDEFINED_NAME,
  REDECLARED_NAME,
  TYPE_MISMATCH,
  CAPABILITY_VIOLATION,
  INTERNAL_ERROR,
  NOT_IMPLEMENTED,
};

struct Span {
  uint32_t line = 1;
  uint32_t col = 1;
  uint32_t len = 1;
};

struct Diag {
  DiagCode code = DiagCode::OK;
  Span span{};
  std::string message;
};

static Diag make_diag(DiagCode c, Span s, std::string m) {
  return Diag{c, s, std::move(m)};
}

////////////////////////////////////////////////////////////////
// CIAM hooks: the "interpret surface intent" layer
////////////////////////////////////////////////////////////////
struct CIAMContext {
  bool surface_mode = true; // true: indent syntax.rane, false: canonical CIL
  std::unordered_set<std::string> enabled;
};

struct CIAM {
  // Token shaping: e.g., turn indentation/newlines into block tokens; rewrite `xor` into `^`, etc.
  static void shape_tokens(CIAMContext&, std::vector<struct Token>&) {}

  // Grammar rewriting + sugar lowering insertion: with/defer/lock, match expansions, etc.
  static void rewrite_ast(CIAMContext&, struct ASTModule&) {}

  // Semantic insertion + inference: add implicit try/finally for defer, add capability checks, etc.
  static void semantic_insert(CIAMContext&, struct ASTModule&) {}

  // Typed rewrites + lowering decisions (AST->TypedCIL decisions)
  static void decide_lowering(CIAMContext&) {}

  // Optimization + backend shaping
  static void osw_shape(CIAMContext&, struct class OSWProgram&) {}
};

////////////////////////////////////////////////////////////////
// Lexer / Tokenizer
////////////////////////////////////////////////////////////////
enum class TokKind : uint16_t {
  End,
  Ident,
  IntLit,
  FloatLit,
  StringLit,

  // Punctuation / operators
  LParen, RParen, LBrace, RBrace, LBrack, RBrack,
  Comma, Colon, Semi, Dot,
  Arrow, Assign,
  Plus, Minus, Star, Slash, Percent,
  Amp, Pipe, Caret, Tilde, Bang,
  Lt, Le, Gt, Ge, EqEq, Ne,
  AndAnd, OrOr,
  Question,

  // Keywords (subset; expand freely)
  KwImport, KwModule, KwNamespace,
  KwProc, KwExport, KwInline, KwPublic, KwPrivate, KwProtected, KwAdmin,
  KwReturn, KwLet, KwIf, KwElse, KwFor, KwWhile,
  KwType, KwTypealias, KwAlias,
  KwConst, KwConstexpr, KwConstinit, KwConsteval,
  KwStruct, KwEnum, KwUnion, KwVariant,
  KwCapability, KwRequires,
  KwMatch, KwCase, KwDefault, KwSwitch, KwDecide,
  KwAsync, KwAwait, KwDedicate,
  KwWith, KwAs, KwDefer, KwTry, KwCatch, KwFinally, KwThrow,
  KwMacro, KwTemplate,
  KwMutex, KwChannel, KwSend, KwRecv, KwJoin, KwSpawn, KwLock,
  KwMMIO, KwRegion, KwFrom, KwSize,
  KwRead32, KwWrite32,
  KwAddr, KwLoad, KwStore,
  KwNull, KwTrue, KwFalse,
  KwTrap, KwHalt, KwGoto, KwLabel,

  // Indentation structure (surface mode)
  Newline, Indent, Dedent,
};

struct Token {
  TokKind kind{};
  Span span{};
  std::string text; // for Ident/StringLit; for numbers keep original too
};

static bool is_alpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
static bool is_digit(char c){ return (c>='0'&&c<='9');}
static bool is_alnum(char c){ return is_alpha(c)||is_digit(c);}

static std::unordered_map<std::string, TokKind> kKeywords = {
  {"import",TokKind::KwImport},{"module",TokKind::KwModule},{"namespace",TokKind::KwNamespace},
  {"proc",TokKind::KwProc},{"export",TokKind::KwExport},{"inline",TokKind::KwInline},
  {"public",TokKind::KwPublic},{"private",TokKind::KwPrivate},{"protected",TokKind::KwProtected},{"admin",TokKind::KwAdmin},
  {"return",TokKind::KwReturn},{"let",TokKind::KwLet},{"if",TokKind::KwIf},{"else",TokKind::KwElse},
  {"for",TokKind::KwFor},{"while",TokKind::KwWhile},
  {"type",TokKind::KwType},{"typealias",TokKind::KwTypealias},{"alias",TokKind::KwAlias},
  {"const",TokKind::KwConst},{"constexpr",TokKind::KwConstexpr},{"constinit",TokKind::KwConstinit},{"consteval",TokKind::KwConsteval},
  {"struct",TokKind::KwStruct},{"enum",TokKind::KwEnum},{"union",TokKind::KwUnion},{"variant",TokKind::KwVariant},
  {"capability",TokKind::KwCapability},{"requires",TokKind::KwRequires},
  {"match",TokKind::KwMatch},{"case",TokKind::KwCase},{"default",TokKind::KwDefault},
  {"switch",TokKind::KwSwitch},{"decide",TokKind::KwDecide},
  {"async",TokKind::KwAsync},{"await",TokKind::KwAwait},{"dedicate",TokKind::KwDedicate},
  {"with",TokKind::KwWith},{"as",TokKind::KwAs},{"defer",TokKind::KwDefer},{"try",TokKind::KwTry},
  {"catch",TokKind::KwCatch},{"finally",TokKind::KwFinally},{"throw",TokKind::KwThrow},
  {"macro",TokKind::KwMacro},{"template",TokKind::KwTemplate},
  {"mutex",TokKind::KwMutex},{"channel",TokKind::KwChannel},{"send",TokKind::KwSend},{"recv",TokKind::KwRecv},
  {"join",TokKind::KwJoin},{"spawn",TokKind::KwSpawn},{"lock",TokKind::KwLock},
  {"mmio",TokKind::KwMMIO},{"region",TokKind::KwRegion},{"from",TokKind::KwFrom},{"size",TokKind::KwSize},
  {"read32",TokKind::KwRead32},{"write32",TokKind::KwWrite32},
  {"addr",TokKind::KwAddr},{"load",TokKind::KwLoad},{"store",TokKind::KwStore},
  {"null",TokKind::KwNull},{"true",TokKind::KwTrue},{"false",TokKind::KwFalse},
  {"trap",TokKind::KwTrap},{"halt",TokKind::KwHalt},{"goto",TokKind::KwGoto},{"label",TokKind::KwLabel},
};

struct LexResult {
  std::vector<Token> toks;
  std::vector<Diag> diags;
};

static void push_tok(std::vector<Token>& t, TokKind k, Span s, std::string txt="") {
  t.push_back(Token{k,s,std::move(txt)});
}

// Surface lexer: emits Newline/Indent/Dedent
static LexResult lex_surface(std::string_view src) {
  LexResult r;
  uint32_t line=1,col=1;
  size_t i=0;
  std::vector<int> indent_stack{0};
  auto cur = [&]()->char{ return i<src.size()?src[i]:'\0'; };
  auto adv = [&](){ if(i<src.size()){ if(src[i]=='\n'){line++;col=1;} else col++; i++; } };

  auto emit_newline = [&](){
    push_tok(r.toks, TokKind::Newline, Span{line,col,1});
  };

  while(i<src.size()){
    char c = cur();

    // comments (#...)
    if(c=='#'){
      while(i<src.size() && cur()!='\n') adv();
      continue;
    }

    if(c=='\r'){ adv(); continue; }

    // newline -> Newline + indentation measurement
    if(c=='\n'){
      adv();
      emit_newline();

      // measure indentation of next line
      int spaces=0;
      while(i<src.size()){
        char d=cur();
        if(d==' ') { spaces++; adv(); continue; }
        if(d=='\t'){ spaces+=4; adv(); continue; } // normalize
        break;
      }
      // ignore blank lines / comment-only
      if(i<src.size() && (cur()=='\n' || cur()=='#')) continue;

      int prev = indent_stack.back();
      if(spaces > prev){
        indent_stack.push_back(spaces);
        push_tok(r.toks, TokKind::Indent, Span{line,col,1});
      } else if(spaces < prev){
        while(!indent_stack.empty() && spaces < indent_stack.back()){
          indent_stack.pop_back();
          push_tok(r.toks, TokKind::Dedent, Span{line,col,1});
        }
        if(indent_stack.empty() || spaces != indent_stack.back()){
          r.diags.push_back(make_diag(DiagCode::LEX_ERROR, Span{line,col,1}, "Indentation error"));
          break;
        }
      }
      continue;
    }

    // whitespace
    if(c==' '||c=='\t'){ adv(); continue; }

    // string
    if(c=='"'){
      Span s{line,col,1};
      adv();
      std::string out;
      while(i<src.size() && cur()!='"'){
        if(cur()=='\\'){ adv(); if(i>=src.size()) break; char e=cur(); // simplistic escapes
          if(e=='n') out.push_back('\n'); else out.push_back(e);
          adv();
        } else { out.push_back(cur()); adv(); }
      }
      if(cur()=='"') adv();
      push_tok(r.toks, TokKind::StringLit, s, out);
      continue;
    }

    // number
    if(is_digit(c)){
      Span s{line,col,1};
      std::string num;
      bool is_float=false;
      while(i<src.size() && (is_digit(cur())||cur()=='_'||cur()=='.'||cur()=='x'||cur()=='b'||(cur()>='A'&&cur()<='F')||(cur()>='a'&&cur()<='f'))){
        if(cur()=='.') is_float=true;
        num.push_back(cur());
        adv();
      }
      push_tok(r.toks, is_float?TokKind::FloatLit:TokKind::IntLit, s, num);
      continue;
    }

    // ident / keyword
    if(is_alpha(c)){
      Span s{line,col,1};
      std::string id;
      while(i<src.size() && is_alnum(cur())){ id.push_back(cur()); adv(); }
      auto it = kKeywords.find(id);
      if(it!=kKeywords.end()) push_tok(r.toks, it->second, s, id);
      else push_tok(r.toks, TokKind::Ident, s, id);
      continue;
    }

    // operators/punct
    Span s{line,col,1};
    auto two = [&](char a,char b, TokKind k)->bool{
      if(i+1<src.size() && src[i]==a && src[i+1]==b){ adv(); adv(); push_tok(r.toks,k,s); return true; }
      return false;
    };
    if(two('-','>',TokKind::Arrow)) continue;
    if(two('=','=',TokKind::EqEq)) continue;
    if(two('!','=',TokKind::Ne)) continue;
    if(two('<','=',TokKind::Le)) continue;
    if(two('>','=',TokKind::Ge)) continue;
    if(two('&','&',TokKind::AndAnd)) continue;
    if(two('|','|',TokKind::OrOr)) continue;

    switch(c){
      case '(': adv(); push_tok(r.toks,TokKind::LParen,s); break;
      case ')': adv(); push_tok(r.toks,TokKind::RParen,s); break;
      case '[': adv(); push_tok(r.toks,TokKind::LBrack,s); break;
      case ']': adv(); push_tok(r.toks,TokKind::RBrack,s); break;
      case ',': adv(); push_tok(r.toks,TokKind::Comma,s); break;
      case ':': adv(); push_tok(r.toks,TokKind::Colon,s); break;
      case '.': adv(); push_tok(r.toks,TokKind::Dot,s); break;
      case '+': adv(); push_tok(r.toks,TokKind::Plus,s); break;
      case '-': adv(); push_tok(r.toks,TokKind::Minus,s); break;
      case '*': adv(); push_tok(r.toks,TokKind::Star,s); break;
      case '/': adv(); push_tok(r.toks,TokKind::Slash,s); break;
      case '%': adv(); push_tok(r.toks,TokKind::Percent,s); break;
      case '&': adv(); push_tok(r.toks,TokKind::Amp,s); break;
      case '|': adv(); push_tok(r.toks,TokKind::Pipe,s); break;
      case '^': adv(); push_tok(r.toks,TokKind::Caret,s); break;
      case '~': adv(); push_tok(r.toks,TokKind::Tilde,s); break;
      case '!': adv(); push_tok(r.toks,TokKind::Bang,s); break;
      case '<': adv(); push_tok(r.toks,TokKind::Lt,s); break;
      case '>': adv(); push_tok(r.toks,TokKind::Gt,s); break;
      case '=': adv(); push_tok(r.toks,TokKind::Assign,s); break;
      case '?': adv(); push_tok(r.toks,TokKind::Question,s); break;
      default:
        r.diags.push_back(make_diag(DiagCode::LEX_ERROR, s, std::string("Unexpected char: ") + c));
        adv();
        break;
    }
  }

  // close dedents
  while(indent_stack.size()>1){
    indent_stack.pop_back();
    push_tok(r.toks, TokKind::Dedent, Span{line,col,1});
  }
  push_tok(r.toks, TokKind::End, Span{line,col,1});
  return r;
}

// Canonical lexer: braces/semicolon language; Newline ignored
static LexResult lex_canonical(std::string_view src) {
  // We can reuse surface lexer and just ignore Indent/Dedent/Newline by not producing them.
  // Simpler: run surface lexer with indentation disabled by treating newline as whitespace.
  // Here: implement a small canonical lexer by taking surface and filtering indentation tokens.
  LexResult tmp = lex_surface(src);
  LexResult out;
  out.diags = std::move(tmp.diags);
  for(auto& t : tmp.toks){
    if(t.kind==TokKind::Indent||t.kind==TokKind::Dedent||t.kind==TokKind::Newline) continue;
    out.toks.push_back(std::move(t));
  }
  return out;
}

////////////////////////////////////////////////////////////////
// AST
////////////////////////////////////////////////////////////////
enum class ASTKind : uint16_t {
  Module,
  Import,
  Namespace,
  Proc,
  Block,
  Let,
  Return,
  If,
  Call,
  Ident,
  Int,
  String,
  Match,
  VariantDecl,
  StructDecl,
  EnumDecl,
  UnionDecl,
};

struct TypeRef {
  std::string name;              // "i64", "Maybe", "Result"
  std::vector<TypeRef> args;     // generics
};

struct ASTNode {
  ASTKind kind{};
  Span span{};
  virtual ~ASTNode() = default;
};

using ASTPtr = std::unique_ptr<ASTNode>;

struct ASTExpr : ASTNode {};
struct ASTStmt : ASTNode {};

struct ASTIdent : ASTExpr {
  std::string name;
  ASTIdent(Span s, std::string n){ kind=ASTKind::Ident; span=s; name=std::move(n); }
};

struct ASTInt : ASTExpr {
  int64_t value{};
  ASTInt(Span s, int64_t v){ kind=ASTKind::Int; span=s; value=v; }
};

struct ASTString : ASTExpr {
  std::string value;
  ASTString(Span s, std::string v){ kind=ASTKind::String; span=s; value=std::move(v); }
};

struct ASTCall : ASTExpr {
  std::string callee;
  std::vector<std::unique_ptr<ASTExpr>> args;
  ASTCall(Span s, std::string c){ kind=ASTKind::Call; span=s; callee=std::move(c); }
};

struct ASTLet : ASTStmt {
  std::string name;
  std::optional<TypeRef> annotated;
  std::unique_ptr<ASTExpr> init;
  ASTLet(Span s){ kind=ASTKind::Let; span=s; }
};

struct ASTReturn : ASTStmt {
  std::unique_ptr<ASTExpr> value;
  ASTReturn(Span s){ kind=ASTKind::Return; span=s; }
};

struct ASTIf : ASTStmt {
  std::unique_ptr<ASTExpr> cond;
  std::vector<std::unique_ptr<ASTStmt>> thenStmts;
  std::vector<std::unique_ptr<ASTStmt>> elseStmts;
  ASTIf(Span s){ kind=ASTKind::If; span=s; }
};

struct ASTMatch : ASTStmt {
  std::unique_ptr<ASTExpr> scrutinee;
  struct Case {
    std::string tag;                 // "Some", "None", "Data", "Three"
    std::vector<std::string> binds;  // payload binding names
    std::vector<std::unique_ptr<ASTStmt>> body;
    Span span{};
  };
  std::vector<Case> cases;
  ASTMatch(Span s){ kind=ASTKind::Match; span=s; }
};

struct ASTProc : ASTNode {
  std::string name;
  std::vector<std::pair<std::string, TypeRef>> params;
  TypeRef ret;
  bool is_public=false, is_private=false, is_export=false, is_inline=false, is_async=false, is_dedicate=false;
  std::unordered_set<std::string> requires_caps;
  std::vector<std::unique_ptr<ASTStmt>> body;
  ASTProc(Span s){ kind=ASTKind::Proc; span=s; }
};

struct ASTImport : ASTNode {
  std::string path;
  ASTImport(Span s, std::string p){ kind=ASTKind::Import; span=s; path=std::move(p); }
};

struct ASTNamespace : ASTNode {
  std::string name;
  std::vector<std::unique_ptr<ASTProc>> procs;
  ASTNamespace(Span s, std::string n){ kind=ASTKind::Namespace; span=s; name=std::move(n); }
};

struct ASTVariantDecl : ASTNode {
  std::string name;
  std::vector<std::string> type_params;
  struct Tag { std::string name; std::vector<TypeRef> payload; };
  std::vector<Tag> tags;
  ASTVariantDecl(Span s){ kind=ASTKind::VariantDecl; span=s; }
};

struct ASTModule : ASTNode {
  std::string name;
  std::vector<std::unique_ptr<ASTImport>> imports;
  std::vector<std::unique_ptr<ASTNamespace>> namespaces;
  std::vector<std::unique_ptr<ASTProc>> procs;
  std::vector<std::unique_ptr<ASTVariantDecl>> variants;
  std::unordered_set<std::string> declared_caps;
  ASTModule(){ kind=ASTKind::Module; }
};

////////////////////////////////////////////////////////////////
// Parser (subset, but wired to CIAM points)
////////////////////////////////////////////////////////////////
struct Parser {
  CIAMContext& ciam;
  std::vector<Token>& t;
  size_t at=0;
  std::vector<Diag> diags;

  Parser(CIAMContext& c, std::vector<Token>& toks) : ciam(c), t(toks) {}

  Token& peek(){ return t[at]; }
  bool is(TokKind k){ return peek().kind==k; }
  Token& prev(){ return t[at-1]; }

  bool accept(TokKind k){
    if(is(k)){ at++; return true; }
    return false;
  }

  bool expect(TokKind k, const char* msg){
    if(accept(k)) return true;
    diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, msg));
    return false;
  }

  // Helpers
  bool eat_stmt_sep(){
    if(ciam.surface_mode){
      // in surface mode, statements often end at Newline or Dedent boundary;
      // we already emit Newline tokens, so accept any number here.
      bool any=false;
      while(accept(TokKind::Newline)) any=true;
      return any;
    } else {
      return accept(TokKind::Semi);
    }
  }

  std::optional<std::string> parse_ident(){
    if(is(TokKind::Ident)){ auto s=peek().text; at++; return s; }
    return std::nullopt;
  }

  TypeRef parse_type(){
    // very small type parser: Ident [ "<" type ("," type)* ">" ] or canonical uses angle? we skip angle for now.
    // Your canonical form uses "Maybe<i64>" so we parse "<...>".
    TypeRef tr;
    if(is(TokKind::Ident)){
      tr.name = peek().text; at++;
    } else {
      // fallback: keyword type tokens are still in Ident in this lexer; keep it simple.
      tr.name = "<?>"; diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected type name"));
      return tr;
    }
    if(accept(TokKind::Lt)){
      // Not lexed as '<' in our lexer in canonical filtering? It is. Good.
      while(true){
        tr.args.push_back(parse_type());
        if(accept(TokKind::Comma)) continue;
        break;
      }
      expect(TokKind::Gt, "Expected '>'");
    }
    return tr;
  }

  std::unique_ptr<ASTExpr> parse_primary(){
    if(is(TokKind::IntLit)){
      auto s=peek().span;
      std::string txt=peek().text; at++;
      // parse only decimal here; hex/bin allowed later
      int64_t v=0;
      try{ v = std::stoll(txt, nullptr, 0); } catch(...) {}
      return std::make_unique<ASTInt>(s,v);
    }
    if(is(TokKind::StringLit)){
      auto s=peek().span;
      std::string v=peek().text; at++;
      return std::make_unique<ASTString>(s,std::move(v));
    }
    if(is(TokKind::KwTrue)){ auto s=peek().span; at++; return std::make_unique<ASTInt>(s,1); }
    if(is(TokKind::KwFalse)){ auto s=peek().span; at++; return std::make_unique<ASTInt>(s,0); }
    if(is(TokKind::KwNull)){ auto s=peek().span; at++; return std::make_unique<ASTInt>(s,0); } // null modeled as 0 for now

    if(is(TokKind::Ident)){
      auto s=peek().span;
      std::string id=peek().text; at++;
      // call? in surface: "print x" looks like ident + args without parens.
      // in canonical: "print(x)".
      if(ciam.surface_mode){
        // greedy parse args until newline/colon/dedent/end
        auto call = std::make_unique<ASTCall>(s, id);
        while(!(is(TokKind::Newline)||is(TokKind::Colon)||is(TokKind::Dedent)||is(TokKind::End)||is(TokKind::Semi))){
          // stop on block open
          if(is(TokKind::Indent)) break;
          // allow commas in canonical-ish
          if(accept(TokKind::Comma)) continue;
          call->args.push_back(parse_primary());
        }
        if(!call->args.empty()) return call;
        return std::make_unique<ASTIdent>(s, id);
      } else {
        if(accept(TokKind::LParen)){
          auto call = std::make_unique<ASTCall>(s, id);
          if(!accept(TokKind::RParen)){
            while(true){
              call->args.push_back(parse_primary());
              if(accept(TokKind::Comma)) continue;
              expect(TokKind::RParen, "Expected ')'");
              break;
            }
          }
          return call;
        }
        return std::make_unique<ASTIdent>(s, id);
      }
    }

    diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected expression"));
    // recover
    at++;
    return std::make_unique<ASTInt>(Span{},0);
  }

  // Very small expression layer: primary only (extend later with precedence table)
  std::unique_ptr<ASTExpr> parse_expr(){
    return parse_primary();
  }

  std::unique_ptr<ASTStmt> parse_stmt(){
    // let
    if(accept(TokKind::KwLet)){
      auto s = prev().span;
      auto st = std::make_unique<ASTLet>(s);
      auto nm = parse_ident();
      if(!nm){ diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected identifier after let")); return st; }
      st->name = *nm;

      // canonical typed: let x: i64 = ...
      if(!ciam.surface_mode && accept(TokKind::Colon)){
        st->annotated = parse_type();
      } else if(ciam.surface_mode){
        // surface: let x i64 = ...
        if(is(TokKind::Ident) && peek().text!="="){
          // heuristic: if next token is an ident and after that Assign occurs, treat as type
          size_t save=at;
          if(is(TokKind::Ident)){
            std::string tn=peek().text;
            at++;
            if(is(TokKind::Assign)){
              st->annotated = TypeRef{tn,{}};
            } else {
              at = save; // not a type
            }
          }
        }
      }

      expect(TokKind::Assign, "Expected '=' in let");
      st->init = parse_expr();
      eat_stmt_sep();
      return st;
    }

    // return
    if(accept(TokKind::KwReturn)){
      auto s = prev().span;
      auto st = std::make_unique<ASTReturn>(s);
      st->value = parse_expr();
      eat_stmt_sep();
      return st;
    }

    // if
    if(accept(TokKind::KwIf)){
      auto s = prev().span;
      auto st = std::make_unique<ASTIf>(s);
      st->cond = parse_expr();

      if(ciam.surface_mode){
        expect(TokKind::Colon, "Expected ':' after if condition");
        expect(TokKind::Newline, "Expected newline after ':'");
        expect(TokKind::Indent, "Expected indent");
        while(!is(TokKind::Dedent) && !is(TokKind::End)){
          st->thenStmts.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
        }
        expect(TokKind::Dedent, "Expected dedent");
        // optional else:
        if(accept(TokKind::KwElse)){
          expect(TokKind::Colon, "Expected ':' after else");
          expect(TokKind::Newline, "Expected newline");
          expect(TokKind::Indent, "Expected indent");
          while(!is(TokKind::Dedent) && !is(TokKind::End)){
            st->elseStmts.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
          }
          expect(TokKind::Dedent, "Expected dedent after else");
        }
      } else {
        expect(TokKind::LBrace, "Expected '{' for if");
        while(!accept(TokKind::RBrace) && !is(TokKind::End)){
          st->thenStmts.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
        }
        if(accept(TokKind::KwElse)){
          expect(TokKind::LBrace, "Expected '{' for else");
          while(!accept(TokKind::RBrace) && !is(TokKind::End)){
            st->elseStmts.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
          }
        }
      }
      return st;
    }

    // match (subset: match expr: case TAG [binds]: ... end)
    if(accept(TokKind::KwMatch)){
      auto s = prev().span;
      auto st = std::make_unique<ASTMatch>(s);
      st->scrutinee = parse_expr();

      if(ciam.surface_mode){
        expect(TokKind::Colon, "Expected ':' after match");
        expect(TokKind::Newline, "Expected newline");
        expect(TokKind::Indent, "Expected indent");

        while(!is(TokKind::Dedent) && !is(TokKind::End)){
          if(accept(TokKind::KwCase)){
            ASTMatch::Case cs;
            cs.span = prev().span;
            // tag name
            if(is(TokKind::Ident)){ cs.tag = peek().text; at++; }
            else { diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected case tag")); }

            // optional bind: "case Some x:" or "case Data d:"
            if(is(TokKind::Ident)){
              // bind symbol(s) until ':'
              while(is(TokKind::Ident)){
                cs.binds.push_back(peek().text); at++;
              }
            }

            expect(TokKind::Colon, "Expected ':' after case");
            // body is a single-line stmt or indented block (support both)
            if(accept(TokKind::Newline)){
              expect(TokKind::Indent, "Expected indent for case body");
              while(!is(TokKind::Dedent) && !is(TokKind::End)){
                cs.body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
              }
              expect(TokKind::Dedent, "Expected dedent after case body");
            } else {
              cs.body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
            }
            st->cases.push_back(std::move(cs));
            continue;
          }

          if(accept(TokKind::KwDefault)){
            ASTMatch::Case cs;
            cs.tag = "default";
            cs.span = prev().span;
            expect(TokKind::Colon, "Expected ':' after default");
            if(accept(TokKind::Newline)){
              expect(TokKind::Indent, "Expected indent for default body");
              while(!is(TokKind::Dedent) && !is(TokKind::End)){
                cs.body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
              }
              expect(TokKind::Dedent, "Expected dedent after default");
            } else {
              cs.body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
            }
            st->cases.push_back(std::move(cs));
            continue;
          }

          // allow stray newlines
          accept(TokKind::Newline);
          if(!is(TokKind::Dedent)){
            diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected case/default in match"));
            at++;
          }
        }
        expect(TokKind::Dedent, "Expected dedent after match");
      } else {
        // canonical: match expr { case Tag(x): ... }
        expect(TokKind::LBrace, "Expected '{' after match");
        while(!accept(TokKind::RBrace) && !is(TokKind::End)){
          if(accept(TokKind::KwCase)){
            ASTMatch::Case cs;
            cs.span = prev().span;
            if(is(TokKind::Ident)){ cs.tag=peek().text; at++; }
            // optional payload binding: (x) or ((x,y,z)) not fully parsed; accept "(ident)" only
            if(accept(TokKind::LParen)){
              if(is(TokKind::Ident)){ cs.binds.push_back(peek().text); at++; }
              expect(TokKind::RParen,"Expected ')'");
            }
            expect(TokKind::Colon, "Expected ':' after case");
            // single stmt until ';' or block in braces
            if(accept(TokKind::LBrace)){
              while(!accept(TokKind::RBrace) && !is(TokKind::End)){
                cs.body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
              }
            } else {
              cs.body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
            }
            st->cases.push_back(std::move(cs));
            continue;
          }
          // skip separators
          accept(TokKind::Semi);
          if(!is(TokKind::RBrace)){
            // recovery
            at++;
          }
        }
      }
      return st;
    }

    // Fallback: expression statement (call)
    auto e = parse_expr();
    // Keep only call statements for now; others are ignored
    eat_stmt_sep();
    struct ExprStmt : ASTStmt {
      std::unique_ptr<ASTExpr> expr;
      ExprStmt(Span s, std::unique_ptr<ASTExpr> e){ kind=ASTKind::Call; span=s; expr=std::move(e); }
    };
    return std::make_unique<ExprStmt>(e->span, std::move(e));
  }

  std::unique_ptr<ASTProc> parse_proc(){
    // visibility + modifiers
    bool is_pub=false,is_priv=false,is_export=false,is_inline=false,is_async=false,is_ded=false;
    if(accept(TokKind::KwPublic)) is_pub=true;
    else if(accept(TokKind::KwPrivate)) is_priv=true;
    else if(accept(TokKind::KwExport)) is_export=true; // surface "export inline proc ..." seen inside namespace

    if(accept(TokKind::KwInline)) is_inline=true;
    if(accept(TokKind::KwAsync)) is_async=true;
    if(accept(TokKind::KwDedicate)) is_ded=true;

    if(!accept(TokKind::KwProc)){
      diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected proc"));
      return nullptr;
    }
    auto s = prev().span;
    auto p = std::make_unique<ASTProc>(s);
    p->is_public=is_pub; p->is_private=is_priv; p->is_export=is_export; p->is_inline=is_inline; p->is_async=is_async; p->is_dedicate=is_ded;

    auto nm = parse_ident();
    if(!nm){ diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected proc name")); return p; }
    p->name = *nm;

    // Params:
    if(ciam.surface_mode){
      // surface: proc foo x i64 y i64 -> i64:
      while(is(TokKind::Ident)){
        std::string pname = peek().text; at++;
        TypeRef ty = parse_type();
        p->params.push_back({pname, ty});
      }
      expect(TokKind::Arrow, "Expected '->'");
      p->ret = parse_type();
      // optional requires caps
      if(accept(TokKind::KwRequires)){
        // requires network_io:
        while(is(TokKind::Ident)){
          p->requires_caps.insert(peek().text); at++;
        }
      }
      expect(TokKind::Colon, "Expected ':'");
      expect(TokKind::Newline, "Expected newline");
      expect(TokKind::Indent, "Expected indent for proc body");
      while(!is(TokKind::Dedent) && !is(TokKind::End)){
        p->body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
      }
      expect(TokKind::Dedent, "Expected dedent after proc");
    } else {
      // canonical: proc foo(x: i64, y: i64) -> i64 { ... }
      expect(TokKind::LParen, "Expected '('");
      if(!accept(TokKind::RParen)){
        while(true){
          auto pn = parse_ident();
          expect(TokKind::Colon, "Expected ':' in param");
          TypeRef ty = parse_type();
          p->params.push_back({*pn, ty});
          if(accept(TokKind::Comma)) continue;
          expect(TokKind::RParen, "Expected ')'");
          break;
        }
      }
      expect(TokKind::Arrow, "Expected '->'");
      p->ret = parse_type();
      // requires(network_io, ...)
      if(accept(TokKind::KwRequires)){
        expect(TokKind::LParen,"Expected '(' after requires");
        if(!accept(TokKind::RParen)){
          while(true){
            auto cap = parse_ident();
            if(cap) p->requires_caps.insert(*cap);
            if(accept(TokKind::Comma)) continue;
            expect(TokKind::RParen,"Expected ')'");
            break;
          }
        }
      }
      expect(TokKind::LBrace, "Expected '{'");
      while(!accept(TokKind::RBrace) && !is(TokKind::End)){
        p->body.push_back(std::unique_ptr<ASTStmt>(static_cast<ASTStmt*>(parse_stmt().release())));
      }
    }
    return p;
  }

  std::unique_ptr<ASTVariantDecl> parse_variant_decl_surface(Span s){
    auto vd = std::make_unique<ASTVariantDecl>(s);
    auto nm = parse_ident();
    if(!nm){ diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected variant name")); return vd; }
    vd->name = *nm;

    // optional generic params: Maybe<T>
    if(accept(TokKind::Lt)){
      while(true){
        auto tp = parse_ident();
        if(tp) vd->type_params.push_back(*tp);
        if(accept(TokKind::Comma)) continue;
        expect(TokKind::Gt, "Expected '>'");
        break;
      }
    }

    expect(TokKind::Colon, "Expected ':' after variant decl");
    expect(TokKind::Newline,"Expected newline");
    expect(TokKind::Indent,"Expected indent");
    while(!is(TokKind::Dedent) && !is(TokKind::End)){
      if(is(TokKind::Ident)){
        ASTVariantDecl::Tag tag;
        tag.name = peek().text; at++;
        // payload types: "Some T" or "Data Payload" or "Three (T T T)"
        while(is(TokKind::Ident) || is(TokKind::LParen)){
          if(accept(TokKind::LParen)){
            // tuple payload types inside (...)
            while(!accept(TokKind::RParen) && !is(TokKind::End)){
              if(is(TokKind::Ident)){
                TypeRef tr{peek().text,{}};
                at++;
                tag.payload.push_back(std::move(tr));
                continue;
              }
              accept(TokKind::Comma);
              if(is(TokKind::RParen)) break;
              at++;
            }
          } else {
            TypeRef tr{peek().text,{}};
            at++;
            tag.payload.push_back(std::move(tr));
          }
        }
        eat_stmt_sep();
        vd->tags.push_back(std::move(tag));
        continue;
      }
      accept(TokKind::Newline);
      if(!is(TokKind::Dedent)){
        diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected variant tag"));
        at++;
      }
    }
    expect(TokKind::Dedent,"Expected dedent after variant");
    return vd;
  }

  ASTModule parse_module(){
    ASTModule m;

    // Allow leading newlines
    while(accept(TokKind::Newline)) {}

    // imports
    while(accept(TokKind::KwImport)){
      auto s = prev().span;
      // path may be ident or dotted path; we accept ident
      if(is(TokKind::Ident)){
        m.imports.push_back(std::make_unique<ASTImport>(s, peek().text));
        at++;
      } else {
        diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected import path ident"));
      }
      eat_stmt_sep();
    }

    // module name
    if(accept(TokKind::KwModule)){
      auto nm = parse_ident();
      if(nm) m.name = *nm;
      eat_stmt_sep();
    } else {
      diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected module"));
    }

    // top-level: namespace / variant / procs / capability decl (subset)
    while(!is(TokKind::End)){
      if(accept(TokKind::KwCapability)){
        // surface: capability heap_alloc  | canonical: capability(heap_alloc);
        if(ciam.surface_mode){
          if(is(TokKind::Ident)){ m.declared_caps.insert(peek().text); at++; }
          eat_stmt_sep();
        } else {
          expect(TokKind::LParen,"Expected '('");
          if(is(TokKind::Ident)){ m.declared_caps.insert(peek().text); at++; }
          expect(TokKind::RParen,"Expected ')'");
          eat_stmt_sep();
        }
        continue;
      }

      if(accept(TokKind::KwVariant)){
        auto s=prev().span;
        if(ciam.surface_mode){
          m.variants.push_back(parse_variant_decl_surface(s));
          continue;
        } else {
          // canonical: variant Maybe<T> = Some(T) | None
          // Not fully parsed here; treat as not implemented for now.
          diags.push_back(make_diag(DiagCode::NOT_IMPLEMENTED, s, "Canonical variant decl parsing not implemented in this skeleton"));
          // recover: skip until ';'
          while(!accept(TokKind::Semi) && !is(TokKind::End)) at++;
          continue;
        }
      }

      if(accept(TokKind::KwNamespace)){
        auto s=prev().span;
        auto nm = parse_ident();
        if(!nm){ diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Expected namespace name")); break; }
        auto ns = std::make_unique<ASTNamespace>(s, *nm);

        if(ciam.surface_mode){
          expect(TokKind::Colon, "Expected ':' after namespace");
          expect(TokKind::Newline, "Expected newline");
          expect(TokKind::Indent, "Expected indent for namespace");
          while(!is(TokKind::Dedent) && !is(TokKind::End)){
            // "export inline proc ..." in namespace
            if(is(TokKind::KwExport) || is(TokKind::KwPublic) || is(TokKind::KwPrivate) || is(TokKind::KwProc) || is(TokKind::KwInline) || is(TokKind::KwAsync) || is(TokKind::KwDedicate)){
              auto pr = parse_proc();
              if(pr) ns->procs.push_back(std::move(pr));
              continue;
            }
            accept(TokKind::Newline);
            if(!is(TokKind::Dedent)){
              diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Unexpected token inside namespace"));
              at++;
            }
          }
          expect(TokKind::Dedent, "Expected dedent after namespace");
        } else {
          expect(TokKind::LBrace, "Expected '{' after namespace");
          while(!accept(TokKind::RBrace) && !is(TokKind::End)){
            auto pr = parse_proc();
            if(pr) ns->procs.push_back(std::move(pr));
          }
        }

        m.namespaces.push_back(std::move(ns));
        continue;
      }

      // procs
      if(is(TokKind::KwPublic) || is(TokKind::KwPrivate) || is(TokKind::KwExport) || is(TokKind::KwProc) || is(TokKind::KwInline) || is(TokKind::KwAsync) || is(TokKind::KwDedicate)){
        auto pr = parse_proc();
        if(pr) m.procs.push_back(std::move(pr));
        continue;
      }

      // skip stray newlines/semi
      if(accept(TokKind::Newline) || accept(TokKind::Semi)) continue;

      // unknown
      diags.push_back(make_diag(DiagCode::PARSE_ERROR, peek().span, "Unexpected top-level token"));
      at++;
    }

    return m;
  }
};

////////////////////////////////////////////////////////////////
// Resolver (names/scopes/capabilities/types — minimal, expandable)
////////////////////////////////////////////////////////////////
enum class BaseType : uint8_t { I64, I32, U32, BOOL, STRING, VOID, UNKNOWN };

struct SymInfo {
  std::string name;
  BaseType type = BaseType::UNKNOWN;
  bool is_proc=false;
  std::unordered_set<std::string> requires_caps;
};

struct ResolveResult {
  std::vector<Diag> diags;
  std::unordered_map<std::string, SymInfo> globals;
};

static BaseType map_type(const TypeRef& t){
  if(t.name=="i64"||t.name=="int") return BaseType::I64;
  if(t.name=="i32") return BaseType::I32;
  if(t.name=="u32") return BaseType::U32;
  if(t.name=="bool") return BaseType::BOOL;
  if(t.name=="string") return BaseType::STRING;
  if(t.name=="void") return BaseType::VOID;
  return BaseType::UNKNOWN;
}

static ResolveResult resolve(CIAMContext& ciam, ASTModule& m){
  ResolveResult rr;

  // gather procs in namespaces and module
  auto add_proc = [&](ASTProc& p){
    if(rr.globals.count(p.name)){
      rr.diags.push_back(make_diag(DiagCode::REDECLARED_NAME, p.span, "Redeclared proc: " + p.name));
      return;
    }
    SymInfo si;
    si.name = p.name;
    si.is_proc = true;
    si.type = map_type(p.ret);
    si.requires_caps = p.requires_caps;
    rr.globals[p.name]=std::move(si);
  };

  for(auto& ns : m.namespaces){
    for(auto& p : ns->procs) add_proc(*p);
  }
  for(auto& p : m.procs) add_proc(*p);

  // capability check: if proc requires cap not declared globally, flag it
  auto check_caps = [&](ASTProc& p){
    for(auto& cap : p.requires_caps){
      if(!m.declared_caps.count(cap)){
        rr.diags.push_back(make_diag(DiagCode::CAPABILITY_VIOLATION, p.span,
          "Proc '" + p.name + "' requires capability '" + cap + "' but it is not declared (capability ...)."));
      }
    }
  };

  for(auto& ns : m.namespaces) for(auto& p : ns->procs) check_caps(*p);
  for(auto& p : m.procs) check_caps(*p);

  (void)ciam;
  return rr;
}

////////////////////////////////////////////////////////////////
// Typed CIL
////////////////////////////////////////////////////////////////
enum class TCilOp : uint16_t {
  // Constants / moves
  CONST_I64,
  MOV,

  // Integer ops (subset)
  ADD_I64,
  SUB_I64,

  // Control
  RET,
  BR,        // unconditional jump label
  BR_IF,     // branch if (nonzero)
  LABEL,

  // Calls
  CALL_SYM,  // call by symbol id
  CALL_IMPORT, // call import by name

  // Capabilities (semantic fences)
  CAP_CHECK, // ensure capability present

  // Variants (core)
  VAR_NEW,     // create variant value
  VAR_IS,      // test tag
  VAR_GET,     // extract payload (by index)
  MATCH_BEGIN, // metadata anchor (debug)
  MATCH_END,

  // MMIO (stubbed but cataloged)
  MMIO_READ32,
  MMIO_WRITE32,

  // Trap/halt (control)
  TRAP,
  HALT,
};

struct VReg { uint32_t id=0; };
struct Label { uint32_t id=0; };

struct TCilInstr {
  TCilOp op{};
  Span span{};
  // operands encoded in a generic way for this skeleton
  VReg dst{}, a{}, b{};
  int64_t imm = 0;
  std::string sym; // for imports/symbol names/tags
  Label lab0{}, lab1{};
  uint32_t aux = 0; // e.g. payload index/tag id
};

struct TCilFunc {
  std::string name;
  std::vector<TCilInstr> ins;
  std::unordered_map<std::string, VReg> locals;
  uint32_t next_vreg=1;
  uint32_t next_label=1;
  BaseType ret = BaseType::I32;
  std::unordered_set<std::string> requires_caps;
};

struct TCilModule {
  std::string name;
  std::vector<TCilFunc> funcs;
};

static VReg new_vreg(TCilFunc& f){ return VReg{f.next_vreg++}; }
static Label new_label(TCilFunc& f){ return Label{f.next_label++}; }

static void emit(TCilFunc& f, TCilInstr in){ f.ins.push_back(std::move(in)); }

static VReg lower_expr(TCilFunc& f, ASTExpr& e){
  switch(e.kind){
    case ASTKind::Int: {
      auto& n = static_cast<ASTInt&>(e);
      VReg r = new_vreg(f);
      emit(f, {TCilOp::CONST_I64, n.span, r, {}, {}, n.value});
      return r;
    }
    case ASTKind::String: {
      // Strings not modeled in codegen for this skeleton; treat as 0
      auto& n = static_cast<ASTString&>(e);
      VReg r = new_vreg(f);
      emit(f, {TCilOp::CONST_I64, n.span, r, {}, {}, 0});
      return r;
    }
    case ASTKind::Ident: {
      auto& n = static_cast<ASTIdent&>(e);
      auto it = f.locals.find(n.name);
      if(it!=f.locals.end()) return it->second;
      // unknown ident -> 0 to keep pipeline running; resolver should have flagged in a fuller build
      VReg r=new_vreg(f);
      emit(f, {TCilOp::CONST_I64, n.span, r, {}, {}, 0});
      return r;
    }
    case ASTKind::Call: {
      auto& c = static_cast<ASTCall&>(e);
      // only support: call symbol with 0..1 arg -> returns i64
      VReg dst = new_vreg(f);
      // For now, pass first arg in a and embed call name
      VReg arg0{};
      if(!c.args.empty()) arg0 = lower_expr(f, *c.args[0]);
      TCilInstr in{};
      in.op = TCilOp::CALL_SYM;
      in.span = c.span;
      in.dst = dst;
      in.a = arg0;
      in.sym = c.callee;
      emit(f, std::move(in));
      return dst;
    }
    default:
      break;
  }
  VReg r=new_vreg(f);
  emit(f, {TCilOp::CONST_I64, e.span, r, {}, {}, 0});
  return r;
}

static void lower_stmt(TCilFunc& f, ASTStmt& s){
  switch(s.kind){
    case ASTKind::Let: {
      auto& st = static_cast<ASTLet&>(s);
      VReg v = lower_expr(f, *st.init);
      f.locals[st.name]=v;
      return;
    }
    case ASTKind::Return: {
      auto& st = static_cast<ASTReturn&>(s);
      VReg v = lower_expr(f, *st.value);
      TCilInstr in{}; in.op=TCilOp::RET; in.span=st.span; in.a=v;
      emit(f, std::move(in));
      return;
    }
    case ASTKind::If: {
      auto& st = static_cast<ASTIf&>(s);
      VReg c = lower_expr(f, *st.cond);
      Label l_then = new_label(f);
      Label l_else = new_label(f);
      Label l_end  = new_label(f);

      // BR_IF c -> then else
      TCilInstr br{}; br.op=TCilOp::BR_IF; br.span=st.span; br.a=c; br.lab0=l_then; br.lab1=l_else;
      emit(f, std::move(br));

      emit(f, {TCilOp::LABEL, st.span, {}, {}, {}, 0, "", l_then});
      for(auto& ss : st.thenStmts) lower_stmt(f, *ss);
      emit(f, {TCilOp::BR, st.span, {}, {}, {}, 0, "", l_end});

      emit(f, {TCilOp::LABEL, st.span, {}, {}, {}, 0, "", l_else});
      for(auto& ss : st.elseStmts) lower_stmt(f, *ss);
      emit(f, {TCilOp::BR, st.span, {}, {}, {}, 0, "", l_end});

      emit(f, {TCilOp::LABEL, st.span, {}, {}, {}, 0, "", l_end});
      return;
    }
    case ASTKind::Match: {
      auto& mt = static_cast<ASTMatch&>(s);
      VReg scr = lower_expr(f, *mt.scrutinee);

      emit(f, {TCilOp::MATCH_BEGIN, mt.span, {}, scr});

      // Very simple lowering:
      // - For each case: VAR_IS scr, tag -> if true jump into body
      // - default falls through
      Label l_end = new_label(f);

      std::vector<std::pair<ASTMatch::Case*, Label>> caseLabs;
      for(auto& cs : mt.cases){
        caseLabs.push_back({&cs, new_label(f)});
      }
      Label l_default{};
      bool has_default=false;
      for(auto& p : caseLabs){
        if(p.first->tag=="default"){ l_default=p.second; has_default=true; }
      }
      if(!has_default){ l_default = l_end; }

      // chain tests
      for(auto& p : caseLabs){
        auto* cs = p.first;
        if(cs->tag=="default") continue;
        Label l_hit = p.second;
        // VAR_IS dst = is_tag(scr, cs->tag)
        VReg test = new_vreg(f);
        TCilInstr is{}; is.op=TCilOp::VAR_IS; is.span=cs->span; is.dst=test; is.a=scr; is.sym=cs->tag;
        emit(f, std::move(is));
        // BR_IF test -> hit else continue
        Label l_next = new_label(f);
        emit(f, {TCilOp::BR_IF, cs->span, {}, test, {}, 0, "", l_hit, l_next});
        emit(f, {TCilOp::LABEL, cs->span, {}, {}, {}, 0, "", l_next});
      }

      // default jump
      emit(f, {TCilOp::BR, mt.span, {}, {}, {}, 0, "", l_default});

      // bodies
      for(auto& p : caseLabs){
        auto* cs = p.first;
        Label l_body = p.second;
        emit(f, {TCilOp::LABEL, cs->span, {}, {}, {}, 0, "", l_body});
        for(auto& bs : cs->body) lower_stmt(f, *bs);
        emit(f, {TCilOp::BR, cs->span, {}, {}, {}, 0, "", l_end});
      }

      emit(f, {TCilOp::LABEL, mt.span, {}, {}, {}, 0, "", l_end});
      emit(f, {TCilOp::MATCH_END, mt.span});
      return;
    }
    default:
      // treat call-expr as statement if it is ASTKind::Call wrapper
      return;
  }
}

static TCilModule lower_to_tcil(CIAMContext& ciam, ASTModule& m, const ResolveResult& rr){
  (void)rr;
  CIAM::decide_lowering(ciam);

  TCilModule out;
  out.name = m.name;

  auto lower_proc = [&](ASTProc& p){
    TCilFunc f;
    f.name = p.name;
    f.ret = map_type(p.ret);
    f.requires_caps = p.requires_caps;

    // capability checks at function entry (semantic insertion)
    for(auto& cap : f.requires_caps){
      TCilInstr chk{};
      chk.op = TCilOp::CAP_CHECK;
      chk.sym = cap;
      chk.span = p.span;
      emit(f, std::move(chk));
    }

    // Params become locals vregs
    for(auto& [pn,pt] : p.params){
      (void)pt;
      f.locals[pn] = new_vreg(f);
    }

    for(auto& st : p.body){
      lower_stmt(f, *st);
    }

    // If no explicit RET, return 0
    bool has_ret=false;
    for(auto& in : f.ins) if(in.op==TCilOp::RET) { has_ret=true; break; }
    if(!has_ret){
      VReg z=new_vreg(f);
      emit(f, {TCilOp::CONST_I64, p.span, z, {}, {}, 0});
      emit(f, {TCilOp::RET, p.span, {}, z});
    }
    out.funcs.push_back(std::move(f));
  };

  for(auto& ns : m.namespaces) for(auto& p : ns->procs) lower_proc(*p);
  for(auto& p : m.procs) lower_proc(*p);
  return out;
}

////////////////////////////////////////////////////////////////
// OSW (Optimized Structure Web) — minimal “web” model + passes
////////////////////////////////////////////////////////////////
struct OSWProgram {
  TCilModule tcil;
};

static void osw_const_fold(OSWProgram& p){
  // tiny constant folding: if CALL_SYM not present and return is CONST -> RET, collapse to RET imm.
  // More fully: build def-use, fold ADD/SUB, DCE etc. This is a placeholder pass hook.
  (void)p;
}

static void osw_run(CIAMContext& ciam, OSWProgram& p){
  osw_const_fold(p);
  CIAM::osw_shape(ciam, p);
}

////////////////////////////////////////////////////////////////
// Frame planner (Win64 ABI: shadow space 32 bytes, 16-byte align)
////////////////////////////////////////////////////////////////
struct FramePlan {
  uint32_t shadow = 32;
  uint32_t locals_bytes = 0;
  uint32_t stack_bytes = 0; // shadow + locals + align pad
};

static FramePlan plan_frame(const TCilFunc& f){
  FramePlan fp;
  // naive: each vreg that must spill gets 8 bytes. For now spill none.
  fp.locals_bytes = 0;
  uint32_t raw = fp.shadow + fp.locals_bytes;
  uint32_t align = 16;
  uint32_t pad = (align - (raw % align)) % align;
  fp.stack_bytes = raw + pad;
  (void)f;
  return fp;
}

////////////////////////////////////////////////////////////////
// x64 Emission + minimal PE writer
//
// This skeleton generates:
//   main_stub:
//     sub rsp, stack_bytes
//     mov ecx, <return_code>
//     call ExitProcess
//
// It extracts return_code if TCil main returns constant.
// If not constant, returns 0 (still emits).
////////////////////////////////////////////////////////////////
struct ByteBuf {
  std::vector<uint8_t> b;
  void u8(uint8_t v){ b.push_back(v); }
  void u32(uint32_t v){
    for(int i=0;i<4;i++) b.push_back(uint8_t((v>>(8*i))&0xFF));
  }
  void u64(uint64_t v){
    for(int i=0;i<8;i++) b.push_back(uint8_t((v>>(8*i))&0xFF));
  }
  void bytes(std::initializer_list<uint8_t> v){ b.insert(b.end(), v.begin(), v.end()); }
};

static std::optional<int32_t> try_eval_const_return(const TCilFunc& f){
  // If last meaningful pattern is CONST_I64 rX then RET rX -> extract imm.
  std::unordered_map<uint32_t, int64_t> consts;
  for(auto& in : f.ins){
    if(in.op==TCilOp::CONST_I64){
      consts[in.dst.id] = in.imm;
    } else if(in.op==TCilOp::RET){
      auto it = consts.find(in.a.id);
      if(it!=consts.end()) return (int32_t)it->second;
    }
  }
  return std::nullopt;
}

////////////////////////////////////////////////////////////////
// Minimal PE32+ (Windows x64) writer with import ExitProcess
////////////////////////////////////////////////////////////////
#pragma pack(push,1)
struct IMAGE_DOS_HEADER {
  uint16_t e_magic=0x5A4D;
  uint16_t e_cblp=0x0090, e_cp=0x0003, e_crlc=0, e_cparhdr=0x0004;
  uint16_t e_minalloc=0, e_maxalloc=0xFFFF, e_ss=0, e_sp=0x00B8;
  uint16_t e_csum=0, e_ip=0, e_cs=0, e_lfarlc=0x0040, e_ovno=0;
  uint16_t e_res[4]{};
  uint16_t e_oemid=0, e_oeminfo=0;
  uint16_t e_res2[10]{};
  int32_t  e_lfanew=0x80;
};

struct IMAGE_FILE_HEADER {
  uint16_t Machine=0x8664;
  uint16_t NumberOfSections=0;
  uint32_t TimeDateStamp=0;
  uint32_t PointerToSymbolTable=0;
  uint32_t NumberOfSymbols=0;
  uint16_t SizeOfOptionalHeader=0;
  uint16_t Characteristics=0x0022;
};

struct IMAGE_DATA_DIRECTORY { uint32_t VirtualAddress=0; uint32_t Size=0; };

struct IMAGE_OPTIONAL_HEADER64 {
  uint16_t Magic=0x20B;
  uint8_t  MajorLinkerVersion=14;
  uint8_t  MinorLinkerVersion=0;
  uint32_t SizeOfCode=0;
  uint32_t SizeOfInitializedData=0;
  uint32_t SizeOfUninitializedData=0;
  uint32_t AddressOfEntryPoint=0;
  uint32_t BaseOfCode=0;
  uint64_t ImageBase=0x0000000140000000ULL;
  uint32_t SectionAlignment=0x1000;
  uint32_t FileAlignment=0x200;
  uint16_t MajorOperatingSystemVersion=6;
  uint16_t MinorOperatingSystemVersion=0;
  uint16_t MajorImageVersion=0;
  uint16_t MinorImageVersion=0;
  uint16_t MajorSubsystemVersion=6;
  uint16_t MinorSubsystemVersion=0;
  uint32_t Win32VersionValue=0;
  uint32_t SizeOfImage=0;
  uint32_t SizeOfHeaders=0;
  uint32_t CheckSum=0;
  uint16_t Subsystem=3; // Windows CUI
  uint16_t DllCharacteristics=0x8160;
  uint64_t SizeOfStackReserve=0x100000;
  uint64_t SizeOfStackCommit=0x1000;
  uint64_t SizeOfHeapReserve=0x100000;
  uint64_t SizeOfHeapCommit=0x1000;
  uint32_t LoaderFlags=0;
  uint32_t NumberOfRvaAndSizes=16;
  IMAGE_DATA_DIRECTORY DataDirectory[16]{};
};

struct IMAGE_NT_HEADERS64 {
  uint32_t Signature=0x00004550; // PE\0\0
  IMAGE_FILE_HEADER FileHeader{};
  IMAGE_OPTIONAL_HEADER64 OptionalHeader{};
};

struct IMAGE_SECTION_HEADER {
  uint8_t  Name[8]{};
  union { uint32_t PhysicalAddress; uint32_t VirtualSize; } Misc{};
  uint32_t VirtualAddress=0;
  uint32_t SizeOfRawData=0;
  uint32_t PointerToRawData=0;
  uint32_t PointerToRelocations=0;
  uint32_t PointerToLinenumbers=0;
  uint16_t NumberOfRelocations=0;
  uint16_t NumberOfLinenumbers=0;
  uint32_t Characteristics=0;
};

struct IMAGE_IMPORT_DESCRIPTOR {
  uint32_t OriginalFirstThunk=0;
  uint32_t TimeDateStamp=0;
  uint32_t ForwarderChain=0;
  uint32_t Name=0;
  uint32_t FirstThunk=0;
};
#pragma pack(pop)

static uint32_t align_up(uint32_t x, uint32_t a){ return (x + (a-1)) & ~(a-1); }

static bool write_file(const std::string& path, const std::vector<uint8_t>& bytes){
  std::ofstream f(path, std::ios::binary);
  if(!f) return false;
  f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
  return true;
}

static std::vector<uint8_t> build_pe_x64_exitprocess(ByteBuf& text, int32_t retcode){
  // Layout:
  // Headers -> .text -> .rdata(.idata-ish kept in .rdata for simplicity)
  // This is a minimal, pragmatic PE; good enough for deterministic byte output and import call.

  const uint32_t FileAlign = 0x200;
  const uint32_t SectAlign = 0x1000;

  // --- Build .text ---
  ByteBuf code;
  code = text;

  // --- Build .rdata with import table for kernel32.dll ExitProcess ---
  ByteBuf rdata;

  // strings
  uint32_t rva_rdata_base = 0; // set later
  uint32_t off_kernel32 = (uint32_t)rdata.b.size();
  const char k32[]="kernel32.dll";
  rdata.b.insert(rdata.b.end(), k32, k32+sizeof(k32));

  uint32_t off_exitproc_hintname = (uint32_t)rdata.b.size();
  // IMAGE_IMPORT_BY_NAME: Hint (2) + Name + 0
  rdata.u16(0); // hint
  const char ep[]="ExitProcess";
  rdata.b.insert(rdata.b.end(), ep, ep+sizeof(ep));

  // align to 8
  while(rdata.b.size()%8) rdata.u8(0);

  uint32_t off_int = (uint32_t)rdata.b.size(); // Import Name Table (OriginalFirstThunk)
  rdata.u64(0); // thunk[0] -> &hint/name (filled later)
  rdata.u64(0); // null

  uint32_t off_iat = (uint32_t)rdata.b.size(); // Import Address Table (FirstThunk)
  rdata.u64(0); // same
  rdata.u64(0);

  // import descriptor
  uint32_t off_desc = (uint32_t)rdata.b.size();
  IMAGE_IMPORT_DESCRIPTOR desc{};
  rdata.b.insert(rdata.b.end(), (uint8_t*)&desc, (uint8_t*)&desc + sizeof(desc));
  IMAGE_IMPORT_DESCRIPTOR zero{};
  rdata.b.insert(rdata.b.end(), (uint8_t*)&zero, (uint8_t*)&zero + sizeof(zero));

  // --- Now set RVAs ---
  // We'll compute section RVAs after headers known.
  // Build headers first.

  IMAGE_DOS_HEADER dos{};
  std::vector<uint8_t> stub(0x80 - sizeof(dos), 0);
  // minimal DOS stub text isn't required, keep zeros.

  IMAGE_NT_HEADERS64 nt{};
  nt.FileHeader.NumberOfSections = 2;
  nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);

  IMAGE_SECTION_HEADER sh_text{};
  std::memcpy(sh_text.Name, ".text", 5);
  sh_text.Misc.VirtualSize = (uint32_t)code.b.size();
  sh_text.VirtualAddress = SectAlign; // 0x1000
  sh_text.SizeOfRawData = align_up((uint32_t)code.b.size(), FileAlign);
  sh_text.PointerToRawData = align_up((uint32_t)(sizeof(IMAGE_DOS_HEADER)+stub.size()+sizeof(IMAGE_NT_HEADERS64)+2*sizeof(IMAGE_SECTION_HEADER)), FileAlign);
  sh_text.Characteristics = 0x60000020; // RX

  IMAGE_SECTION_HEADER sh_rdata{};
  std::memcpy(sh_rdata.Name, ".rdata", 6);
  sh_rdata.Misc.VirtualSize = (uint32_t)rdata.b.size();
  sh_rdata.VirtualAddress = align_up(sh_text.VirtualAddress + sh_text.Misc.VirtualSize, SectAlign);
  sh_rdata.SizeOfRawData = align_up((uint32_t)rdata.b.size(), FileAlign);
  sh_rdata.PointerToRawData = sh_text.PointerToRawData + sh_text.SizeOfRawData;
  sh_rdata.Characteristics = 0x40000040; // R

  // patch RVAs in rdata
  rva_rdata_base = sh_rdata.VirtualAddress;

  uint32_t rva_kernel32 = rva_rdata_base + off_kernel32;
  uint32_t rva_hintname = rva_rdata_base + off_exitproc_hintname;
  uint32_t rva_int      = rva_rdata_base + off_int;
  uint32_t rva_iat      = rva_rdata_base + off_iat;
  uint32_t rva_desc     = rva_rdata_base + off_desc;

  // write thunk values
  auto patch_u64 = [&](uint32_t offset, uint64_t v){
    for(int i=0;i<8;i++) rdata.b[offset+i] = uint8_t((v>>(8*i))&0xFF);
  };
  patch_u64(off_int + 0, (uint64_t)rva_hintname);
  patch_u64(off_iat + 0, (uint64_t)rva_hintname);

  // patch import descriptor
  auto patch_u32 = [&](uint32_t offset, uint32_t v){
    for(int i=0;i<4;i++) rdata.b[offset+i] = uint8_t((v>>(8*i))&0xFF);
  };
  patch_u32(off_desc + 0, rva_int);      // OriginalFirstThunk
  patch_u32(off_desc + 12, rva_kernel32);// Name
  patch_u32(off_desc + 16, rva_iat);     // FirstThunk

  // optional header fields
  nt.OptionalHeader.AddressOfEntryPoint = sh_text.VirtualAddress; // entry at .text start
  nt.OptionalHeader.BaseOfCode = sh_text.VirtualAddress;

  nt.OptionalHeader.SizeOfCode = sh_text.SizeOfRawData;
  nt.OptionalHeader.SizeOfInitializedData = sh_rdata.SizeOfRawData;

  uint32_t headers_raw_size =
    align_up((uint32_t)(sizeof(IMAGE_DOS_HEADER)+stub.size()+sizeof(IMAGE_NT_HEADERS64)+2*sizeof(IMAGE_SECTION_HEADER)), FileAlign);

  nt.OptionalHeader.SizeOfHeaders = headers_raw_size;
  nt.OptionalHeader.SizeOfImage = align_up(sh_rdata.VirtualAddress + sh_rdata.Misc.VirtualSize, SectAlign);

  // Import directory points to descriptors
  nt.OptionalHeader.DataDirectory[1].VirtualAddress = rva_desc; // IMAGE_DIRECTORY_ENTRY_IMPORT
  nt.OptionalHeader.DataDirectory[1].Size = sizeof(IMAGE_IMPORT_DESCRIPTOR)*2;

  // IAT directory
  nt.OptionalHeader.DataDirectory[12].VirtualAddress = rva_iat; // IMAGE_DIRECTORY_ENTRY_IAT
  nt.OptionalHeader.DataDirectory[12].Size = 16;

  // --- Build final file ---
  std::vector<uint8_t> file;
  file.resize(headers_raw_size, 0);

  // DOS + stub
  std::memcpy(file.data(), &dos, sizeof(dos));
  // NT headers at 0x80
  std::memcpy(file.data()+dos.e_lfanew, &nt, sizeof(nt));
  // section headers immediately after NT
  uint8_t* sh_base = file.data()+dos.e_lfanew+sizeof(nt);
  std::memcpy(sh_base, &sh_text, sizeof(sh_text));
  std::memcpy(sh_base+sizeof(sh_text), &sh_rdata, sizeof(sh_rdata));

  // write .text raw
  file.resize(sh_rdata.PointerToRawData + sh_rdata.SizeOfRawData, 0);
  std::memcpy(file.data()+sh_text.PointerToRawData, code.b.data(), code.b.size());

  // write .rdata raw
  std::memcpy(file.data()+sh_rdata.PointerToRawData, rdata.b.data(), rdata.b.size());

  // Patch code CALL [RIP+disp32] to IAT slot:
  // We generated code with placeholder 0xCCCCCCCC disp32 at a known location (we’ll do that below).
  // In this simplified build, we emit:
  //   mov ecx, imm32
  //   sub rsp, imm8
  //   call qword ptr [rip+disp32]  ; points to IAT[0]
  //
  // We patch disp32 so:
  //   target = ImageBase + rva_iat
  //   next_ip = ImageBase + entry_rva + call_next_offset
  //   disp32 = target - next_ip
  //
  // Find marker sequence: FF 15 CC CC CC CC
  uint32_t text_raw = sh_text.PointerToRawData;
  for(uint32_t k=0; k+6 < (uint32_t)code.b.size(); k++){
    if(file[text_raw+k+0]==0xFF && file[text_raw+k+1]==0x15 &&
       file[text_raw+k+2]==0xCC && file[text_raw+k+3]==0xCC &&
       file[text_raw+k+4]==0xCC && file[text_raw+k+5]==0xCC){
      uint32_t call_rva = sh_text.VirtualAddress + k;
      uint32_t next_ip_rva = call_rva + 6;
      int64_t disp = (int64_t)rva_iat - (int64_t)next_ip_rva;
      int32_t d32 = (int32_t)disp;
      std::memcpy(file.data()+text_raw+k+2, &d32, 4);
      break;
    }
  }

  (void)retcode; // already embedded in code
  return file;
}

////////////////////////////////////////////////////////////////
// Emit stub .text for calling ExitProcess(retcode)
////////////////////////////////////////////////////////////////
static ByteBuf emit_text_main_stub(uint32_t stack_bytes, int32_t retcode){
  ByteBuf t;
  // sub rsp, imm32: 48 81 EC xx xx xx xx
  t.bytes({0x48,0x81,0xEC});
  t.u32(stack_bytes);

  // mov ecx, imm32: B9 xx xx xx xx
  t.u8(0xB9);
  t.u32((uint32_t)retcode);

  // call qword ptr [rip+disp32]: FF 15 dd dd dd dd
  t.bytes({0xFF,0x15,0xCC,0xCC,0xCC,0xCC}); // patch later

  // int3 (optional), add rsp, imm32, ret
  // add rsp, imm32: 48 81 C4 xx xx xx xx
  t.bytes({0x48,0x81,0xC4});
  t.u32(stack_bytes);

  // ret
  t.u8(0xC3);
  return t;
}

////////////////////////////////////////////////////////////////
// Pipeline runner
////////////////////////////////////////////////////////////////
static std::string read_all(const std::string& path){
  std::ifstream f(path, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static void print_diags(const std::vector<Diag>& d){
  for(auto& x : d){
    std::cerr << "diag(" << (uint32_t)x.code << ") @" << x.span.line << ":" << x.span.col
              << " " << x.message << "\n";
  }
}

int main(int argc, char** argv){
  if(argc < 4){
    std::cerr << "usage: rane_pipeline <input.rane> <out.exe> (--surface|--cil)\n";
    return 2;
  }

  std::string in_path = argv[1];
  std::string out_path = argv[2];
  std::string mode = argv[3];

  CIAMContext ciam{};
  ciam.surface_mode = (mode == "--surface");

  std::string src = read_all(in_path);
  LexResult lx = ciam.surface_mode ? lex_surface(src) : lex_canonical(src);
  if(!lx.diags.empty()){
    print_diags(lx.diags);
    return 3;
  }

  // CIAM token shaping stage
  CIAM::shape_tokens(ciam, lx.toks);

  Parser ps(ciam, lx.toks);
  ASTModule mod = ps.parse_module();
  if(!ps.diags.empty()){
    print_diags(ps.diags);
    return 4;
  }

  // CIAM grammar rewrites + sugar
  CIAM::rewrite_ast(ciam, mod);

  // CIAM semantic insertion (with/defer/lock lowering would live here)
  CIAM::semantic_insert(ciam, mod);

  // Resolver
  ResolveResult rr = resolve(ciam, mod);
  if(!rr.diags.empty()){
    print_diags(rr.diags);
    // continue anyway to allow iterative bring-up
  }

  // Typed CIL
  TCilModule tcil = lower_to_tcil(ciam, mod, rr);

  // OSW
  OSWProgram osw{tcil};
  osw_run(ciam, osw);

  // Find main
  const TCilFunc* mainf = nullptr;
  for(auto& f : osw.tcil.funcs){
    if(f.name == "main"){
      mainf = &f;
      break;
    }
  }
  if(!mainf){
    std::cerr << "No main() found; emitting stub that returns 0\n";
  }

  // Frame plan
  FramePlan fp = mainf ? plan_frame(*mainf) : FramePlan{};

  // Evaluate constant return
  int32_t rc = 0;
  if(mainf){
    if(auto v = try_eval_const_return(*mainf)) rc = *v;
  }

  // Emit .text + PE
  ByteBuf text = emit_text_main_stub(fp.stack_bytes, rc);
  auto pe = build_pe_x64_exitprocess(text, rc);
  if(!write_file(out_path, pe)){
    std::cerr << "Failed to write " << out_path << "\n";
    return 5;
  }

  std::cout << "Wrote " << out_path << " (ExitProcess rc=" << rc << ")\n";
  return 0;
}
