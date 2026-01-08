// RANE single-file compiler (handwritten IR + x64 emitter)
// - Loads `syntax.rane` as a rule DB
// - Lexes user code, runs CIAMS, parses to AST
// - Typechecks (minimal), lowers AST -> handcrafted IR
// - Emits x64 machine code (flat binary) using a small emitter
//
// This file upgrades the previous compiler implementation to cover the
// full `syntax.rane` surface for parsing, CIAMS normalization, and
// lowering to a conservative IR. Many advanced constructs are retained
// as parseable stubs so the pipeline can accept the entire coverage file.
// Keep extending lowering/codegen to fully implement semantics.

#include <array>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <stdexcept>
#include <algorithm>

static std::string read_file_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ----------------------------- Rule DB ------------------------------------

struct RuleDB {
    std::unordered_set<std::string> keywords;
    std::unordered_set<std::string> types;
    std::unordered_set<std::string> builtins;

    // Seed a comprehensive set (preserves everything in syntax.rane).
    void seed_comprehensive() {
        const char* kws[] = {
            "let","if","then","else","elif","while","do","for","break","continue","return","ret",
            "proc","def","call","import","export","include","exclude","decide","case","default",
            "jump","goto","mark","label","guard","zone","hot","cold","deterministic","repeat","unroll",
            "not","and","or","xor","shl","shr","sar",
            "try","catch","throw",
            "define","ifdef","ifndef","pragma","namespace","enum","struct","class","public","private","protected",
            "static","inline","extern","virtual","const","volatile","constexpr","consteval","constinit",
            "new","del","cast","type","typealias","alias","mut","immutable","mutable","null","match","pattern","lambda",
            "handle","target","splice","split","difference","increment","decrement","dedicate","mutex","ignore","bypass",
            "isolate","separate","join","declaration","compile","score","sys","admin","plot","peak","point","reg","exception",
            "align","mutate","string","literal","linear","nonlinear","primitives","tuples","member","open","close",
            "module","node","start","set","to","add","by","say","go","halt","into","from","mmio","region","read32","write32","trap",
            "vector","map","channel","spawn","join","lock","with","using","defer","macro","template","consteval","constinit",
            "asm","syscall","tailcall","unroll","profile","optimize","lto","eval","enum","contract","assert","async","await",
            "yield","coroutine","parallel","borrow","allocate","free","borrow_mut","borrowed","record","variant","union","event",
            "subscribe","emit","publish","handle","target","splice","split","difference","pragma","define","ifdef","ifndef"
        };
        for (auto k : kws) keywords.insert(k);

        const char* tps[] = {
            "u8","u16","u32","u64","u128","u512","i8","i16","i32","i64","i128","i512","f32","f64","f128","bool","string","void"
        };
        for (auto t : tps) types.insert(t);

        const char* blt[] = {
            "print","addr","load","store","choose","allocate","free","vector","map","send","recv","open","close","parse_int","parse"
        };
        for (auto b : blt) builtins.insert(b);
    }

    bool is_keyword(const std::string& s) const { return keywords.count(s) > 0; }
    bool is_type(const std::string& s) const { return types.count(s) > 0; }
    bool is_builtin(const std::string& s) const { return builtins.count(s) > 0; }
};

// ----------------------------- Lexer --------------------------------------

// Expanded tokenizer: accepts '#ident', raw strings, char literals, doc comments, nested block comments.

enum class TokKind { Eof, Ident, Number, String, Char, Kw, Sym, HashIdent };

struct Token {
    TokKind kind = TokKind::Eof;
    std::string lexeme;
    int line = 1, col = 1;
};

class Lexer {
    std::string src;
    size_t i = 0;
    int line = 1, col = 1;
    RuleDB* rules = nullptr;

    char peek(size_t off = 0) const { return (i + off < src.size()) ? src[i + off] : '\0'; }
    char getch() {
        char c = peek();
        if (!c) return c;
        ++i;
        if (c == '\n') { ++line; col = 1; } else ++col;
        return c;
    }

    void skip_ws_and_comments() {
        for (;;) {
            while (std::isspace((unsigned char)peek())) getch();

            // line comment //
            if (peek() == '/' && peek(1) == '/') {
                // doc comment '///' preserved as comment only
                getch(); getch();
                while (peek() && peek() != '\n') getch();
                continue;
            }

            // block comment /* ... */ with nesting
            if (peek() == '/' && peek(1) == '*') {
                getch(); getch();
                int depth = 1;
                while (peek()) {
                    if (peek() == '/' && peek(1) == '*') { getch(); getch(); ++depth; continue; }
                    if (peek() == '*' && peek(1) == '/') { getch(); getch(); --depth; if (depth == 0) break; continue; }
                    getch();
                }
                continue;
            }

            break;
        }
    }

public:
    Lexer(std::string s, RuleDB* r) : src(std::move(s)), rules(r) {}

    Token next() {
        skip_ws_and_comments();
        Token t; t.line = line; t.col = col;
        char c = peek();
        if (!c) { t.kind = TokKind::Eof; return t; }

        // Hash-ident like '#REG' or '#rane_rt_print'
        if (c == '#') {
            getch();
            t.lexeme.push_back('#');
            while (std::isalnum((unsigned char)peek()) || peek()=='_' || peek()==':') t.lexeme.push_back(getch());
            t.kind = TokKind::HashIdent;
            return t;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            while (isalnum((unsigned char)peek()) || peek() == '_' || peek() == '?') t.lexeme.push_back(getch());
            if (rules && rules->is_keyword(t.lexeme)) t.kind = TokKind::Kw;
            else t.kind = TokKind::Ident;
            return t;
        }
        if (isdigit((unsigned char)c)) {
            // number: supports underscores, hex (0x), bin (0b), oct (0o), floats
            if (c == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
                t.lexeme.push_back(getch()); t.lexeme.push_back(getch());
                while (std::isxdigit((unsigned char)peek()) || peek() == '_') t.lexeme.push_back(getch());
                t.kind = TokKind::Number;
                return t;
            }
            if (c == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
                t.lexeme.push_back(getch()); t.lexeme.push_back(getch());
                while ((peek() == '0' || peek() == '1' || peek() == '_')) t.lexeme.push_back(getch());
                t.kind = TokKind::Number;
                return t;
            }
            bool seenDot = false;
            while (std::isdigit((unsigned char)peek()) || peek() == '_' || (!seenDot && peek() == '.')) {
                if (peek() == '.') seenDot = true;
                t.lexeme.push_back(getch());
            }
            t.kind = TokKind::Number;
            return t;
        }
        if (c == '"' || c == '\'') {
            char q = getch();
            bool isChar = (q == '\'');
            while (peek() && peek() != q) {
                char ch = getch();
                if (ch == '\\' && peek()) { t.lexeme.push_back(ch); t.lexeme.push_back(getch()); }
                else t.lexeme.push_back(ch);
            }
            if (peek() == q) getch();
            t.kind = isChar ? TokKind::Char : TokKind::String;
            return t;
        }

        // multi-char symbols
        std::string two;
        two.push_back(peek()); two.push_back(peek(1));
        if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "&&" || two == "||" ||
            two == "<<" || two == ">>" || two == "->" || two == "::" || two == "??" || two == "->" || two == "=>") {
            t.lexeme = two;
            getch(); getch();
            t.kind = TokKind::Sym;
            return t;
        }

        // single-char
        t.lexeme.push_back(getch());
        t.kind = TokKind::Sym;
        return t;
    }

    std::vector<Token> lex_all() {
        std::vector<Token> out;
        for (;;) {
            Token t = next();
            if (t.kind == TokKind::Eof) break;
            out.push_back(std::move(t));
        }
        return out;
    }
};

// ----------------------------- CIAMS --------------------------------------
// Richer contextual rewrites and normalization to cover syntax.rane forms.

void ciams_run(std::vector<Token>& toks) {
    // Normalize word operators into symbol tokens where appropriate.
    for (auto &t : toks) {
        if (t.kind == TokKind::Kw) {
            if (t.lexeme == "xor") { t.kind = TokKind::Sym; t.lexeme = "^"; }
            if (t.lexeme == "and") { t.kind = TokKind::Sym; t.lexeme = "&&"; }
            if (t.lexeme == "or")  { t.kind = TokKind::Sym; t.lexeme = "||"; }
            if (t.lexeme == "not") { t.kind = TokKind::Sym; t.lexeme = "!"; }
            if (t.lexeme == "shl") { t.kind = TokKind::Sym; t.lexeme = "<<"; }
            if (t.lexeme == "shr") { t.kind = TokKind::Sym; t.lexeme = ">>"; }
            if (t.lexeme == "sar") { t.kind = TokKind::Sym; t.lexeme = ">>"; } // approximate
        }
    }

    // Heuristic: convert '=' to '==' in expression contexts (but not after 'let' or 'set').
    for (size_t i = 1; i + 1 < toks.size(); ++i) {
        if (toks[i].kind == TokKind::Sym && toks[i].lexeme == "=") {
            bool left_is_keyword_let = (toks[i-1].kind == TokKind::Kw && (toks[i-1].lexeme == "let" || toks[i-1].lexeme == "set"));
            bool left_is_comma = (toks[i-1].kind == TokKind::Sym && toks[i-1].lexeme == ",");
            bool likely_expr = !left_is_keyword_let && !left_is_comma;
            if (likely_expr) toks[i].lexeme = "==";
        }
    }

    // Other CIAMS: recognize `choose max(a,b)` -> canonical `choose_max(a,b)` by token fusion.
    for (size_t i = 2; i + 1 < toks.size(); ++i) {
        if (toks[i-2].kind == TokKind::Kw && toks[i-2].lexeme == "choose" &&
            toks[i-1].kind == TokKind::Ident &&
            (toks[i-1].lexeme == "max" || toks[i-1].lexeme == "min") &&
            toks[i].kind == TokKind::Sym && toks[i].lexeme == "(") {
            // fold by changing token sequence to Ident: choose_max with removing choose
            toks[i-1].lexeme = std::string("choose_") + toks[i-1].lexeme;
            toks.erase(toks.begin() + (i-2));
            i = (i>=3) ? i-3 : 0;
        }
    }
}

// ----------------------------- AST ----------------------------------------

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// Pattern support for match/case
struct Pattern {
    enum Kind { Wild, Ident, Int, Tuple, Constructor } k = Wild;
    std::string text; // identifier or int text or constructor name
    std::vector<Pattern> parts;
};

// Match case
struct MatchCase {
    Pattern pat;
    ExprPtr expr;
};

struct Expr {
    enum Kind { IntLit, FloatLit, StrLit, BoolLit, NullLit, Ident, HashIdent, Unary, Binary, Call, Ternary,
                ArrayLit, Field, Index, StructLit, Lambda, Match } k;
    std::string text; // literal text, ident name, or field name
    std::string op; // operator for unary/binary
    std::vector<ExprPtr> args; // call args, binary children, array elements, index operands
    ExprPtr cond; // optional for ternary

    // Lambda specifics
    std::vector<std::string> lambda_params;
    std::vector<StmtPtr> lambda_body;

    // Match specifics
    std::vector<MatchCase> match_cases;
    Expr() = default;
};

struct Stmt {
    enum Kind { Let, Return, ExprStmt } k;
    std::string name; // let name or other
    ExprPtr expr;
};

struct Proc {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
};

struct StructDecl {
    std::string name;
    // pair<fieldname, type name>
    std::vector<std::pair<std::string,std::string>> fields;
};

struct TypeAlias {
    std::string name;
    std::string target;
};

struct ModuleDecl {
    std::string name;
    // nested program content
    std::vector<StructDecl> structs;
    std::vector<TypeAlias> typealiases;
    std::vector<Proc> procs;
    std::vector<std::pair<std::string,std::string>> imports; // name -> alias
};

struct Program {
    std::vector<Proc> procs;
    std::vector<StructDecl> structs;
    std::vector<TypeAlias> typealiases;
    std::vector<ModuleDecl> modules;
    std::vector<std::pair<std::string,std::string>> imports; // top-level imports (name -> alias)
};

// ----------------------------- Parser -------------------------------------

struct Parser {
    std::vector<Token> toks;
    size_t p = 0;
    Parser(std::vector<Token> t): toks(std::move(t)) {}

    const Token& cur() const { static Token eof{TokKind::Eof,"",0,0}; return p < toks.size() ? toks[p] : eof; }
    bool accept(TokKind k, const std::string& s = "") {
        if (cur().kind == k && (s.empty() || cur().lexeme == s)) { ++p; return true; }
        return false;
    }
    bool accept_sym(const std::string& s) { return accept(TokKind::Sym, s); }
    void expect_sym(const std::string& s) { if (!accept_sym(s)) throw std::runtime_error(std::string("expected symbol: ") + s + " got '" + cur().lexeme + "'"); }
    void expect_kw(const std::string& s) { if (!(cur().kind == TokKind::Kw && cur().lexeme == s)) throw std::runtime_error(std::string("expected keyword: ") + s); ++p; }

    // Helper: read possibly qualified identifier sequence like `mod::sub::name`
    std::string read_qualified_ident_after_consuming_first(const std::string& first) {
        std::string name = first;
        while (accept_sym("::")) {
            if (cur().kind != TokKind::Ident) throw std::runtime_error("expected ident after ::");
            name += "::" + cur().lexeme;
            ++p;
        }
        return name;
    }

    // Pattern parsing (simple)
    Pattern parse_pattern() {
        Pattern pat;
        if (cur().kind == TokKind::Ident) {
            std::string id = cur().lexeme; ++p;
            if (id == "_") { pat.k = Pattern::Wild; pat.text = "_"; return pat; }
            // constructor-like or binding
            if (accept_sym("(")) {
                pat.k = Pattern::Constructor;
                pat.text = id;
                // parse subpatterns
                if (!accept_sym(")")) {
                    for (;;) {
                        pat.parts.push_back(parse_pattern());
                        if (accept_sym(")")) break;
                        expect_sym(",");
                    }
                }
                return pat;
            } else {
                // plain identifier binding
                pat.k = Pattern::Ident; pat.text = id; return pat;
            }
        } else if (cur().kind == TokKind::Number) {
            pat.k = Pattern::Int; pat.text = cur().lexeme; ++p; return pat;
        } else if (accept_sym("(")) {
            pat.k = Pattern::Tuple;
            if (!accept_sym(")")) {
                for (;;) {
                    pat.parts.push_back(parse_pattern());
                    if (accept_sym(")")) break;
                    expect_sym(",");
                }
            }
            return pat;
        } else {
            // fallback wildcard
            pat.k = Pattern::Wild; pat.text = "_"; return pat;
        }
    }

    ExprPtr parse_primary() {
        // Numbers, strings, bool, null, hash-ident handled as before
        if (cur().kind == TokKind::Number) {
            // Decide int vs float ('.' indicates float for our syntax)
            if (cur().lexeme.find('.') != std::string::npos) {
                auto e = std::make_unique<Expr>(); e->k = Expr::FloatLit; e->text = cur().lexeme; ++p; return e;
            } else {
                auto e = std::make_unique<Expr>(); e->k = Expr::IntLit; e->text = cur().lexeme; ++p; return e;
            }
        }
        if (cur().kind == TokKind::String) {
            auto e = std::make_unique<Expr>(); e->k = Expr::StrLit; e->text = cur().lexeme; ++p; return e;
        }
        if (cur().kind == TokKind::Kw && (cur().lexeme=="true" || cur().lexeme=="false")) {
            auto e = std::make_unique<Expr>(); e->k = Expr::BoolLit; e->text = cur().lexeme; ++p; return e;
        }
        if (cur().kind == TokKind::Kw && cur().lexeme=="null") {
            auto e = std::make_unique<Expr>(); e->k = Expr::NullLit; ++p; return e;
        }
        if (cur().kind == TokKind::HashIdent) {
            auto e = std::make_unique<Expr>(); e->k = Expr::HashIdent; e->text = cur().lexeme; ++p; return e;
        }

        // Lambda: 'lambda (params) { ... }' or 'lambda (params) => expr'
        if (cur().kind == TokKind::Kw && cur().lexeme == "lambda") {
            ++p;
            auto e = std::make_unique<Expr>(); e->k = Expr::Lambda;
            expect_sym("(");
            if (!accept_sym(")")) {
                for (;;) {
                    if (cur().kind != TokKind::Ident) throw std::runtime_error("expected param name in lambda");
                    e->lambda_params.push_back(cur().lexeme); ++p;
                    if (accept_sym(")")) break;
                    expect_sym(",");
                }
            }
            if (accept_sym("{")) {
                while (!(cur().kind == TokKind::Sym && cur().lexeme == "}")) {
                    e->lambda_body.push_back(parse_stmt());
                }
                expect_sym("}");
            } else if (accept_sym("=>")) {
                // single-expression lambda body, wrap in return
                StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::Return;
                s->expr = parse_expr();
                e->lambda_body.push_back(std::move(s));
            } else {
                throw std::runtime_error("expected '{' or '=>' after lambda parameter list");
            }
            return e;
        }

        // Match: 'match expr { case pat => expr ; ... }'
        if (cur().kind == TokKind::Kw && cur().lexeme == "match") {
            ++p;
            auto e = std::make_unique<Expr>(); e->k = Expr::Match;
            // subject expression
            e->args.push_back(parse_expr());
            expect_sym("{");
            while (!(cur().kind == TokKind::Sym && cur().lexeme == "}")) {
                // accept 'case' or 'default'
                if (cur().kind == TokKind::Kw && cur().lexeme == "case") {
                    ++p;
                    MatchCase mc;
                    mc.pat = parse_pattern();
                    expect_sym("=>");
                    mc.expr = parse_expr();
                    accept(TokKind::Sym, ";");
                    e->match_cases.push_back(std::move(mc));
                    continue;
                } else if (cur().kind == TokKind::Kw && cur().lexeme == "default") {
                    ++p;
                    MatchCase mc;
                    mc.pat.k = Pattern::Wild;
                    expect_sym("=>");
                    mc.expr = parse_expr();
                    accept(TokKind::Sym, ";");
                    e->match_cases.push_back(std::move(mc));
                    continue;
                } else {
                    // skip unexpected tokens to remain robust
                    ++p;
                }
            }
            expect_sym("}");
            return e;
        }

        // Array literal [ ... ]
        if (accept_sym("[")) {
            auto arr = std::make_unique<Expr>(); arr->k = Expr::ArrayLit;
            if (!accept_sym("]")) {
                for (;;) {
                    arr->args.push_back(parse_expr());
                    if (accept_sym("]")) break;
                    expect_sym(",");
                }
            }
            return arr;
        }

        if (cur().kind == TokKind::Ident) {
            // read possibly qualified id 'mod::name' before deciding call/ident
            std::string id = cur().lexeme; ++p;
            id = read_qualified_ident_after_consuming_first(id);

            // call: 'id(...)'
            if (accept(TokKind::Sym, "(")) {
                auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = id;
                if (!accept(TokKind::Sym, ")")) {
                    for (;;) {
                        call->args.push_back(parse_expr());
                        if (accept(TokKind::Sym, ")")) break;
                        expect_sym(",");
                    }
                }
                // allow trailing field/indexing after call result
                ExprPtr curExpr = std::move(call);
                for (;;) {
                    if (accept_sym(".")) {
                        if (cur().kind == TokKind::Ident) {
                            std::string fld = cur().lexeme; ++p;
                            auto fldExpr = std::make_unique<Expr>(); fldExpr->k = Expr::Field; fldExpr->text = fld;
                            fldExpr->args.push_back(std::move(curExpr));
                            curExpr = std::move(fldExpr);
                            continue;
                        } else throw std::runtime_error("expected field name after '.'");
                    }
                    if (accept_sym("[")) {
                        auto idx = parse_expr();
                        expect_sym("]");
                        auto idxExpr = std::make_unique<Expr>(); idxExpr->k = Expr::Index;
                        idxExpr->args.push_back(std::move(curExpr));
                        idxExpr->args.push_back(std::move(idx));
                        curExpr = std::move(idxExpr);
                        continue;
                    }
                    break;
                }
                return curExpr;
            }

            // plain ident (or qualified) -> possibly followed by .field or [index]
            auto e = std::make_unique<Expr>(); e->k = Expr::Ident; e->text = id;
            ExprPtr curExpr = std::move(e);
            for (;;) {
                if (accept_sym(".")) {
                    if (cur().kind == TokKind::Ident) {
                        std::string fld = cur().lexeme; ++p;
                        auto fldExpr = std::make_unique<Expr>(); fldExpr->k = Expr::Field; fldExpr->text = fld;
                        fldExpr->args.push_back(std::move(curExpr));
                        curExpr = std::move(fldExpr);
                        continue;
                    } else throw std::runtime_error("expected field name after '.'");
                }
                if (accept_sym("[")) {
                    auto idx = parse_expr();
                    expect_sym("]");
                    auto idxExpr = std::make_unique<Expr>(); idxExpr->k = Expr::Index;
                    idxExpr->args.push_back(std::move(curExpr));
                    idxExpr->args.push_back(std::move(idx));
                    curExpr = std::move(idxExpr);
                    continue;
                }
                break;
            }
            return curExpr;
        }

        if (accept(TokKind::Sym, "(")) {
            auto e = parse_expr();
            expect_sym(")");
            return e;
        }
        // unexpected token -> return dummy 0 literal
        auto e = std::make_unique<Expr>(); e->k = Expr::IntLit; e->text = "0"; return e;
    }

    int prec_of(const Token& t) const {
        if (t.kind != TokKind::Sym) return -1;
        const std::string& op = t.lexeme;
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") return 3;
        if (op == "|" ) return 4;
        if (op == "^" ) return 5;
        if (op == "&" ) return 6;
        if (op == "<<" || op == ">>") return 7;
        if (op == "+" || op == "-") return 8;
        if (op == "*" || op == "/" || op == "%") return 9;
        return -1;
    }

    ExprPtr parse_unary() {
        if (cur().kind == TokKind::Sym && (cur().lexeme == "!" || cur().lexeme == "-" || cur().lexeme == "~")) {
            std::string op = cur().lexeme; ++p;
            auto rhs = parse_unary();
            auto e = std::make_unique<Expr>(); e->k = Expr::Unary; e->op = op; e->args.push_back(std::move(rhs)); return e;
        }
        if (cur().kind == TokKind::Kw && cur().lexeme == "not") {
            ++p;
            auto rhs = parse_unary();
            auto e = std::make_unique<Expr>(); e->k = Expr::Unary; e->op = "!"; e->args.push_back(std::move(rhs)); return e;
        }
        return parse_primary();
    }

    ExprPtr parse_bin_rhs(int minPrec, ExprPtr lhs) {
        for (;;) {
            int prec = (cur().kind == TokKind::Sym) ? prec_of(cur()) : -1;
            if (prec < minPrec) return lhs;
            Token op = cur(); ++p;
            auto rhs = parse_unary();
            int nextPrec = (cur().kind==TokKind::Sym) ? prec_of(cur()) : -1;
            if (nextPrec > prec) rhs = parse_bin_rhs(prec + 1, std::move(rhs));
            auto e = std::make_unique<Expr>(); e->k = Expr::Binary; e->op = op.lexeme;
            e->args.push_back(std::move(lhs)); e->args.push_back(std::move(rhs));
            lhs = std::move(e);
        }
    }

    ExprPtr parse_expr() {
        auto lhs = parse_unary();
        return parse_bin_rhs(1, std::move(lhs));
    }

    StmtPtr parse_stmt() {
        if (cur().kind == TokKind::Kw && cur().lexeme == "let") {
            ++p;
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::Let;
            if (cur().kind == TokKind::Ident) { s->name = cur().lexeme; ++p; }
            expect_sym("=");
            s->expr = parse_expr();
            accept(TokKind::Sym, ";");
            return s;
        }
        if (cur().kind == TokKind::Kw && cur().lexeme == "return") {
            ++p;
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::Return;
            s->expr = parse_expr();
            accept(TokKind::Sym, ";");
            return s;
        }

        // import <ident> [as alias] ;
        if (cur().kind == TokKind::Kw && cur().lexeme == "import") {
            ++p;
            std::string name;
            if (cur().kind == TokKind::Ident) {
                name = cur().lexeme; ++p;
                // read qualified import as needed
                name = read_qualified_ident_after_consuming_first(name);
            }
            std::string alias;
            if (cur().kind == TokKind::Kw && cur().lexeme == "as") { ++p; if (cur().kind == TokKind::Ident) { alias = cur().lexeme; ++p; } }
            accept(TokKind::Sym, ";");
            // record as ExprStmt import for existing pipeline, but also let Program collect them in parse_program
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = "import";
            if (!name.empty()) {
                auto arg = std::make_unique<Expr>(); arg->k = Expr::Ident; arg->text = name;
                call->args.push_back(std::move(arg));
            }
            if (!alias.empty()) {
                auto a2 = std::make_unique<Expr>(); a2->k = Expr::Ident; a2->text = alias;
                call->args.push_back(std::move(a2));
            }
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }

        // mmio region ...
        if (cur().kind == TokKind::Kw && cur().lexeme == "mmio") {
            ++p;
            // collect until semicolon
            std::vector<Token> saved;
            while (cur().kind != TokKind::Sym || cur().lexeme != ";") {
                if (cur().kind == TokKind::Eof) break;
                saved.push_back(cur()); ++p;
            }
            accept(TokKind::Sym, ";");
            // create stub call
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = "mmio_region";
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }

        // read32 REG, 0 into x;
        if (cur().kind == TokKind::Kw && (cur().lexeme == "read32" || cur().lexeme == "write32")) {
            std::string op = cur().lexeme; ++p;
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = op;
            // collect args until ';'
            while (!(cur().kind == TokKind::Sym && cur().lexeme == ";") && cur().kind != TokKind::Eof) {
                // parse expressions loosely: treat identifiers and numbers as primaries
                if (cur().kind == TokKind::Ident || cur().kind == TokKind::Number || cur().kind == TokKind::HashIdent) {
                    auto a = std::make_unique<Expr>();
                    a->k = (cur().kind==TokKind::Number)?Expr::IntLit:Expr::Ident;
                    a->text = cur().lexeme; call->args.push_back(std::move(a)); ++p;
                } else {
                    ++p;
                }
            }
            accept(TokKind::Sym, ";");
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }

        // label forms: "label L;" or "L:" form
        if (cur().kind == TokKind::Kw && cur().lexeme == "label") {
            ++p;
            std::string name;
            if (cur().kind == TokKind::Ident) { name = cur().lexeme; ++p; }
            accept(TokKind::Sym, ";");
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = "label";
            auto arg = std::make_unique<Expr>(); arg->k = Expr::Ident; arg->text = name;
            call->args.push_back(std::move(arg));
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }
        if (cur().kind == TokKind::Ident && (p+1 < toks.size()) && toks[p+1].kind==TokKind::Sym && toks[p+1].lexeme==":") {
            std::string name = cur().lexeme; p+=2; // consume ident and colon
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = "label";
            auto arg = std::make_unique<Expr>(); arg->k = Expr::Ident; arg->text = name;
            call->args.push_back(std::move(arg));
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }

        // trap, halt, call stmt, goto forms -> parse into Call exprs
        if (cur().kind == TokKind::Kw && cur().lexeme == "trap") {
            ++p;
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = "trap";
            if (cur().kind == TokKind::Number) { auto a = std::make_unique<Expr>(); a->k = Expr::IntLit; a->text = cur().lexeme; call->args.push_back(std::move(a)); ++p; }
            accept(TokKind::Sym, ";");
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }
        if (cur().kind == TokKind::Kw && cur().lexeme == "halt") {
            ++p; accept(TokKind::Sym, ";");
            auto call = std::make_unique<Expr>(); call->k = Expr::Call; call->text = "halt";
            StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt; s->expr = std::move(call);
            return s;
        }

        // generic expression-stmt fallback
        StmtPtr s = std::make_unique<Stmt>(); s->k = Stmt::ExprStmt;
        s->expr = parse_expr();
        accept(TokKind::Sym, ";");
        return s;
    }

    Proc parse_proc() {
        Proc pd;
        // expect 'proc'
        if (!(cur().kind == TokKind::Kw && cur().lexeme == "proc")) throw std::runtime_error("expected proc");
        ++p;
        if (cur().kind != TokKind::Ident) throw std::runtime_error("expected proc name");
        pd.name = cur().lexeme; ++p;
        expect_sym("(");
        if (!accept_sym(")")) {
            for (;;) {
                if (cur().kind != TokKind::Ident) throw std::runtime_error("expected param");
                pd.params.push_back(cur().lexeme); ++p;
                if (accept_sym(")")) break;
                expect_sym(",");
            }
        }
        expect_sym("{");
        while (!(cur().kind == TokKind::Sym && cur().lexeme == "}")) {
            pd.body.push_back(parse_stmt());
        }
        expect_sym("}");
        return pd;
    }

    StructDecl parse_struct_decl() {
        StructDecl sd;
        // current token is 'struct'
        if (!(cur().kind == TokKind::Kw && cur().lexeme == "struct")) throw std::runtime_error("expected struct");
        ++p;
        if (cur().kind == TokKind::Ident) { sd.name = cur().lexeme; ++p; }
        // optional '{' fields '}' or single-line "struct Name field:type;"
        if (accept_sym("{")) {
            while (!(cur().kind == TokKind::Sym && cur().lexeme == "}")) {
                if (cur().kind == TokKind::Ident) {
                    std::string fld = cur().lexeme; ++p;
                    if (accept_sym(":")) {
                        std::string ty;
                        if (cur().kind == TokKind::Ident || cur().kind == TokKind::Kw) { ty = cur().lexeme; ++p; }
                        sd.fields.emplace_back(fld, ty);
                        accept(TokKind::Sym, ";");
                        continue;
                    }
                }
                // skip unexpected tokens to remain robust
                ++p;
            }
            expect_sym("}");
        } else {
            // skip until semicolon or end
            while (p < toks.size() && !(cur().kind == TokKind::Sym && cur().lexeme == ";")) ++p;
            accept(TokKind::Sym, ";");
        }
        return sd;
    }

    TypeAlias parse_typealias() {
        // expect 'type' or 'typealias'
        if (!(cur().kind == TokKind::Kw && (cur().lexeme == "type" || cur().lexeme == "typealias"))) throw std::runtime_error("expected type/typealias");
        ++p;
        TypeAlias ta;
        if (cur().kind == TokKind::Ident) { ta.name = cur().lexeme; ++p; }
        if (accept_sym("=")) {
            if (cur().kind == TokKind::Ident || cur().kind == TokKind::Kw) { ta.target = cur().lexeme; ++p; }
            accept(TokKind::Sym, ";");
        } else {
            // fallback skip to semicolon
            while (p < toks.size() && !(cur().kind == TokKind::Sym && cur().lexeme == ";")) ++p;
            accept(TokKind::Sym, ";");
        }
        return ta;
    }

    ModuleDecl parse_module() {
        ModuleDecl md;
        if (!(cur().kind == TokKind::Kw && cur().lexeme == "module")) throw std::runtime_error("expected module");
        ++p;
        if (cur().kind == TokKind::Ident) { md.name = cur().lexeme; ++p; }
        // parse block
        if (accept_sym("{")) {
            while (!(cur().kind == TokKind::Sym && cur().lexeme == "}")) {
                if (cur().kind == TokKind::Kw && cur().lexeme == "proc") {
                    md.procs.push_back(parse_proc());
                    continue;
                }
                if (cur().kind == TokKind::Kw && cur().lexeme == "struct") {
                    md.structs.push_back(parse_struct_decl());
                    continue;
                }
                if (cur().kind == TokKind::Kw && (cur().lexeme == "type" || cur().lexeme == "typealias")) {
                    md.typealiases.push_back(parse_typealias());
                    continue;
                }
                if (cur().kind == TokKind::Kw && cur().lexeme == "import") {
                    ++p;
                    std::string name;
                    if (cur().kind == TokKind::Ident) {
                        name = cur().lexeme; ++p;
                        // read qualified import as needed
                        name = read_qualified_ident_after_consuming_first(name);
                    }
                    std::string alias;
                    if (cur().kind == TokKind::Kw && cur().lexeme == "as") { ++p; if (cur().kind == TokKind::Ident) { alias = cur().lexeme; ++p; } }
                    accept(TokKind::Sym, ";");
                    md.imports.emplace_back(name, alias);
                    continue;
                }
                // skip unknown tokens inside module
                ++p;
            }
            expect_sym("}");
        } else {
            // skip to end
            while (p < toks.size() && !(cur().kind == TokKind::Kw && cur().lexeme == "end")) ++p;
            if (p < toks.size() && cur().kind == TokKind::Kw && cur().lexeme == "end") ++p;
        }
        return md;
    }

    Program parse_program() {
        Program prog;
        while (p < toks.size()) {
            if (cur().kind == TokKind::Kw && cur().lexeme == "proc") {
                prog.procs.push_back(parse_proc());
                continue;
            }
            if (cur().kind == TokKind::Kw && cur().lexeme == "struct") {
                prog.structs.push_back(parse_struct_decl());
                continue;
            }
            if (cur().kind == TokKind::Kw && (cur().lexeme == "type" || cur().lexeme == "typealias")) {
                prog.typealiases.push_back(parse_typealias());
                continue;
            }
            if (cur().kind == TokKind::Kw && cur().lexeme == "module") {
                prog.modules.push_back(parse_module());
                continue;
            }
            if (cur().kind == TokKind::Kw && cur().lexeme == "import") {
                ++p;
                std::string name;
                if (cur().kind == TokKind::Ident) {
                    name = cur().lexeme; ++p;
                    name = read_qualified_ident_after_consuming_first(name);
                }
                std::string alias;
                if (cur().kind == TokKind::Kw && cur().lexeme == "as") { ++p; if (cur().kind == TokKind::Ident) { alias = cur().lexeme; ++p; } }
                accept(TokKind::Sym, ";");
                prog.imports.emplace_back(name, alias);
                continue;
            }
            // We still accept existing top-level constructs and skip others robustly.
            if (cur().kind == TokKind::Kw && (cur().lexeme == "node" || cur().lexeme == "enum" || cur().lexeme == "namespace")) {
                // consume declaration header then balanced block if present
                ++p;
                if (cur().kind == TokKind::Ident) ++p;
                if (cur().kind == TokKind::Sym && cur().lexeme == "{") {
                    int depth = 0;
                    while (p < toks.size()) {
                        if (cur().kind == TokKind::Sym && cur().lexeme == "{") ++depth;
                        else if (cur().kind == TokKind::Sym && cur().lexeme == "}") { --depth; if (depth <= 0) { ++p; break; } }
                        ++p;
                    }
                } else {
                    // consume until next blank or 'end' token
                    while (p < toks.size() && !(cur().kind == TokKind::Kw && cur().lexeme == "end")) ++p;
                    if (p < toks.size() && cur().kind == TokKind::Kw && cur().lexeme == "end") ++p;
                }
                continue;
            }
            // fallback: collect top-level statements into a synthetic proc? For now just advance.
            ++p;
        }
        return prog;
    }
};

// ----------------------------- Typechecking --------------------------------

struct TypeEnv {
    std::unordered_map<std::string, std::string> vars;
    std::unordered_map<std::string, Proc*> procs;
    std::unordered_map<std::string, StructDecl*> structs;
    std::unordered_map<std::string, std::string> typealiases;
    std::unordered_map<std::string, ModuleDecl*> modules;
    std::set<std::string> imports;

    // resolve a type name considering typealiases
    std::string resolve_type(const std::string& name) {
        auto it = typealiases.find(name);
        if (it != typealiases.end()) return it->second;
        return name;
    }
};

void typecheck_program(Program& prog, RuleDB& /*rules*/) {
    TypeEnv env;
    // register structs
    for (auto &sd : prog.structs) env.structs[sd.name] = const_cast<StructDecl*>(&sd);
    for (auto &ta : prog.typealiases) env.typealiases[ta.name] = ta.target;
    // register modules (top-level only)
    for (auto &md : prog.modules) env.modules[md.name] = const_cast<ModuleDecl*>(&md);
    // imports
    for (auto &imp : prog.imports) if (!imp.first.empty()) env.imports.insert(imp.first);

    for (auto& p : prog.procs) env.procs[p.name] = &p;
    for (auto& md : prog.modules) {
        for (auto &p : md.procs) env.procs[md.name + "::" + p.name] = &const_cast<Proc&>(p);
        for (auto &sd : md.structs) env.structs[md.name + "::" + sd.name] = const_cast<StructDecl*>(&sd);
        for (auto &ta : md.typealiases) env.typealiases[md.name + "::" + ta.name] = ta.target;
    }

    for (auto& p : prog.procs) {
        env.vars.clear();
        for (auto& par : p.params) env.vars[par] = "i64";
        for (auto& st : p.body) {
            if (st->k == Stmt::Let) env.vars[st->name] = "i64";
            // basic checks for field access: if expr is Field, ensure object exists (best-effort)
            if (st->expr && st->expr->k == Expr::Field) {
                auto &f = st->expr;
                if (!f->args.empty()) {
                    // if field applied to identifier, verify that identifier is known var
                    if (f->args[0]->k == Expr::Ident) {
                        std::string obj = f->args[0]->text;
                        if (env.vars.find(obj) == env.vars.end()) {
                            // maybe imported module type or global; warn but continue
                            // keep lightweight: do not error
                        }
                    }
                }
            }
        }
    }
    if (env.procs.find("main") == env.procs.end()) {
        std::cerr << "warning: no main() found in program\n";
    }
}

// ----------------------------- Handwritten IR --------------------------------

struct IRInst {
    enum Op { NOP, CONST, FCONST, ADD, SUB, MUL, DIV, FADD, FSUB, FMUL, FDIV, CALL, RET, PRINT, MMIO_READ, MMIO_WRITE, TRAP, HALT,
              ALLOC, FREE, CLOSURE, MATCH } op = NOP;
    int dst = -1;
    int lhs = -1;
    int rhs = -1;
    int64_t imm = 0;
    double fimm = 0.0;
    std::string sym;
};

struct IRFunc {
    std::string name;
    int paramCount = 0;
    std::vector<IRInst> insts;
    int next_temp = 0;
    std::unordered_map<std::string,int> locals;
    int alloc_temp() { return next_temp++; }
};

struct IRModule {
    std::vector<IRFunc> funcs;
    std::vector<std::pair<std::string,std::string>> datasec; // name -> literal
    IRFunc* find_func(const std::string& name) {
        for (auto &f: funcs) if (f.name == name) return &f;
        return nullptr;
    }
    std::string intern_string(const std::string& s) {
        std::string name = ".str" + std::to_string(datasec.size());
        datasec.push_back({name, s});
        return name;
    }
};

// -------------------------- Lowering AST -> IR ------------------------------

// helpers for cloning AST fragments (lambda lowering creates new IRFunc without mutating original AST)
static ExprPtr clone_expr(const Expr* e);
static StmtPtr clone_stmt(const Stmt* s);

static ExprPtr clone_expr(const Expr* e) {
    if (!e) return nullptr;
    auto r = std::make_unique<Expr>();
    r->k = e->k;
    r->text = e->text;
    r->op = e->op;
    for (auto &a : e->args) r->args.push_back(clone_expr(a.get()));
    if (e->cond) r->cond = clone_expr(e->cond.get());
    r->lambda_params = e->lambda_params;
    for (auto &st : e->lambda_body) r->lambda_body.push_back(clone_stmt(st.get()));
    for (auto &mc : e->match_cases) {
        MatchCase mcc;
        mcc.pat = mc.pat;
        mcc.expr = clone_expr(mc.expr.get());
        r->match_cases.push_back(std::move(mcc));
    }
    return r;
}
static StmtPtr clone_stmt(const Stmt* s) {
    if (!s) return nullptr;
    auto r = std::make_unique<Stmt>();
    r->k = s->k;
    r->name = s->name;
    r->expr = clone_expr(s->expr.get());
    return r;
}

static int parse_int_literal(const std::string& s) {
    std::string t; for (char c: s) if (c != '_') t.push_back(c);
    try {
        if (t.size()>2 && t[0]=='0' && (t[1]=='x' || t[1]=='X')) return (int)std::stoll(t, nullptr, 0);
        if (t.size()>2 && t[0]=='0' && (t[1]=='b' || t[1]=='B')) return (int)std::stoll(t.substr(2), nullptr, 2);
        return (int)std::stoll(t, nullptr, 10);
    } catch (...) { return 0; }
}

static double parse_float_literal(const std::string& s) {
    std::string t; for (char c: s) if (c != '_') t.push_back(c);
    try { return std::stod(t); } catch(...) { return 0.0; }
}

// Collect identifiers (simple) used in an expression (used to detect captures)
static void collect_idents_in_expr(const Expr* e, std::set<std::string>& out) {
    if (!e) return;
    if (e->k == Expr::Ident) out.insert(e->text);
    for (auto &a : e->args) collect_idents_in_expr(a.get(), out);
    if (e->cond) collect_idents_in_expr(e->cond.get(), out);
    for (auto &st : e->lambda_body) {
        if (st && st->expr) collect_idents_in_expr(st->expr.get(), out);
    }
    for (auto &mc : e->match_cases) {
        if (mc.expr) collect_idents_in_expr(mc.expr.get(), out);
    }
}

// we need an anon function counter for lambdas
static int g_anon_func_counter = 0;

// Forward declaration of lowering helpers
int lower_expr_to_ir(IRFunc& F, IRModule& M, Expr* e);
void lower_proc_into_ir(IRFunc& F, IRModule& M, const Proc& p);

int lower_expr_to_ir(IRFunc& F, IRModule& M, Expr* e) {
    if (!e) return -1;
    switch (e->k) {
    case Expr::IntLit: {
        int t = F.alloc_temp();
        IRInst i; i.op = IRInst::CONST; i.dst = t; i.imm = parse_int_literal(e->text);
        F.insts.push_back(i);
        return t;
    }
    case Expr::FloatLit: {
        int t = F.alloc_temp();
        IRInst i; i.op = IRInst::FCONST; i.dst = t; i.fimm = parse_float_literal(e->text);
        F.insts.push_back(i);
        return t;
    }
    case Expr::StrLit: {
        // intern to module data
        std::string sym = M.intern_string(e->text);
        int t = F.alloc_temp();
        IRInst i; i.op = IRInst::CONST; i.dst = t; i.sym = sym;
        F.insts.push_back(i);
        return t;
    }
    case Expr::ArrayLit: {
        // lower elements to temps (but allocate an opaque runtime array)
        std::vector<int> elems;
        for (auto &el : e->args) elems.push_back(lower_expr_to_ir(F, M, el.get()));
        int ret = F.alloc_temp();
        IRInst ci; ci.op = IRInst::CALL; ci.dst = ret; ci.sym = "array_literal";
        // pack simple metadata cheaply in imm (not used by emitter) so passes optimizers
        for (size_t i = 0; i < elems.size(); ++i) ci.imm ^= (elems[i] + 0x9e3779b97f4a7c15ULL + (uint64_t)i);
        F.insts.push_back(ci);
        return ret;
    }
    case Expr::Ident: {
        auto it = F.locals.find(e->text);
        if (it != F.locals.end()) return it->second;
        int t = F.alloc_temp();
        IRInst i; i.op = IRInst::CONST; i.dst = t; i.imm = 0;
        F.insts.push_back(i);
        F.locals[e->text] = t;
        return t;
    }
    case Expr::Field: {
        // object.field -> lower object then emit a CALL stub to fetch field
        int obj = lower_expr_to_ir(F, M, e->args.size() ? e->args[0].get() : nullptr);
        int ret = F.alloc_temp();
        IRInst ci; ci.op = IRInst::CALL; ci.dst = ret; ci.lhs = obj; ci.sym = std::string("field_get:") + e->text;
        F.insts.push_back(ci);
        return ret;
    }
    case Expr::Index: {
        // obj[index] -> lower obj and index then make CALL stub
        int obj = lower_expr_to_ir(F, M, e->args.size() > 0 ? e->args[0].get() : nullptr);
        int idx = lower_expr_to_ir(F, M, e->args.size() > 1 ? e->args[1].get() : nullptr);
        int ret = F.alloc_temp();
        IRInst ci; ci.op = IRInst::CALL; ci.dst = ret; ci.lhs = obj; ci.rhs = idx; ci.sym = "index_get";
        F.insts.push_back(ci);
        return ret;
    }
    case Expr::Call: {
        // heap allocation and free builtins: implement with ALLOC/FREE ops
        if (e->text == "allocate" || e->text == "allocate_mem" || e->text == "allocate_heap") {
            int sizeArg = -1;
            if (!e->args.empty()) sizeArg = lower_expr_to_ir(F, M, e->args[0].get());
            int dst = F.alloc_temp();
            IRInst a; a.op = IRInst::ALLOC; a.dst = dst; a.lhs = sizeArg; F.insts.push_back(a);
            return dst;
        }
        if (e->text == "free") {
            int ptrArg = -1;
            if (!e->args.empty()) ptrArg = lower_expr_to_ir(F, M, e->args[0].get());
            IRInst fr; fr.op = IRInst::FREE; fr.lhs = ptrArg; F.insts.push_back(fr);
            return -1;
        }

        // builtin print accepts strings and numbers; lower as PRINT to runtime stub
        if (e->text == "print") {
            int argt = -1;
            if (!e->args.empty()) argt = lower_expr_to_ir(F, M, e->args[0].get());
            IRInst i; i.op = IRInst::PRINT; i.lhs = argt; F.insts.push_back(i);
            return -1;
        }
        // import: treat as no-op at lower level (import handled at parse/typecheck)
        if (e->text == "import") {
            return -1;
        }
        // generic call (including module-qualified names like mod::fn)
        std::vector<int> args;
        for (auto &a : e->args) args.push_back(lower_expr_to_ir(F, M, a.get()));
        int ret = F.alloc_temp();
        IRInst ci; ci.op = IRInst::CALL; ci.dst = ret; ci.lhs = (args.size() ? args[0] : -1); ci.sym = e->text;
        // cheap pack remaining args into imm (not used for semantics here)
        for (size_t ai=1; ai<args.size(); ++ai) ci.imm ^= args[ai];
        F.insts.push_back(ci);
        return ret;
    }
    case Expr::Binary: {
        // detect float operations if either operand is float constant or previously produced float temp
        int L = lower_expr_to_ir(F, M, e->args[0].get());
        int R = lower_expr_to_ir(F, M, e->args[1].get());
        int dst = F.alloc_temp();
        IRInst ins;
        // If either operand's defining instruction was FCONST or op produces float, prefer float ops
        bool useFloat = false;
        for (auto &i : F.insts) {
            if (i.dst == L && i.op == IRInst::FCONST) useFloat = true;
            if (i.dst == R && i.op == IRInst::FCONST) useFloat = true;
        }
        if (useFloat) {
            if (e->op == "+") ins.op = IRInst::FADD;
            else if (e->op == "-") ins.op = IRInst::FSUB;
            else if (e->op == "*") ins.op = IRInst::FMUL;
            else if (e->op == "/") ins.op = IRInst::FDIV;
            else ins.op = IRInst::FSUB;
        } else {
            if (e->op == "+") ins.op = IRInst::ADD;
            else if (e->op == "-") ins.op = IRInst::SUB;
            else if (e->op == "*") ins.op = IRInst::MUL;
            else if (e->op == "/") ins.op = IRInst::DIV;
            else ins.op = IRInst::SUB;
        }
        ins.dst = dst; ins.lhs = L; ins.rhs = R;
        F.insts.push_back(ins);
        return dst;
    }
    case Expr::Lambda: {
        // Lower lambda by creating a new IRFunc in module M, capturing free vars and producing a closure object.
        std::set<std::string> used;
        // collect idents from lambda body
        for (auto &st : e->lambda_body) if (st && st->expr) collect_idents_in_expr(st->expr.get(), used);
        // exclude parameters (they are local to lambda)
        for (auto &pn : e->lambda_params) used.erase(pn);
        // Now `used` contains candidate captured names
        std::vector<std::string> captures(used.begin(), used.end());

        std::string fname = ".anon" + std::to_string(g_anon_func_counter++);
        IRFunc lambdaF; lambdaF.name = fname;
        // lambda function parameters: first captured names, then lambda params
        lambdaF.paramCount = (int)(captures.size() + e->lambda_params.size());
        // create locals for captures and params
        for (auto &cap : captures) {
            int t = lambdaF.alloc_temp();
            lambdaF.locals[cap] = t;
        }
        for (auto &pname : e->lambda_params) {
            int t = lambdaF.alloc_temp();
            lambdaF.locals[pname] = t;
        }
        // Lower the lambda body into lambdaF (clone to avoid mutating original AST)
        for (auto &st : e->lambda_body) {
            if (!st) continue;
            if (st->k == Stmt::Let) {
                int src = -1;
                if (st->expr) src = lower_expr_to_ir(lambdaF, M, clone_expr(st->expr.get()).get());
                if (src >= 0) lambdaF.locals[st->name] = src;
            } else if (st->k == Stmt::Return) {
                int v = -1;
                if (st->expr) v = lower_expr_to_ir(lambdaF, M, clone_expr(st->expr.get()).get());
                IRInst r; r.op = IRInst::RET; r.lhs = v; lambdaF.insts.push_back(r);
            } else if (st->k == Stmt::ExprStmt) {
                if (st->expr) lower_expr_to_ir(lambdaF, M, clone_expr(st->expr.get()).get());
            }
        }
        // If lambda didn't end with RET, add a default RET 0
        if (lambdaF.insts.empty() || lambdaF.insts.back().op != IRInst::RET) {
            IRInst r; r.op = IRInst::RET; r.lhs = -1; lambdaF.insts.push_back(r);
        }
        // add lambdaF to module
        M.funcs.push_back(std::move(lambdaF));

        // In the parent function F, build closure creation instruction
        std::vector<int> capTemps;
        for (auto &capName : captures) {
            auto it = F.locals.find(capName);
            if (it != F.locals.end()) capTemps.push_back(it->second);
            else {
                // if not present in parent, create a zero temp
                int t = F.alloc_temp();
                IRInst i; i.op = IRInst::CONST; i.dst = t; i.imm = 0;
                F.insts.push_back(i);
                F.locals[capName] = t;
                capTemps.push_back(t);
            }
        }
        int closureTemp = F.alloc_temp();
        IRInst ci; ci.op = IRInst::CLOSURE; ci.dst = closureTemp; ci.sym = fname;
        ci.lhs = (capTemps.size() ? capTemps[0] : -1);
        for (size_t i = 1; i < capTemps.size(); ++i) ci.imm ^= (int64_t)capTemps[i] + (int64_t)(i * 7919);
        F.insts.push_back(ci);
        return closureTemp;
    }
    case Expr::Match: {
        // Lower match conservatively to a MATCH IR call that returns result of match.
        int subj = -1;
        if (!e->args.empty()) subj = lower_expr_to_ir(F, M, e->args[0].get());
        int dst = F.alloc_temp();
        IRInst mi; mi.op = IRInst::MATCH; mi.dst = dst; mi.lhs = subj;
        // pack simple info about cases into imm (not used here); ensures uniqueness
        for (size_t i = 0; i < e->match_cases.size(); ++i) {
            const auto &mc = e->match_cases[i];
            if (mc.pat.k == Pattern::Int) mi.imm ^= (int64_t)parse_int_literal(mc.pat.text) + (int64_t)(i*1315423911);
            else mi.imm ^= (int64_t)(i+1) * 2654435761LL;
        }
        F.insts.push_back(mi);
        return dst;
    }
    default:
        return -1;
    }
}

void lower_proc_into_ir(IRFunc& F, IRModule& M, const Proc& p) {
    // already set up param locals by the caller
    for (auto &st : p.body) {
        if (st->k == Stmt::Let) {
            int src = -1;
            if (st->expr) src = lower_expr_to_ir(F, M, st->expr.get());
            if (src >= 0) F.locals[st->name] = src;
        } else if (st->k == Stmt::Return) {
            int v = -1;
            if (st->expr) v = lower_expr_to_ir(F, M, st->expr.get());
            IRInst r; r.op = IRInst::RET; r.lhs = v; F.insts.push_back(r);
        } else if (st->k == Stmt::ExprStmt) {
            if (st->expr) {
                if (st->expr->k == Expr::Call) {
                    std::string callee = st->expr->text;
                    if (callee == "trap") {
                        IRInst t; t.op = IRInst::TRAP; if (!st->expr->args.empty() && st->expr->args[0]) t.imm = parse_int_literal(st->expr->args[0]->text); F.insts.push_back(t);
                    } else if (callee == "halt") {
                        IRInst h; h.op = IRInst::HALT; F.insts.push_back(h);
                    } else if (callee == "read32") {
                        IRInst r; r.op = IRInst::MMIO_READ; if (!st->expr->args.empty()) r.sym = st->expr->args[0]->text; if (st->expr->args.size()>1) r.imm = parse_int_literal(st->expr->args[1]->text); F.insts.push_back(r);
                    } else if (callee == "write32") {
                        IRInst w; w.op = IRInst::MMIO_WRITE; if (!st->expr->args.empty()) w.sym = st->expr->args[0]->text; if (st->expr->args.size()>1) w.imm = parse_int_literal(st->expr->args[1]->text); if (st->expr->args.size()>2) w.lhs = lower_expr_to_ir(F, M, st->expr->args[2].get()); F.insts.push_back(w);
                    } else {
                        lower_expr_to_ir(F, M, st->expr.get());
                    }
                } else {
                    lower_expr_to_ir(F, M, st->expr.get());
                }
            }
        }
    }
}

// Lower entire program into IR
IRModule lower_program_to_ir(Program& P) {
    IRModule M;
    // Lower module-level procs as separate functions with module::prefix in name
    for (auto &md : P.modules) {
        for (auto &p : md.procs) {
            IRFunc F; F.name = md.name + "::" + p.name; F.paramCount = (int)p.params.size();
            for (size_t i = 0; i < p.params.size(); ++i) {
                int t = F.alloc_temp();
                F.locals[p.params[i]] = t;
            }
            lower_proc_into_ir(F, M, p);
            // ensure function ends with RET
            if (F.insts.empty() || F.insts.back().op != IRInst::RET) {
                IRInst r; r.op = IRInst::RET; r.lhs = -1; F.insts.push_back(r);
            }
            M.funcs.push_back(std::move(F));
        }
    }

    // Lower top-level procs
    for (auto &p : P.procs) {
        IRFunc F; F.name = p.name; F.paramCount = (int)p.params.size();
        for (size_t i = 0; i < p.params.size(); ++i) {
            int t = F.alloc_temp();
            F.locals[p.params[i]] = t;
        }
        lower_proc_into_ir(F, M, p);
        if (F.insts.empty() || F.insts.back().op != IRInst::RET) {
            IRInst r; r.op = IRInst::RET; r.lhs = -1; F.insts.push_back(r);
        }
        M.funcs.push_back(std::move(F));
    }
    return M;
}

// ----------------------------- NEW: AST Constant Folding --------------------
// Walk AST and fold integer/float constant binary/unary expressions.

static double eval_binary_float(const std::string& op, double a, double b) {
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") return b != 0.0 ? a / b : 0.0;
    if (op == "==") return (a == b) ? 1.0 : 0.0;
    if (op == "!=") return (a != b) ? 1.0 : 0.0;
    if (op == "<")  return (a < b) ? 1.0 : 0.0;
    if (op == "<=") return (a <= b) ? 1.0 : 0.0;
    if (op == ">")  return (a > b) ? 1.0 : 0.0;
    if (op == ">=") return (a >= b) ? 1.0 : 0.0;
    return 0.0;
}

static int64_t eval_binary_int(const std::string& op, int64_t a, int64_t b) {
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") return b != 0 ? a / b : 0;
    if (op == "%") return b != 0 ? a % b : 0;
    if (op == "==") return (a == b) ? 1 : 0;
    if (op == "!=") return (a != b) ? 1 : 0;
    if (op == "<")  return (a < b) ? 1 : 0;
    if (op == "<=") return (a <= b) ? 1 : 0;
    if (op == ">")  return (a > b) ? 1 : 0;
    if (op == ">=") return (a >= b) ? 1 : 0;
    if (op == "&")  return a & b;
    if (op == "|")  return a | b;
    if (op == "^")  return a ^ b;
    if (op == "<<") return a << b;
    if (op == ">>") return a >> b;
    return 0;
}

bool fold_constants_in_expr(Expr* e) {
    if (!e) return false;
    bool changed = false;
    // Recurse first
    for (auto &a : e->args) if (a) changed |= fold_constants_in_expr(a.get());
    if (e->cond) changed |= fold_constants_in_expr(e->cond.get());
    for (auto &st : e->lambda_body) if (st && st->expr) changed |= fold_constants_in_expr(st->expr.get());
    for (auto &mc : e->match_cases) if (mc.expr) changed |= fold_constants_in_expr(mc.expr.get());

    // Fold unary int/float
    if (e->k == Expr::Unary && e->args.size() == 1 && e->args[0]) {
        if (e->args[0]->k == Expr::IntLit) {
            int64_t v = parse_int_literal(e->args[0]->text);
            if (e->op == "-") v = -v;
            e->k = Expr::IntLit; e->text = std::to_string(v); e->args.clear(); changed = true;
        } else if (e->args[0]->k == Expr::FloatLit) {
            double v = parse_float_literal(e->args[0]->text);
            if (e->op == "-") v = -v;
            e->k = Expr::FloatLit; e->text = std::to_string(v); e->args.clear(); changed = true;
        }
    }

    // Fold binary int
    if (e->k == Expr::Binary && e->args.size() == 2 && e->args[0] && e->args[1]) {
        if (e->args[0]->k == Expr::IntLit && e->args[1]->k == Expr::IntLit) {
            int64_t a = parse_int_literal(e->args[0]->text);
            int64_t b = parse_int_literal(e->args[1]->text);
            int64_t r = eval_binary_int(e->op, a, b);
            e->k = Expr::IntLit; e->text = std::to_string(r); e->args.clear(); changed = true;
        } else if ((e->args[0]->k == Expr::FloatLit || e->args[1]->k == Expr::FloatLit) &&
                   (e->args[0]->k == Expr::FloatLit || e->args[0]->k == Expr::IntLit) &&
                   (e->args[1]->k == Expr::FloatLit || e->args[1]->k == Expr::IntLit)) {
            double a = (e->args[0]->k == Expr::FloatLit) ? parse_float_literal(e->args[0]->text) : (double)parse_int_literal(e->args[0]->text);
            double b = (e->args[1]->k == Expr::FloatLit) ? parse_float_literal(e->args[1]->text) : (double)parse_int_literal(e->args[1]->text);
            double r = eval_binary_float(e->op, a, b);
            // if op produced boolean-like result but inputs included float, represent as int lit
            if (e->op == "==" || e->op == "!=" || e->op == "<" || e->op == "<=" || e->op == ">" || e->op == ">=") {
                e->k = Expr::IntLit; e->text = std::to_string((int64_t)r);
            } else {
                e->k = Expr::FloatLit; e->text = std::to_string(r);
            }
            e->args.clear(); changed = true;
        }
    }

    return changed;
}

bool fold_constants_in_proc(Proc& p) {
    bool changed = false;
    for (auto &s : p.body) {
        if (s && s->expr) changed |= fold_constants_in_expr(s->expr.get());
    }
    return changed;
}

void fold_constants_program(Program& prog) {
    for (auto &p : prog.procs) {
        bool any = fold_constants_in_proc(p);
        if (any) {
            // optional: repeat to reach fixed point
            fold_constants_in_proc(p);
        }
    }
}

// ----------------------------- NEW: IR Dead Code Elim ----------------------
// Remove IR instructions defining temps never used and without side-effects.

void optimize_ir_dead_code(IRModule& M) {
    for (auto &F : M.funcs) {
        bool changed = true;
        while (changed) {
            changed = false;
            std::set<int> used;
            // Scan for uses in operands and for return
            for (auto &ins : F.insts) {
                if (ins.lhs >= 0) used.insert(ins.lhs);
                if (ins.rhs >= 0) used.insert(ins.rhs);
                if (ins.op == IRInst::RET && ins.lhs >= 0) used.insert(ins.lhs);
                if (ins.op == IRInst::PRINT && ins.lhs >= 0) used.insert(ins.lhs);
                if (ins.op == IRInst::CLOSURE && ins.lhs >= 0) used.insert(ins.lhs);
            }
            std::vector<IRInst> out;
            for (auto &ins : F.insts) {
                bool defines_temp = (ins.dst >= 0);
                bool has_side_effect = (ins.op == IRInst::PRINT || ins.op == IRInst::CALL || ins.op == IRInst::MMIO_READ || ins.op == IRInst::MMIO_WRITE || ins.op == IRInst::TRAP || ins.op == IRInst::HALT || ins.op == IRInst::ALLOC || ins.op == IRInst::FREE || ins.op == IRInst::CLOSURE || ins.op == IRInst::MATCH);
                if (defines_temp && !has_side_effect && used.count(ins.dst) == 0) {
                    // drop instruction
                    changed = true;
                    continue;
                }
                out.push_back(ins);
            }
            F.insts.swap(out);
        }
    }
}

// ----------------------------- NEW: IR Peephole (CONST coalescing) --------
// Remove immediate CONST followed immediately by another CONST to same temp.

void optimize_ir_peephole(IRModule& M) {
    for (auto &F : M.funcs) {
        std::vector<IRInst> out;
        for (size_t i = 0; i < F.insts.size(); ++i) {
            if (i + 1 < F.insts.size()) {
                auto &a = F.insts[i];
                auto &b = F.insts[i+1];
                if ((a.op == IRInst::CONST || a.op == IRInst::FCONST) && (b.op == IRInst::CONST || b.op == IRInst::FCONST) && a.dst == b.dst) {
                    // keep only b (later constant)
                    out.push_back(b);
                    ++i; // skip b (already added)
                    continue;
                }
            }
            out.push_back(F.insts[i]);
        }
        F.insts.swap(out);
    }
}

// ----------------------------- x64 Emitter ---------------------------------

namespace rane { namespace x64 {
    using byte = uint8_t;
    enum Reg { RAX=0, RCX=1, RDX=2, RBX=3, RSP=4, RBP=5, RSI=6, RDI=7 };

    struct CodeBuffer {
        std::vector<byte> data;
        void emit(byte b) { data.push_back(b); }
        void emit(const std::vector<byte>& v) { data.insert(data.end(), v.begin(), v.end()); }
        void emit32(uint32_t x) { for (int i=0;i<4;++i) emit((x>>(i*8))&0xFF); }
        void emit64(uint64_t x) { for (int i=0;i<8;++i) emit((x>>(i*8))&0xFF); }
        size_t size() const { return data.size(); }
    };

    struct Emitter {
        CodeBuffer buf;

        void prologue(int stackBytes) {
            buf.emit({0x55, 0x48, 0x89, 0xE5});
            if (stackBytes > 0) {
                buf.emit({0x48, 0x81, 0xEC});
                buf.emit32((uint32_t)stackBytes);
            }
        }
        void epilogue() {
            buf.emit({0x48, 0x89, 0xEC, 0x5D, 0xC3});
        }

        void mov_imm64_to_reg(int reg, uint64_t imm) {
            buf.emit((byte)0x48);
            buf.emit((byte)(0xB8 + (reg & 7)));
            buf.emit64(imm);
        }

        void mov_reg_to_stackslot(int reg, int32_t disp) {
            byte modrm = 0x80 | ((reg & 7) << 3) | 0x5;
            buf.emit({0x48, 0x89, modrm});
            buf.emit32((uint32_t)(-disp));
        }

        void mov_stackslot_to_reg(int reg, int32_t disp) {
            byte modrm = 0x80 | ((reg & 7) << 3) | 0x5;
            buf.emit({0x48, 0x8B, modrm});
            buf.emit32((uint32_t)(-disp));
        }

        void add_reg_reg(int dst, int src) {
            byte modrm = 0xC0 | ((src & 7) << 3) | (dst & 7);
            buf.emit({0x48, 0x01, modrm});
        }
        void sub_reg_reg(int dst, int src) {
            byte modrm = 0xC0 | ((src & 7) << 3) | (dst & 7);
            buf.emit({0x48, 0x29, modrm});
        }
        void imul_reg_reg(int dst, int src) {
            byte modrm = ((dst & 7) << 3) | (src & 7);
            buf.emit({0x48, 0x0F, 0xAF, modrm});
        }
        void idiv_reg(int reg) {
            buf.emit({0x48, 0x99});
            buf.emit({0x48, 0xF7, (byte)(0xF8 | (reg & 7))});
        }

        void mov_stack_to_rax(int32_t disp) { mov_stackslot_to_reg(RAX, disp); }
        void mov_rax_to_stack(int32_t disp) { mov_reg_to_stackslot(RAX, disp); }

        // movsd xmm, qword [rbp - disp]
        void movsd_load_xmm_from_stack(int xmm, int32_t disp) {
            // F2 0F 10 /r : MOVSD xmm, m64
            byte reg = (0xC0 | ((xmm & 7) << 3) | 0x5);
            buf.emit({0xF2, 0x0F, 0x10, reg});
            buf.emit32((uint32_t)(-disp));
        }
        // movsd qword [rbp - disp], xmm
        void movsd_store_xmm_to_stack(int xmm, int32_t disp) {
            // F2 0F 11 /r : MOVSD m64, xmm
            byte reg = (0xC0 | ((xmm & 7) << 3) | 0x5);
            buf.emit({0xF2, 0x0F, 0x11, reg});
            buf.emit32((uint32_t)(-disp));
        }
        // addsd xmm1, xmm2
        void addsd_xmm_xmm(int dst_xmm, int src_xmm) {
            byte mod = ((src_xmm & 7) << 3) | (dst_xmm & 7);
            buf.emit({0xF2, 0x0F, 0x58, mod});
        }
        void subsd_xmm_xmm(int dst_xmm, int src_xmm) {
            byte mod = ((src_xmm & 7) << 3) | (dst_xmm & 7);
            buf.emit({0xF2, 0x0F, 0x5C, mod});
        }
        void mulsd_xmm_xmm(int dst_xmm, int src_xmm) {
            byte mod = ((src_xmm & 7) << 3) | (dst_xmm & 7);
            buf.emit({0xF2, 0x0F, 0x59, mod});
        }
        void divsd_xmm_xmm(int dst_xmm, int src_xmm) {
            byte mod = ((src_xmm & 7) << 3) | (dst_xmm & 7);
            buf.emit({0xF2, 0x0F, 0x5E, mod});
        }

        void write_to_file(const std::string& path) {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(buf.data.data()), buf.data.size());
        }
    };
}} // namespace rane::x64

// ----------------------------- IR -> x64 codegen ----------------------------

struct CodeGen {
    IRModule* M = nullptr;
    rane::x64::Emitter emitter;

    void gen_function(IRFunc& F, const std::string& outPathPrefix) {
        rane::x64::Emitter e;
        int nTemps = F.next_temp;
        int slotBytes = nTemps * 8;
        int stackReserve = ((slotBytes + 15) / 16) * 16;
        e.prologue(stackReserve);

        auto slot_of = [](int t)->int { return 8 * (t + 1); };

        for (auto &ins : F.insts) {
            switch (ins.op) {
            case IRInst::CONST: {
                int dst = ins.dst; int disp = slot_of(dst);
                if (!ins.sym.empty()) {
                    // Symbolic string/data address: leave zero (linker or appended data will be resolved by loader).
                    e.mov_imm64_to_reg(rane::x64::RAX, 0);
                } else {
                    e.mov_imm64_to_reg(rane::x64::RAX, (uint64_t)ins.imm);
                }
                e.mov_rax_to_stack(disp);
                break;
            }
            case IRInst::FCONST: {
                int dst = ins.dst; int disp = slot_of(dst);
                // move immediate double: move bitpattern into rax then store to stack, later load into xmm when used.
                uint64_t bits;
                static_assert(sizeof(double) == sizeof(uint64_t), "double size");
                std::memcpy(&bits, &ins.fimm, sizeof(double));
                e.mov_imm64_to_reg(rane::x64::RAX, bits);
                e.mov_rax_to_stack(disp);
                break;
            }
            case IRInst::ADD: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.mov_stackslot_to_reg(rane::x64::RAX, slot_of(L));
                e.mov_stackslot_to_reg(rane::x64::RCX, slot_of(R));
                e.add_reg_reg(rane::x64::RAX, rane::x64::RCX);
                e.mov_rax_to_stack(slot_of(dst));
                break;
            }
            case IRInst::SUB: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.mov_stackslot_to_reg(rane::x64::RAX, slot_of(L));
                e.mov_stackslot_to_reg(rane::x64::RCX, slot_of(R));
                e.sub_reg_reg(rane::x64::RAX, rane::x64::RCX);
                e.mov_rax_to_stack(slot_of(dst));
                break;
            }
            case IRInst::MUL: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.mov_stackslot_to_reg(rane::x64::RAX, slot_of(L));
                e.mov_stackslot_to_reg(rane::x64::RCX, slot_of(R));
                e.imul_reg_reg(rane::x64::RAX, rane::x64::RCX);
                e.mov_rax_to_stack(slot_of(dst));
                break;
            }
            case IRInst::DIV: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.mov_stackslot_to_reg(rane::x64::RAX, slot_of(L));
                e.mov_stackslot_to_reg(rane::x64::RCX, slot_of(R));
                e.idiv_reg(rane::x64::RCX);
                e.mov_rax_to_stack(slot_of(dst));
                break;
            }
            case IRInst::FADD: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.movsd_load_xmm_from_stack(0, slot_of(L));
                e.movsd_load_xmm_from_stack(1, slot_of(R));
                e.addsd_xmm_xmm(0,1);
                e.movsd_store_xmm_to_stack(0, slot_of(dst));
                break;
            }
            case IRInst::FSUB: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.movsd_load_xmm_from_stack(0, slot_of(L));
                e.movsd_load_xmm_from_stack(1, slot_of(R));
                e.subsd_xmm_xmm(0,1);
                e.movsd_store_xmm_to_stack(0, slot_of(dst));
                break;
            }
            case IRInst::FMUL: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.movsd_load_xmm_from_stack(0, slot_of(L));
                e.movsd_load_xmm_from_stack(1, slot_of(R));
                e.mulsd_xmm_xmm(0,1);
                e.movsd_store_xmm_to_stack(0, slot_of(dst));
                break;
            }
            case IRInst::FDIV: {
                int dst = ins.dst, L = ins.lhs, R = ins.rhs;
                e.movsd_load_xmm_from_stack(0, slot_of(L));
                e.movsd_load_xmm_from_stack(1, slot_of(R));
                e.divsd_xmm_xmm(0,1);
                e.movsd_store_xmm_to_stack(0, slot_of(dst));
                break;
            }
            case IRInst::ALLOC: {
                // Stub: no runtime malloc here; give zero pointer placeholder
                if (ins.dst >= 0) {
                    e.mov_imm64_to_reg(rane::x64::RAX, 0);
                    e.mov_rax_to_stack(slot_of(ins.dst));
                }
                break;
            }
            case IRInst::FREE: {
                // free is a side-effect; no codegen performed here
                break;
            }
            case IRInst::CLOSURE: {
                // produce opaque closure handle (zero)
                if (ins.dst >= 0) {
                    e.mov_imm64_to_reg(rane::x64::RAX, 0);
                    e.mov_rax_to_stack(slot_of(ins.dst));
                }
                break;
            }
            case IRInst::MATCH: {
                // conservative: produce zero result (pattern matching runtime not emitted)
                if (ins.dst >= 0) {
                    e.mov_imm64_to_reg(rane::x64::RAX, 0);
                    e.mov_rax_to_stack(slot_of(ins.dst));
                }
                break;
            }
            case IRInst::PRINT: {
                // Stub: no runtime available; write code to return 0; but preserve instruction for tooling
                break;
            }
            case IRInst::CALL: {
                if (ins.dst >= 0) {
                    e.mov_imm64_to_reg(rane::x64::RAX, 0);
                    e.mov_rax_to_stack(slot_of(ins.dst));
                }
                break;
            }
            case IRInst::RET: {
                if (ins.lhs >= 0) {
                    // decide if it's float or int by checking previous instruction
                    bool isFloat = false;
                    for (auto &i : F.insts) {
                        if (i.dst == ins.lhs && i.op == IRInst::FCONST) { isFloat = true; break; }
                    }
                    if (isFloat) {
                        // load float from slot into xmm0, then move to rax bitwise for return (simplified)
                        e.movsd_load_xmm_from_stack(0, slot_of(ins.lhs));
                        // move xmm0 to rax: movq rax, xmm0 -> 66 48 0F D6 C0 (complex). For simplicity return 0.
                        e.mov_imm64_to_reg(rane::x64::RAX, 0);
                    } else {
                        e.mov_stackslot_to_reg(rane::x64::RAX, slot_of(ins.lhs));
                    }
                } else e.mov_imm64_to_reg(rane::x64::RAX, 0);
                e.epilogue();
                e.write_to_file(outPathPrefix + "_" + F.name + ".bin");
                // Append datasec if present
                if (M && !M->datasec.empty()) {
                    std::ofstream out(outPathPrefix + "_" + F.name + ".bin", std::ios::binary | std::ios::app);
                    for (auto &d : M->datasec) {
                        out.put(0);
                        out.write(d.second.c_str(), (std::streamsize)d.second.size());
                    }
                }
                return;
            }
            default:
                break;
            }
        }
        e.mov_imm64_to_reg(rane::x64::RAX, 0);
        e.epilogue();
        e.write_to_file(outPathPrefix + "_" + F.name + ".bin");
        if (M && !M->datasec.empty()) {
            std::ofstream out(outPathPrefix + "_" + F.name + ".bin", std::ios::binary | std::ios::app);
            for (auto &d : M->datasec) {
                out.put(0);
                out.write(d.second.c_str(), (std::streamsize)d.second.size());
            }
        }
    }

    void emit_all(IRModule& M, const std::string& prefix) {
        // Optionally parallelize per-function emission for speed.
        std::vector<std::thread> threads;
        for (auto &f : M.funcs) {
            threads.emplace_back([this, &f, prefix](){
                gen_function(const_cast<IRFunc&>(f), prefix);
            });
        }
        for (auto &t : threads) if (t.joinable()) t.join();
    }
};

// -------------------------------- Driver -----------------------------------

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr << "usage: ranecc <syntax.rane> <user.rane> [--opt-level N] [--out-prefix prefix]\n";
            return 2;
        }

        int opt_level = 2;
        std::string out_prefix = "rane_output";
        std::string syntaxPath = argv[1];
        std::string userPath = argv[2];

        // parse optional args
        for (int i = 3; i < argc; ++i) {
            std::string s = argv[i];
            if (s == "--opt-level" && i + 1 < argc) { opt_level = std::stoi(argv[++i]); }
            else if (s == "--out-prefix" && i + 1 < argc) { out_prefix = argv[++i]; }
            else { std::cerr << "unknown arg: " << s << "\n"; }
        }

        auto t0 = std::chrono::high_resolution_clock::now();

        std::string syntaxText = read_file_all(syntaxPath);
        std::string userText = read_file_all(userPath);

        RuleDB rules;
        rules.seed_comprehensive();

        Lexer lex(userText, &rules);
        auto toks = lex.lex_all();

        ciams_run(toks);

        Parser parser(toks);
        Program prog = parser.parse_program();

        // AST-level constant folding (opt)
        if (opt_level > 0) fold_constants_program(prog);

        typecheck_program(prog, rules);

        IRModule M = lower_program_to_ir(prog);

        // IR optimizations
        if (opt_level >= 1) {
            optimize_ir_peephole(M);
            optimize_ir_dead_code(M);
        }

        CodeGen cg; cg.M = &M;
        cg.emit_all(M, out_prefix);

        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = t1 - t0;
        std::cerr << "ok: emitted function binaries (" << out_prefix << "_<proc>.bin) in " << elapsed.count() << "s\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return 1;
    }
}

