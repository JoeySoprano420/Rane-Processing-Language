// Resolver.cpp
// A CIAM-driven Processor → Translator → Executor for a substantial subset of syntax.rane
// C++20 - single-file self-contained resolver for bootstrap use and testing.
//
// - Lexes syntax.rane, runs CIAMS token rewrites, parses a practical subset
//   (imports, mmio region, proc, let, return, expr statements, calls, print, read32/write32,
//    labels, trap/halt, basic goto form). 
// - Translator produces an ActionPlan (sequence of Actions).
// - Executor runs actions deterministically and records a trace.
// - Designed to be pragmatic: deterministic, auditable, and extensible CIAM points.
//
// Notes:
// - This is not a full compiler. It focuses on deterministic resolution of the
//   sample `syntax.rane` surface used by the bootstrap parser and provides
//   clear extension points (CIAM registration, lowering, codegen).
//
// Build: Use your normal Visual Studio toolchain (C++20). Place this file in
// the project and include it in the build.

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace resolver {

    // ----------------------------- Utilities ----------------------------------

    static std::string read_file_all(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("failed to open file: " + path);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static std::string trim_copy(std::string s) {
        auto l = s.find_first_not_of(" \t\r\n");
        if (l == std::string::npos) return "";
        auto r = s.find_last_not_of(" \t\r\n");
        return s.substr(l, r - l + 1);
    }

    static std::string sanitize_filename(std::string s) {
        for (char& c : s) {
            if (!(std::isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-')) c = '_';
        }
        return s;
    }

    // ----------------------------- Lexer / Tokens ------------------------------

    enum class TokenKind { Eof, Ident, Kw, Number, String, Char, Sym, HashIdent };

    struct Token {
        TokenKind kind = TokenKind::Eof;
        std::string lexeme;
        int line = 0, col = 0;
    };

    struct RuleDB {
        std::set<std::string> keywords;
        RuleDB() {
            const char* kws[] = {
                "let","if","then","else","elif","while","do","for","break","continue","return","ret",
                "proc","def","call","into", // added 'into' as keyword
                "import","export","include","exclude","decide","case","default",
                "jump","goto","mark","label","guard","zone","hot","cold","deterministic","repeat","unroll",
                "not","and","or","xor","shl","shr","sar","print","mmio","region","read32","write32",
                "trap","halt","label","module","struct","type","typealias","namespace","true","false","null",
                "choose","max","min","addr","load","store"
            };
            for (auto k : kws) keywords.insert(k);
        }
        bool is_keyword(const std::string& s) const { return keywords.count(s) > 0; }
    };

    class Lexer {
        std::string src;
        size_t i = 0;
        int line = 1, col = 1;
        RuleDB const* rules = nullptr;

        char peek(size_t off = 0) const { return (i + off < src.size()) ? src[i + off] : '\0'; }
        char getch() {
            char c = peek();
            if (!c) return c;
            ++i;
            if (c == '\n') { ++line; col = 1; }
            else ++col;
            return c;
        }
        void skip_ws_and_comments() {
            for (;;) {
                while (std::isspace((unsigned char)peek())) getch();
                if (peek() == '/' && peek(1) == '/') {
                    getch(); getch();
                    while (peek() && peek() != '\n') getch();
                    continue;
                }
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
        Lexer(std::string s, const RuleDB* r) : src(std::move(s)), rules(r) {}

        Token next() {
            skip_ws_and_comments();
            Token t; t.line = line; t.col = col;
            char c = peek();
            if (!c) { t.kind = TokenKind::Eof; return t; }
            if (c == '#') {
                getch();
                t.lexeme.push_back('#');
                while (std::isalnum((unsigned char)peek()) || peek() == '_' || peek() == ':') t.lexeme.push_back(getch());
                t.kind = TokenKind::HashIdent;
                return t;
            }
            if (std::isalpha((unsigned char)c) || c == '_') {
                while (std::isalnum((unsigned char)peek()) || peek() == '_' || peek() == '?') t.lexeme.push_back(getch());
                if (rules && rules->is_keyword(t.lexeme)) t.kind = TokenKind::Kw;
                else t.kind = TokenKind::Ident;
                return t;
            }
            if (std::isdigit((unsigned char)c)) {
                if (c == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
                    t.lexeme.push_back(getch()); t.lexeme.push_back(getch());
                    while (std::isxdigit((unsigned char)peek()) || peek() == '_') t.lexeme.push_back(getch());
                    t.kind = TokenKind::Number; return t;
                }
                if (c == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
                    t.lexeme.push_back(getch()); t.lexeme.push_back(getch());
                    while ((peek() == '0' || peek() == '1' || peek() == '_')) t.lexeme.push_back(getch());
                    t.kind = TokenKind::Number; return t;
                }
                bool seenDot = false;
                while (std::isdigit((unsigned char)peek()) || peek() == '_' || (!seenDot && peek() == '.')) {
                    if (peek() == '.') seenDot = true;
                    t.lexeme.push_back(getch());
                }
                t.kind = TokenKind::Number; return t;
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
                t.kind = isChar ? TokenKind::Char : TokenKind::String;
                return t;
            }
            // two-char symbols
            std::string two; two.push_back(peek()); two.push_back(peek(1));
            if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "&&" || two == "||" ||
                two == "<<" || two == ">>" || two == "->" || two == "::" || two == "=>") {
                t.lexeme = two; getch(); getch(); t.kind = TokenKind::Sym; return t;
            }
            // single char sym
            t.lexeme.push_back(getch()); t.kind = TokenKind::Sym; return t;
        }

        std::vector<Token> lex_all() {
            std::vector<Token> out;
            for (;;) {
                Token t = next();
                if (t.kind == TokenKind::Eof) break;
                out.push_back(std::move(t));
            }
            return out;
        }
    };

    // ----------------------------- CIAMS --------------------------------------
    // Simple CIAM token-level rewrites used by parser to normalize surface.
    // - word->symbol rewrites: and/or/xor/not/shl/shr/sar => &&/||/^/!/<< >> >>
    // - choose max/min fusion: `choose max(a,b)` => Ident `choose_max` '(' ...
    // - convert '=' to '==' in expression contexts (heuristic).

    static void ciams_run(std::vector<Token>& toks) {
        for (auto& t : toks) {
            if (t.kind == TokenKind::Kw) {
                if (t.lexeme == "xor") { t.kind = TokenKind::Sym; t.lexeme = "^"; }
                if (t.lexeme == "and") { t.kind = TokenKind::Sym; t.lexeme = "&&"; }
                if (t.lexeme == "or") { t.kind = TokenKind::Sym; t.lexeme = "||"; }
                if (t.lexeme == "not") { t.kind = TokenKind::Sym; t.lexeme = "!"; }
                if (t.lexeme == "shl") { t.kind = TokenKind::Sym; t.lexeme = "<<"; }
                if (t.lexeme == "shr" || t.lexeme == "sar") { t.kind = TokenKind::Sym; t.lexeme = ">>"; }
            }
        }
        // '=' heuristic to '==' except after 'let'
        for (size_t i = 1; i + 1 < toks.size(); ++i) {
            if (toks[i].kind == TokenKind::Sym && toks[i].lexeme == "=") {
                bool left_is_let = (toks[i - 1].kind == TokenKind::Kw && toks[i - 1].lexeme == "let");
                bool left_is_comma = (toks[i - 1].kind == TokenKind::Sym && toks[i - 1].lexeme == ",");
                if (!left_is_let && !left_is_comma) toks[i].lexeme = "==";
            }
        }
        // choose max/min fusion
        for (size_t i = 2; i + 1 < toks.size(); ++i) {
            if (toks[i - 2].kind == TokenKind::Kw && toks[i - 2].lexeme == "choose" &&
                toks[i - 1].kind == TokenKind::Ident &&
                (toks[i - 1].lexeme == "max" || toks[i - 1].lexeme == "min") &&
                toks[i].kind == TokenKind::Sym && toks[i].lexeme == "(") {
                toks[i - 1].lexeme = std::string("choose_") + toks[i - 1].lexeme;
                toks.erase(toks.begin() + (i - 2));
                i = (i >= 3) ? i - 3 : 0;
            }
        }
    }

    // ----------------------------- AST ----------------------------------------

    struct Expr;
    struct Stmt;
    struct Proc;
    struct Program;

    using ExprPtr = std::unique_ptr<Expr>;
    using StmtPtr = std::unique_ptr<Stmt>;

    struct Expr {
        enum Kind {
            IntLit, FloatLit, StrLit, BoolLit, NullLit, Ident, HashIdent,
            Unary, Binary, Call, Ternary, ArrayLit, Field, Index
        } kind = IntLit;
        std::string text;               // literal text or ident
        std::string op;                 // operator for unary/binary
        std::vector<ExprPtr> args;      // children
        ExprPtr cond;
    };

    struct Stmt {
        enum Kind { Let, Return, ExprStmt, Label, Goto, Trap, Halt, Read32, Write32, CallStmt } kind = ExprStmt;
        std::string name;
        ExprPtr expr;

        ExprPtr cond;
        std::string true_label, false_label;

        std::string call_into_slot;

        // read32/write32 specifics
        std::string mmio_reg;

        // deterministic offset addressing + expr value support
        ExprPtr mmio_offset;   // required for read32/write32
        ExprPtr mmio_value;    // used for write32
    };

    struct Proc {
        std::string name;
        std::vector<std::string> params;
        std::vector<StmtPtr> body;
    };

    struct Program {
        std::vector<Proc> procs;
        std::map<std::string, std::string> env; // mmio register initial values etc
        std::map<std::string, int64_t> numeric_invariants; // e.g., balances
    };

    // ----------------------------- Deep-clone helpers --------------------------

    static ExprPtr clone_expr(const Expr* e) {
        if (!e) return nullptr;
        auto r = std::make_unique<Expr>();
        r->kind = e->kind;
        r->text = e->text;
        r->op = e->op;
        if (e->cond) r->cond = clone_expr(e->cond.get());
        for (const auto& a : e->args) r->args.push_back(clone_expr(a.get()));
        return r;
    }

    static StmtPtr clone_stmt(const Stmt* s) {
        if (!s) return nullptr;
        auto r = std::make_unique<Stmt>();
        r->kind = s->kind;
        r->name = s->name;
        r->call_into_slot = s->call_into_slot;
        r->mmio_reg = s->mmio_reg;
        if (s->mmio_offset) r->mmio_offset = clone_expr(s->mmio_offset.get());
        if (s->mmio_value) r->mmio_value = clone_expr(s->mmio_value.get());
        r->true_label = s->true_label;
        r->false_label = s->false_label;
        r->expr = clone_expr(s->expr.get());
        if (s->cond) r->cond = clone_expr(s->cond.get());
        return r;
    }

    static Proc clone_proc(const Proc& p) {
        Proc r;
        r.name = p.name;
        r.params = p.params;
        r.body.reserve(p.body.size());
        for (const auto& st : p.body) r.body.push_back(clone_stmt(st.get()));
        return r;
    }

    // ----------------------------- Parser -------------------------------------

    struct Parser {
        std::vector<Token> toks;
        size_t p = 0;

        Parser(std::vector<Token> t) : toks(std::move(t)) {}

        const Token& cur() const { static Token eof{ TokenKind::Eof,"",0,0 }; return p < toks.size() ? toks[p] : eof; }
        bool accept(TokenKind k, const std::string& s = "") {
            if (cur().kind == k && (s.empty() || cur().lexeme == s)) { ++p; return true; }
            return false;
        }
        bool accept_kw(const std::string& s) { return accept(TokenKind::Kw, s); }
        bool accept_ident(std::string& out) {
            if (cur().kind == TokenKind::Ident) { out = cur().lexeme; ++p; return true; }
            return false;
        }
        void expect_sym(const std::string& s) { if (!accept(TokenKind::Sym, s)) throw std::runtime_error("expected symbol: " + s + " got " + cur().lexeme); }

        // top-level parser
        Program parse_program() {
            Program prog;
            while (p < toks.size()) {
                if (cur().kind == TokenKind::Kw && cur().lexeme == "import") {
                    // skip import line
                    ++p;
                    if (cur().kind == TokenKind::Ident) ++p;
                    continue;
                }
                if (cur().kind == TokenKind::Kw && cur().lexeme == "mmio") {
                    // mmio region REG from 4096 size 256;
                    ++p;
                    if (cur().kind == TokenKind::Kw && cur().lexeme == "region") ++p;
                    std::string reg;
                    if (cur().kind == TokenKind::Ident) { reg = cur().lexeme; ++p; }
                    // find numeric 'from' and 'size' tokens, but keep simple:
                    // store env registry with placeholder "0"
                    prog.env[reg] = "0";
                    // skip to semicolon
                    while (p < toks.size() && !(cur().kind == TokenKind::Sym && cur().lexeme == ";")) ++p;
                    if (p < toks.size()) ++p;
                    continue;
                }
                if (cur().kind == TokenKind::Kw && cur().lexeme == "proc") {
                    prog.procs.push_back(parse_proc());
                    continue;
                }
                // else consume token
                ++p;
            }
            return prog;
        }

        Proc parse_proc() {
            if (!(cur().kind == TokenKind::Kw && cur().lexeme == "proc")) throw std::runtime_error("expected proc");
            ++p;
            Proc pr;
            if (cur().kind != TokenKind::Ident) throw std::runtime_error("expected proc name");
            pr.name = cur().lexeme; ++p;
            expect_sym("(");
            if (!accept(TokenKind::Sym, ")")) {
                for (;;) {
                    if (cur().kind != TokenKind::Ident) throw std::runtime_error("expected param");
                    pr.params.push_back(cur().lexeme); ++p;
                    if (accept(TokenKind::Sym, ")")) break;
                    expect_sym(",");
                }
            }
            expect_sym("{");
            while (!(cur().kind == TokenKind::Sym && cur().lexeme == "}")) {
                pr.body.push_back(parse_stmt());
            }
            expect_sym("}");
            return pr;
        }

        StmtPtr parse_stmt() {
            // let
            if (cur().kind == TokenKind::Kw && cur().lexeme == "let") {
                ++p;
                if (cur().kind != TokenKind::Ident) throw std::runtime_error("expected ident after let");
                std::string name = cur().lexeme; ++p;
                expect_sym("=");
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Let;
                s->name = name;
                s->expr = parse_expr();
                accept(TokenKind::Sym, ";");
                return s;
            }
            // return
            if (cur().kind == TokenKind::Kw && (cur().lexeme == "return" || cur().lexeme == "ret")) {
                ++p;
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Return;
                s->expr = parse_expr();
                accept(TokenKind::Sym, ";");
                return s;
            }
            // label
            if (cur().kind == TokenKind::Kw && cur().lexeme == "label") {
                ++p;
                std::string lab;
                if (cur().kind == TokenKind::Ident) { lab = cur().lexeme; ++p; }
                accept(TokenKind::Sym, ";");
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Label;
                s->name = lab;
                return s;
            }
            // trap / halt
            if (cur().kind == TokenKind::Kw && cur().lexeme == "trap") {
                ++p;
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Trap;
                // optional number
                if (cur().kind == TokenKind::Number) ++p;
                accept(TokenKind::Sym, ";");
                return s;
            }
            if (cur().kind == TokenKind::Kw && cur().lexeme == "halt") {
                ++p;
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Halt;
                accept(TokenKind::Sym, ";");
                return s;
            }
            // read32 REG, <expr> into x;
            if (cur().kind == TokenKind::Kw && cur().lexeme == "read32") {
                ++p;
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Read32;

                if (cur().kind == TokenKind::Ident) { s->mmio_reg = cur().lexeme; ++p; }

                expect_sym(",");
                s->mmio_offset = parse_expr();

                if (cur().kind == TokenKind::Kw && cur().lexeme == "into") ++p;
                else throw std::runtime_error("expected 'into' in read32");

                if (cur().kind == TokenKind::Ident) { s->name = cur().lexeme; ++p; }
                else throw std::runtime_error("expected destination ident after read32 ... into");

                accept(TokenKind::Sym, ";");
                return s;
            }

            // write32 REG, <expr>, <expr>;
            if (cur().kind == TokenKind::Kw && cur().lexeme == "write32") {
                ++p;
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Write32;

                if (cur().kind == TokenKind::Ident) { s->mmio_reg = cur().lexeme; ++p; }

                expect_sym(",");
                s->mmio_offset = parse_expr();

                expect_sym(",");
                s->mmio_value = parse_expr();

                accept(TokenKind::Sym, ";");
                return s;
            }

            // goto (expr) -> L_true, L_false;
            if (cur().kind == TokenKind::Kw && cur().lexeme == "goto") {
                ++p;
                expect_sym("(");
                auto cond = parse_expr();
                expect_sym(")");
                expect_sym("->");
                std::string tlab, flab;
                if (cur().kind == TokenKind::Ident) { tlab = cur().lexeme; ++p; }
                expect_sym(",");
                if (cur().kind == TokenKind::Ident) { flab = cur().lexeme; ++p; }
                accept(TokenKind::Sym, ";");
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::Goto;
                s->cond = std::move(cond);
                s->true_label = tlab;
                s->false_label = flab;
                return s;
            }

            // call identity(123) into slot 1;
            if (cur().kind == TokenKind::Kw && cur().lexeme == "call") {
                ++p;
                auto call = parse_expr();
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::CallStmt;
                s->expr = std::move(call);
                if (cur().kind == TokenKind::Kw && cur().lexeme == "into") ++p;
                if (cur().kind == TokenKind::Ident) { s->call_into_slot = cur().lexeme; ++p; }
                else if (cur().kind == TokenKind::Number) { s->call_into_slot = cur().lexeme; ++p; }
                accept(TokenKind::Sym, ";");
                return s;
            }
            // generic expression stmt (print/func call/assignment handled by parser->expr)
            {
                auto s = std::make_unique<Stmt>();
                s->kind = Stmt::ExprStmt;
                s->expr = parse_expr();
                accept(TokenKind::Sym, ";");
                return s;
            }
        }

        ExprPtr parse_expr() {
            return parse_bin_rhs(1, parse_unary());
        }

        ExprPtr parse_primary() {
            if (cur().kind == TokenKind::Number) {
                auto e = std::make_unique<Expr>(); e->kind = Expr::IntLit; e->text = cur().lexeme; ++p; return e;
            }
            if (cur().kind == TokenKind::String) {
                auto e = std::make_unique<Expr>(); e->kind = Expr::StrLit; e->text = cur().lexeme; ++p; return e;
            }
            if (cur().kind == TokenKind::Kw && (cur().lexeme == "true" || cur().lexeme == "false")) {
                auto e = std::make_unique<Expr>(); e->kind = Expr::BoolLit; e->text = cur().lexeme; ++p; return e;
            }
            if (cur().kind == TokenKind::Kw && cur().lexeme == "null") {
                auto e = std::make_unique<Expr>(); e->kind = Expr::NullLit; ++p; return e;
            }
            if (cur().kind == TokenKind::HashIdent) {
                auto e = std::make_unique<Expr>(); e->kind = Expr::HashIdent; e->text = cur().lexeme; ++p; return e;
            }
            if (cur().kind == TokenKind::Ident) {
                std::string id = cur().lexeme; ++p;
                // call or ident or qualified
                if (accept(TokenKind::Sym, "(")) {
                    auto call = std::make_unique<Expr>(); call->kind = Expr::Call; call->text = id;
                    if (!accept(TokenKind::Sym, ")")) {
                        for (;;) {
                            call->args.push_back(parse_expr());
                            if (accept(TokenKind::Sym, ")")) break;
                            expect_sym(",");
                        }
                    }
                    return call;
                }
                auto e = std::make_unique<Expr>(); e->kind = Expr::Ident; e->text = id; return e;
            }
            if (accept(TokenKind::Sym, "(")) {
                auto e = parse_expr();
                expect_sym(")");
                return e;
            }
            // fallback zero literal
            auto e = std::make_unique<Expr>(); e->kind = Expr::IntLit; e->text = "0"; return e;
        }

        ExprPtr parse_unary() {
            if (cur().kind == TokenKind::Sym && (cur().lexeme == "!" || cur().lexeme == "-" || cur().lexeme == "~")) {
                std::string op = cur().lexeme; ++p;
                auto rhs = parse_unary();
                auto e = std::make_unique<Expr>(); e->kind = Expr::Unary; e->op = op; e->args.push_back(std::move(rhs)); return e;
            }
            return parse_primary();
        }

        int prec_of(const Token& t) const {
            if (t.kind != TokenKind::Sym) return -1;
            const std::string& op = t.lexeme;
            if (op == "||") return 1;
            if (op == "&&") return 2;
            if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") return 3;
            if (op == "|") return 4;
            if (op == "^") return 5;
            if (op == "&") return 6;
            if (op == "<<" || op == ">>") return 7;
            if (op == "+" || op == "-") return 8;
            if (op == "*" || op == "/" || op == "%") return 9;
            return -1;
        }

        ExprPtr parse_bin_rhs(int minPrec, ExprPtr lhs) {
            for (;;) {
                int prec = (cur().kind == TokenKind::Sym) ? prec_of(cur()) : -1;
                if (prec < minPrec) return lhs;
                Token op = cur(); ++p;
                auto rhs = parse_unary();
                int nextPrec = (cur().kind == TokenKind::Sym) ? prec_of(cur()) : -1;
                if (nextPrec > prec) rhs = parse_bin_rhs(prec + 1, std::move(rhs));
                auto e = std::make_unique<Expr>(); e->kind = Expr::Binary; e->op = op.lexeme;
                e->args.push_back(std::move(lhs)); e->args.push_back(std::move(rhs));
                lhs = std::move(e);
            }
        }
    };

    // ----------------------------- CIAM / Translator ---------------------------

    // ContextFrame and action/plan definitions updated to support control-flow, labels, mmio, traps.

    struct ContextFrame {
        std::string subject;
        std::map<std::string, int64_t> ints;
        std::map<std::string, std::string> strings;
        std::map<std::string, std::string> env; // mmio register values
        std::vector<std::string> trace;

        // control flow / stopping
        bool stop = false;
        int64_t return_value = 0;

        // deterministic trap info
        bool trapped = false;
        int64_t trap_code = 0;
        std::string trap_reason;

        // mmio behavior toggles
        bool mmio_auto_normalize = false;

        void annotate(const std::string& t) { trace.push_back(t); }
    };

    // Action returns optional new instruction pointer (jump target index) or std::nullopt to continue
    struct Action {
        std::string name;
        std::function<std::optional<size_t>(ContextFrame&)> impl;
    };

    struct ActionPlan {
        std::vector<Action> actions;
        std::unordered_map<std::string, size_t> label_to_ip;

        void append(Action&& a) { actions.push_back(std::move(a)); }
    };

    // ----------------------------- Constant-folding helpers -------------------
    // New: perform basic constant folding on AST nodes prior to translation.
    // This reduces runtime work and enables translation-time optimizations.

    static std::optional<int64_t> parse_int_literal_text(const std::string& s) {
        if (s.empty()) return std::nullopt;
        std::string t; for (char c : s) if (c != '_') t.push_back(c);
        try {
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return (int64_t)std::stoll(t, nullptr, 0);
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B')) return (int64_t)std::stoll(t.substr(2), nullptr, 2);
            return (int64_t)std::stoll(t, nullptr, 10);
        }
        catch (...) { return std::nullopt; }
    }

    // Evaluate an expression as a constant if possible. Uses locals for ident resolution.
    static std::optional<int64_t> eval_constant_expr(const Expr* e, const std::map<std::string, int64_t>* locals = nullptr) {
        if (!e) return std::nullopt;
        switch (e->kind) {
        case Expr::IntLit: return parse_int_literal_text(e->text);
        case Expr::BoolLit: return (e->text == "true") ? 1 : 0;
        case Expr::NullLit: return 0;
        case Expr::Ident:
            if (locals) {
                auto it = locals->find(e->text);
                if (it != locals->end()) return it->second;
            }
            return std::nullopt;
        case Expr::Unary: {
            auto a = eval_constant_expr(e->args[0].get(), locals);
            if (!a) return std::nullopt;
            if (e->op == "-") return -*a;
            if (e->op == "!") return (*a == 0) ? 1 : 0;
            if (e->op == "~") return ~(*a);
            return std::nullopt;
        }
        case Expr::Binary: {
            auto a = eval_constant_expr(e->args[0].get(), locals);
            auto b = eval_constant_expr(e->args[1].get(), locals);
            if (!a || !b) return std::nullopt;
            int64_t A = *a, B = *b;

            if (e->op == "+") return A + B;
            if (e->op == "-") return A - B;
            if (e->op == "*") return A * B;
            // Fix: return std::optional<int64_t> explicitly for division/mod to avoid type mismatch
            if (e->op == "/") return (B != 0) ? std::optional<int64_t>(A / B) : std::nullopt;
            if (e->op == "%") return (B != 0) ? std::optional<int64_t>(A % B) : std::nullopt;
            if (e->op == "&") return A & B;
            if (e->op == "|") return A | B;
            if (e->op == "^") return A ^ B;

            // Safe shift handling: require RHS in [0,63] and use unsigned shift for LHS where appropriate.
            if (e->op == "<<") {
                if (B < 0 || B >= 64) return std::nullopt;
                return static_cast<int64_t>(static_cast<uint64_t>(A) << static_cast<unsigned>(B));
            }
            if (e->op == ">>") {
                if (B < 0 || B >= 64) return std::nullopt;
                // Arithmetic right shift: preserve sign
                return (A >> static_cast<unsigned>(B));
            }

            if (e->op == "==") return (A == B) ? 1 : 0;
            if (e->op == "!=") return (A != B) ? 1 : 0;
            if (e->op == "<") return (A < B) ? 1 : 0;
            if (e->op == "<=") return (A <= B) ? 1 : 0;
            if (e->op == ">") return (A > B) ? 1 : 0;
            if (e->op == ">=") return (A >= B) ? 1 : 0;
            if (e->op == "&&") return (A && B) ? 1 : 0;
            if (e->op == "||") return (A || B) ? 1 : 0;
            return std::nullopt;
        }
        case Expr::Call: {
            // fold some builtins: choose_max/min and addr for constant args
            if (e->text == "choose_max" || e->text == "choose_min") {
                if (e->args.size() >= 2) {
                    auto a = eval_constant_expr(e->args[0].get(), locals);
                    auto b = eval_constant_expr(e->args[1].get(), locals);
                    if (!a || !b) return std::nullopt;
                    if (e->text == "choose_max") return (*a > *b) ? *a : *b;
                    return (*a < *b) ? *a : *b;
                }
                return std::nullopt;
            }
            if (e->text == "addr") {
                int64_t acc = 0;
                for (auto& a : e->args) {
                    auto v = eval_constant_expr(a.get(), locals);
                    if (!v) return std::nullopt;
                    acc = acc * 31 + *v;
                }
                return acc;
            }
            return std::nullopt;
        }
        default:
            return std::nullopt;
        }
        return std::nullopt;
    }

    // Replace expression node with a folded constant when possible.
    static bool fold_constants_in_expr(Expr* e, const std::map<std::string, int64_t>* locals = nullptr) {
        if (!e) return false;
        // recurse first
        for (auto& a : e->args) fold_constants_in_expr(a.get(), locals);
        if (e->cond) fold_constants_in_expr(e->cond.get(), locals);
        auto v = eval_constant_expr(e, locals);
        if (v) {
            e->kind = Expr::IntLit;
            e->text = std::to_string(*v);
            e->op.clear();
            e->args.clear();
            e->cond.reset();
            return true;
        }
        return false;
    }

    static void fold_constants_in_stmt(Stmt* s, const std::map<std::string, int64_t>* locals = nullptr) {
        if (!s) return;
        if (s->expr) fold_constants_in_expr(s->expr.get(), locals);
        if (s->mmio_offset) fold_constants_in_expr(s->mmio_offset.get(), locals);
        if (s->mmio_value) fold_constants_in_expr(s->mmio_value.get(), locals);
        if (s->cond) fold_constants_in_expr(s->cond.get(), locals);
    }

    // ----------------------------- Expr evaluator used by actions ------------
    // (unchanged behavior - deterministic evaluation with simple types (ints & strings))
    struct EvalContext {
        ContextFrame* ctx = nullptr;
        std::map<std::string, int64_t>* locals = nullptr;
        std::map<std::string, Proc>* funcs = nullptr;
        int64_t eval_expr(const Expr* e) {
            if (!e) return 0;
            switch (e->kind) {
            case Expr::IntLit: {
                std::string s = e->text;
                // remove underscores
                std::string t; for (char c : s) if (c != '_') t.push_back(c);
                try {
                    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return (int64_t)std::stoll(t, nullptr, 0);
                    if (t.size() > 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B')) return (int64_t)std::stoll(t.substr(2), nullptr, 2);
                    return (int64_t)std::stoll(t, nullptr, 10);
                }
                catch (...) { return 0; }
            }
            case Expr::BoolLit: {
                if (e->text == "true") return 1; return 0;
            }
            case Expr::NullLit: return 0;
            case Expr::Ident: {
                if (locals) {
                    auto it = locals->find(e->text);
                    if (it != locals->end()) return it->second;
                }
                auto it2 = ctx->ints.find(e->text);
                if (it2 != ctx->ints.end()) return it2->second;
                // fallback zero
                return 0;
            }
            case Expr::Unary: {
                int64_t v = eval_expr(e->args[0].get());
                if (e->op == "-") return -v;
                if (e->op == "!") return (v == 0) ? 1 : 0;
                if (e->op == "~") return ~v;
                return v;
            }
            case Expr::Binary: {
                int64_t a = eval_expr(e->args[0].get());
                int64_t b = eval_expr(e->args[1].get());
                if (e->op == "+") return a + b;
                if (e->op == "-") return a - b;
                if (e->op == "*") return a * b;
                if (e->op == "/") return b != 0 ? a / b : 0;
                if (e->op == "%") return b != 0 ? a % b : 0;
                if (e->op == "&") return a & b;
                if (e->op == "|") return a | b;
                if (e->op == "^") return a ^ b;
                if (e->op == "<<") {
                    // Guard RHS and perform unsigned left shift to avoid UB and MSVC diagnostics
                    if (b < 0 || b >= 64) return 0;
                    return static_cast<int64_t>(static_cast<uint64_t>(a) << static_cast<unsigned>(b));
                }
                if (e->op == ">>") {
                    // Guard RHS; arithmetic right shift on signed type is implementation-defined
                    // but on two's complement targets we preserve sign semantics via signed right shift.
                    if (b < 0 || b >= 64) return 0;
                    return (a >> static_cast<unsigned>(b));
                }
                if (e->op == "==") return (a == b) ? 1 : 0;
                if (e->op == "!=") return (a != b) ? 1 : 0;
                if (e->op == "<") return (a < b) ? 1 : 0;
                if (e->op == "<=") return (a <= b) ? 1 : 0;
                if (e->op == ">") return (a > b) ? 1 : 0;
                if (e->op == ">=") return (a >= b) ? 1 : 0;
                if (e->op == "&&") return (a && b) ? 1 : 0;
                if (e->op == "||") return (a || b) ? 1 : 0;
                return 0;
            }
            case Expr::Call: {
                // builtin: print, addr, load, store, choose_max, choose_min
                if (e->text == "print") {
                    // evaluate first arg as string if string literal else int
                    std::string out;
                    if (!e->args.empty()) {
                        auto& arg = e->args[0];
                        if (arg->kind == Expr::StrLit) out = arg->text;
                        else out = std::to_string(eval_expr(arg.get()));
                    }
                    // deterministic output to stdout
                    std::cout << out << std::endl;
                    ctx->annotate(std::string("print:") + out);
                    return 0;
                }
                if (e->text == "choose_max" || e->text == "choose_min") {
                    if (e->args.size() >= 2) {
                        int64_t a = eval_expr(e->args[0].get());
                        int64_t b = eval_expr(e->args[1].get());
                        if (e->text == "choose_max") return (a > b) ? a : b;
                        return (a < b) ? a : b;
                    }
                    return 0;
                }
                if (e->text == "addr") {
                    // return encoded int address: simple fold of args
                    int64_t acc = 0;
                    for (auto& a : e->args) acc = acc * 31 + eval_expr(a.get());
                    return acc;
                }
                if (e->text == "load") {
                    // load(type, addr_expr)
                    if (e->args.size() >= 2) {
                        int64_t addr = eval_expr(e->args[1].get());
                        // deterministic: return ctx.ints[std::to_string(addr)]
                        auto it = ctx->ints.find(std::to_string(addr));
                        if (it != ctx->ints.end()) return it->second;
                        return 0;
                    }
                    return 0;
                }
                if (e->text == "store") {
                    // store(type, addr_expr, value)
                    if (e->args.size() >= 3) {
                        int64_t addr = eval_expr(e->args[1].get());
                        int64_t val = eval_expr(e->args[2].get());
                        ctx->ints[std::to_string(addr)] = val;
                        return val;
                    }
                    return 0;
                }
                // user-defined function call: attempt to find proc and run
                if (funcs) {
                    auto fit = funcs->find(e->text);
                    if (fit != funcs->end()) {
                        // very simple: evaluate args and execute proc body with new locals
                        std::map<std::string, int64_t> fn_locals;
                        for (size_t i = 0; i < fit->second.params.size() && i < e->args.size(); ++i) {
                            fn_locals[fit->second.params[i]] = eval_expr(e->args[i].get());
                        }
                        // interpret statements until return or end
                        int64_t ret = 0;
                        for (auto& stptr : fit->second.body) {
                            if (!stptr) continue;
                            if (stptr->kind == Stmt::Let) {
                                int64_t v = EvalContext{ ctx, &fn_locals, funcs }.eval_expr(stptr->expr.get());
                                fn_locals[stptr->name] = v;
                            }
                            else if (stptr->kind == Stmt::Return) {
                                ret = EvalContext{ ctx, &fn_locals, funcs }.eval_expr(stptr->expr.get());
                                return ret;
                            }
                            else if (stptr->kind == Stmt::ExprStmt) {
                                EvalContext{ ctx, &fn_locals, funcs }.eval_expr(stptr->expr.get());
                            }
                        }
                        return ret;
                    }
                }
                return 0;
            }
            default: return 0;
            }
            return 0;
        }
    };

    // ----------------------------- Lightweight x86-64 emitter stub ------------
    // (unchanged) emits tiny function blobs to illustrate native lowering.

    namespace x64stub {
        using byte = uint8_t;
        struct CodeBuffer {
            std::vector<byte> data;
            void emit(byte b) { data.push_back(b); }
            void emit32(uint32_t x) {
                for (int i = 0; i < 4; ++i) emit((byte)((x >> (i * 8)) & 0xFF));
            }
            void emit64(uint64_t x) {
                for (int i = 0; i < 8; ++i) emit((byte)((x >> (i * 8)) & 0xFF));
            }
            size_t size() const { return data.size(); }
        };

        static void write_blob_to_file(const std::string& path, const CodeBuffer& buf) {
            std::ofstream out(path, std::ios::binary);
            if (!out) throw std::runtime_error("failed to write native stub: " + path);
            out.write(reinterpret_cast<const char*>(buf.data.data()), (std::streamsize)buf.data.size());
        }

        // Emit a tiny function:
        // push rbp
        // mov rbp, rsp
        // mov rax, imm64
        // pop rbp
        // ret
        static void emit_function_return_const(CodeBuffer& cb, uint64_t imm) {
            // push rbp
            cb.emit(0x55);
            // mov rbp, rsp
            cb.emit(0x48); cb.emit(0x89); cb.emit(0xE5);
            // mov rax, imm64
            cb.emit(0x48); cb.emit(0xB8);
            cb.emit64(imm);
            // pop rbp
            cb.emit(0x5D);
            // ret
            cb.emit(0xC3);
        }

        static void write_stub(const std::string& out_prefix, const std::string& proc_name, uint64_t retval = 0) {
            CodeBuffer cb;
            emit_function_return_const(cb, retval);
            std::string fname = sanitize_filename(out_prefix + "_" + proc_name + ".bin");
            write_blob_to_file(fname, cb);
        }
    } // namespace x64stub

    // ----------------------------- Translator (lowering/emit hook) ------------

    // Helpers for mmio deterministic addressing and traps

    static std::string mmio_key(const std::string& reg, int64_t offset) {
        return reg + "@" + std::to_string(offset);
    }

    static int64_t parse_i64_fallback(const std::string& s, int64_t def = 0) {
        try { return (int64_t)std::stoll(s); }
        catch (...) { return def; }
    }

    static bool mmio_is_aligned4(int64_t byte_offset) {
        return (byte_offset % 4) == 0;
    }

    static int64_t mmio_word_index(int64_t byte_offset) {
        return byte_offset / 4;
    }

    static std::string mmio_word_key(const std::string& reg, int64_t word_index) {
        return reg + "@w" + std::to_string(word_index);
    }

    static void mmio_trap(ContextFrame& ctx, int64_t code, const std::string& reason) {
        ctx.trapped = true;
        ctx.trap_code = code;
        ctx.trap_reason = reason;
        ctx.annotate("MMIO_TRAP code=" + std::to_string(code) + " reason=" + reason);
        ctx.stop = true;
    }

    // ----------------------------- Translator -> ActionPlan --------------------

    // Extended capabilities & optimizations implemented below:
    //  - constant folding on AST before translation
    //  - let-folding: if let RHS is constant with known locals, initialize locals at translate-time and omit runtime let
    //  - read32/write32 offset/value precomputation when offsets/values are constant
    //  - expr pre-evaluation (when fully constant) to avoid runtime evaluation
    //  - annotations to make trace more informative

    static ActionPlan translate_proc_to_plan(
        const Proc& proc,
        Program& /*P*/,
        ContextFrame& /*base_ctx*/,
        std::map<std::string, Proc>& procmap
    ) {
        ActionPlan plan;
        auto locals_ptr = std::make_shared<std::map<std::string, int64_t>>();

        // Pre-fold constants inside procedure body using currently-known locals (none initially).
        // Note: we perform a quick fold pass; deeper multi-pass folding could be added.
        for (auto& st : proc.body) {
            fold_constants_in_stmt(st.get(), locals_ptr.get());
        }

        for (size_t si = 0; si < proc.body.size(); ++si) {
            auto& st = proc.body[si];
            if (!st) continue;

            if (st->kind == Stmt::Label) {
                // record label ip = next action index (current size)
                plan.label_to_ip[st->name] = plan.actions.size();

                Action a;
                a.name = proc.name + "::label " + st->name;
                a.impl = [lbl = st->name](ContextFrame& ctx) -> std::optional<size_t> {
                    ctx.annotate("label:" + lbl);
                    return std::nullopt;
                    };
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Let) {
                // Try to fold let at translation time using locals known so far.
                auto pre = eval_constant_expr(st->expr.get(), locals_ptr.get());
                if (pre) {
                    // Initialize local immediately and skip runtime emission.
                    (*locals_ptr)[st->name] = *pre;
                    // annotate plan with a synthetic action that records folding
                    Action a;
                    a.name = proc.name + "::let-folded " + st->name;
                    a.impl = [name = st->name, val = *pre](ContextFrame& ctx) -> std::optional<size_t> {
                        ctx.annotate("let-folded " + name + " = " + std::to_string(val));
                        return std::nullopt;
                    };
                    plan.append(std::move(a));
                    continue;
                }

                Action a;
                a.name = proc.name + "::let " + st->name;
                a.impl = [e = st->expr.get(), locals_ptr, name = st->name, &procmap, action_name = a.name]
                (ContextFrame& ctx) -> std::optional<size_t> {
                    EvalContext ev{ &ctx, locals_ptr.get(), &procmap };
                    int64_t val = ev.eval_expr(e);
                    (*locals_ptr)[name] = val;
                    ctx.annotate(action_name + " = " + std::to_string(val));
                    return std::nullopt;
                    };
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::ExprStmt || st->kind == Stmt::CallStmt) {
                // Try to pre-evaluate expression at translate-time if fully constant.
                auto pre = eval_constant_expr(st->expr.get(), locals_ptr.get());
                if (pre) {
                    Action a;
                    a.name = proc.name + "::expr(const)"; 
                    a.impl = [v = *pre](ContextFrame& ctx) -> std::optional<size_t> {
                        ctx.annotate("expr_const -> " + std::to_string(v));
                        return std::nullopt;
                    };
                    plan.append(std::move(a));
                    continue;
                }

                Action a;
                a.name = proc.name + "::expr";
                a.impl = [e = st->expr.get(), locals_ptr, &procmap](ContextFrame& ctx) -> std::optional<size_t> {
                    EvalContext ev{ &ctx, locals_ptr.get(), &procmap };
                    (void)ev.eval_expr(e);
                    return std::nullopt;
                    };
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Return) {
                // Try to fold return expr if possible
                auto pre = eval_constant_expr(st->expr.get(), locals_ptr.get());
                if (pre) {
                    Action a;
                    a.name = proc.name + "::return(const)";
                    a.impl = [v = *pre](ContextFrame& ctx) -> std::optional<size_t> {
                        ctx.return_value = v;
                        ctx.annotate("return " + std::to_string(v));
                        ctx.stop = true;
                        return std::nullopt;
                    };
                    plan.append(std::move(a));
                    continue;
                }

                Action a;
                a.name = proc.name + "::return";
                a.impl = [e = st->expr.get(), locals_ptr, &procmap](ContextFrame& ctx) -> std::optional<size_t> {
                    EvalContext ev{ &ctx, locals_ptr.get(), &procmap };
                    int64_t v = ev.eval_expr(e);
                    ctx.return_value = v;
                    ctx.annotate("return " + std::to_string(v));
                    ctx.stop = true;
                    return std::nullopt;
                    };
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Trap) {
                Action a;
                a.name = proc.name + "::trap";
                a.impl = [](ContextFrame& ctx) -> std::optional<size_t> {
                    ctx.annotate("trap");
                    ctx.stop = true;
                    return std::nullopt;
                    };
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Halt) {
                Action a;
                a.name = proc.name + "::halt";
                a.impl = [](ContextFrame& ctx) -> std::optional<size_t> {
                    ctx.annotate("halt");
                    ctx.stop = true;
                    return std::nullopt;
                    };
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Read32) {
                // If mmio_offset folds to constant, capture it at translation time for faster runtime.
                auto off_const = eval_constant_expr(st->mmio_offset.get(), locals_ptr.get());
                Action a;
                a.name = proc.name + "::read32 " + st->mmio_reg;
                if (off_const) {
                    int64_t byte_off = *off_const;
                    if (!mmio_is_aligned4(byte_off)) {
                        // Keep runtime behavior but annotate
                        a.impl = [reg = st->mmio_reg, byte_off, dst = st->name](ContextFrame& ctx) -> std::optional<size_t> {
                            ctx.annotate("read32(precomputed_offset) misaligned " + reg + " off=" + std::to_string(byte_off));
                            // follow original behavior: trap unless mmio_auto_normalize
                            if (!ctx.mmio_auto_normalize) {
                                mmio_trap(ctx, 1001, "read32 misaligned byte_offset=" + std::to_string(byte_off) + " reg=" + reg);
                                return std::nullopt;
                            }
                            int64_t norm = (byte_off >= 0) ? (byte_off & ~3LL) : -(((-byte_off) + 3) & ~3LL);
                            int64_t w = mmio_word_index(norm);
                            const std::string key = mmio_word_key(reg, w);
                            ctx.annotate("read32_normalize " + reg + " off=" + std::to_string(byte_off) + " -> " + std::to_string(norm));
                            ctx.annotate("read32 " + reg + " byte_off=" + std::to_string(norm) +
                                " word=" + std::to_string(w));
                            return std::nullopt;

                        };
                    } else {
                        int64_t w = mmio_word_index(byte_off);
                        const std::string key = mmio_word_key(st->mmio_reg, w);
                        a.impl = [key, dst = st->name](ContextFrame& ctx) -> std::optional<size_t> {
                            auto it = ctx.env.find(key);
                            const std::string raw = (it != ctx.env.end()) ? it->second : "0";
                            int64_t val = parse_i64_fallback(raw, 0);
                            ctx.ints[dst] = val;
                            ctx.annotate("read32_const_key " + key + " -> " + dst + " = " + std::to_string(val));
                            return std::nullopt;
                        };
                    }
                } else {
                    // fallback to runtime-evaluated offset
                    a.impl = [reg = st->mmio_reg,
                        offE = st->mmio_offset.get(),
                        dst = st->name,
                        locals_ptr, &procmap, &plan](ContextFrame& ctx) -> std::optional<size_t> {
                        EvalContext ev{ &ctx, locals_ptr.get(), &procmap };
                        int64_t byte_off = ev.eval_expr(offE);

                        if (!mmio_is_aligned4(byte_off)) {
                            if (!ctx.mmio_auto_normalize) {
                                mmio_trap(ctx, 1001, "read32 misaligned byte_offset=" + std::to_string(byte_off) + " reg=" + reg);
                                return std::nullopt;
                            }
                            int64_t norm = (byte_off >= 0) ? (byte_off & ~3LL) : -(((-byte_off) + 3) & ~3LL);
                            ctx.annotate("mmio_normalize " + std::to_string(byte_off) + " -> " + std::to_string(norm));
                            byte_off = norm;
                        }

                        int64_t w = mmio_word_index(byte_off);
                        const std::string key = mmio_word_key(reg, w);

                        auto it = ctx.env.find(key);
                        const std::string raw = (it != ctx.env.end()) ? it->second : "0";
                        int64_t val = parse_i64_fallback(raw, 0);

                        ctx.ints[dst] = val;
                        ctx.annotate("read32 " + reg + " byte_off=" + std::to_string(byte_off) +
                            " word=" + std::to_string(w) + " -> " + dst + " = " + std::to_string(val));
                        return std::nullopt;
                        };
                }
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Write32) {
                // If mmio_offset and mmio_value fold to constants, capture at translation-time
                auto off_const = eval_constant_expr(st->mmio_offset.get(), locals_ptr.get());
                auto val_const = eval_constant_expr(st->mmio_value.get(), locals_ptr.get());
                Action a;
                a.name = proc.name + "::write32 " + st->mmio_reg;
                if (off_const && val_const) {
                    int64_t byte_off = *off_const;
                    int64_t value = *val_const;
                    if (!mmio_is_aligned4(byte_off)) {
                        a.impl = [reg = st->mmio_reg, byte_off, value](ContextFrame& ctx) -> std::optional<size_t> {
                            ctx.annotate("write32(precomputed_offset) misaligned " + reg + " off=" + std::to_string(byte_off));
                            if (!ctx.mmio_auto_normalize) {
                                mmio_trap(ctx, 1002, "write32 misaligned byte_offset=" + std::to_string(byte_off) + " reg=" + reg);
                                return std::nullopt;
                            }
                            int64_t norm = (byte_off >= 0) ? (byte_off & ~3LL) : -(((-byte_off) + 3) & ~3LL);
                            int64_t w = mmio_word_index(norm);
                            const std::string key = mmio_word_key(reg, w);
                            ctx.env[key] = std::to_string(value);
                            ctx.annotate("write32_const " + key + " = " + std::to_string(value));
                            return std::nullopt;
                        };
                    } else {
                        int64_t w = mmio_word_index(byte_off);
                        const std::string key = mmio_word_key(st->mmio_reg, w);
                        a.impl = [key, value](ContextFrame& ctx) -> std::optional<size_t> {
                            ctx.env[key] = std::to_string(value);
                            ctx.annotate("write32_const " + key + " = " + std::to_string(value));
                            return std::nullopt;
                        };
                    }
                } else {
                    a.impl = [reg = st->mmio_reg,
                        offE = st->mmio_offset.get(),
                        valE = st->mmio_value.get(),
                        locals_ptr, &procmap, &plan](ContextFrame& ctx) -> std::optional<size_t> {
                        EvalContext ev{ &ctx, locals_ptr.get(), &procmap };
                        int64_t byte_off = ev.eval_expr(offE);

                        if (!mmio_is_aligned4(byte_off)) {
                            if (!ctx.mmio_auto_normalize) {
                                mmio_trap(ctx, 1002, "write32 misaligned byte_offset=" + std::to_string(byte_off) + " reg=" + reg);
                                return std::nullopt;
                            }
                            int64_t norm = (byte_off >= 0) ? (byte_off & ~3LL) : -(((-byte_off) + 3) & ~3LL);
                            ctx.annotate("mmio_normalize " + std::to_string(byte_off) + " -> " + std::to_string(norm));
                            byte_off = norm;
                        }

                        int64_t w = mmio_word_index(byte_off);
                        int64_t value = ev.eval_expr(valE);

                        const std::string key = mmio_word_key(reg, w);
                        ctx.env[key] = std::to_string(value);

                        ctx.annotate("write32 " + reg + " byte_off=" + std::to_string(byte_off) +
                            " word=" + std::to_string(w) + " = " + std::to_string(value));
                        return std::nullopt;
                        };
                }
                plan.append(std::move(a));
                continue;
            }

            if (st->kind == Stmt::Goto) {
                Action a;
                a.name = proc.name + "::goto";
                a.impl = [cond = st->cond.get(), t = st->true_label, f = st->false_label,
                    locals_ptr, &procmap, &plan](ContextFrame& ctx) -> std::optional<size_t> {
                    EvalContext ev{ &ctx, locals_ptr.get(), &procmap };
                    int64_t v = ev.eval_expr(cond);

                    const std::string& target = v ? t : f;
                    ctx.annotate(std::string("goto ") + (v ? "true" : "false") + " -> " + target);

                    auto it = plan.label_to_ip.find(target);
                    if (it == plan.label_to_ip.end()) {
                        ctx.annotate("goto_target_missing:" + target);
                        ctx.stop = true;
                        return std::nullopt;
                    }
                    return it->second; // jump ip
                    };
                plan.append(std::move(a));
                continue;
            }

            // fallback no-op
            Action a;
            a.name = proc.name + "::nop";
            a.impl = [](ContextFrame&) -> std::optional<size_t> { return std::nullopt; };
            plan.append(std::move(a));
        }

        // Post-translation optimization point:
        // - We could resolve goto targets into direct ips here and patch lambdas,
        //   but current goto lambdas reference plan.label_to_ip and are efficient.
        // - Additional passes can be added to remove redundant no-ops, merge adjacent annotations, etc.

        return plan;
    }

    // ----------------------------- Executor -----------------------------------

    struct ExecStep {
        std::chrono::steady_clock::time_point ts;
        std::string action;
        std::string note;
    };

    struct ExecutionTrace {
        std::vector<ExecStep> steps;
        void push(std::string a, std::string note = "") { steps.push_back({ std::chrono::steady_clock::now(), std::move(a), std::move(note) }); }
    };

    static std::pair<ContextFrame, ExecutionTrace> execute_plan(ActionPlan const& plan, ContextFrame ctx) {
        ExecutionTrace trace;

        size_t ip = 0;
        while (ip < plan.actions.size()) {
            auto const& a = plan.actions[ip];
            try {
                auto jump = a.impl(ctx);
                trace.push(a.name, "ok");
                if (ctx.stop) break;
                if (jump.has_value()) {
                    ip = *jump;
                }
                else {
                    ++ip;
                }
            }
            catch (const std::exception& ex) {
                trace.push(a.name, std::string("ex: ") + ex.what());
                break;
            }
            catch (...) {
                trace.push(a.name, "unknown ex");
                break;
            }
        }

        return { std::move(ctx), std::move(trace) };
    }

    // ----------------------------- End-to-end Resolver API ---------------------

    struct ResolveResult {
        ContextFrame final_ctx;
        ExecutionTrace trace;
    };

    static ResolveResult resolve_and_run(const std::string& source, const std::string& main_proc_name = "main") {
        RuleDB rules;
        Lexer L(source, &rules);
        auto toks = L.lex_all();
        ciams_run(toks);
        Parser P(std::move(toks));
        Program prog = P.parse_program();

        // Run a constant-folding pass over the parsed program (simple, non-recursive locals-aware pass).
        for (auto& pr : prog.procs) {
            // We perform a single pass fold that may reduce many constant sub-expressions.
            std::map<std::string, int64_t> empty_locals;
            for (auto& st : pr.body) fold_constants_in_stmt(st.get(), &empty_locals);
        }

        // build proc map (deep-clone to avoid copying unique_ptrs)
        std::map<std::string, Proc> procmap;
        for (const auto& pr : prog.procs) procmap.emplace(pr.name, clone_proc(pr));

        // initial context frame
        ContextFrame ctx;
        for (auto& kv : prog.env) ctx.env[kv.first] = kv.second;
        for (auto& kv : prog.numeric_invariants) ctx.ints[kv.first] = kv.second;

        // translate selected proc to plan (main)
        auto it = procmap.find(main_proc_name);
        if (it == procmap.end()) throw std::runtime_error("no main proc found");
        ActionPlan plan = translate_proc_to_plan(it->second, prog, ctx, procmap);

        // Lightweight lowering: emit a tiny native stub that returns 0 for the proc.
        try {
            x64stub::write_stub("resolver_native", it->second.name, 0);
        }
        catch (const std::exception& ex) {
            // Emission failure should not stop resolution/execution; annotate context
            ctx.annotate(std::string("native_emit_failed: ") + ex.what());
        }

        // execute plan deterministically
        auto [final_ctx, trace] = execute_plan(plan, ctx);
        return ResolveResult{ std::move(final_ctx), std::move(trace) };
    }

} // namespace resolver

// ----------------------------- small test harness --------------------------

#ifdef RESOLVER_MAIN_TEST
int main(int argc, char** argv) {
    try {
        std::string path = "syntax.rane";
        if (argc > 1) path = argv[1];
        std::string src = resolver::read_file_all(path);
        auto res = resolver::resolve_and_run(src, "main");
        std::cerr << "Execution trace:\n";
        for (auto& s : res.trace.steps) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(s.ts.time_since_epoch()).count();
            std::cerr << "[" << ms << "] " << s.action << " -- " << s.note << "\n";
        }
        std::cerr << "Context traces:\n";
        for (auto& t : res.final_ctx.trace) std::cerr << " - " << t << "\n";
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return 2;
    }
}
#endif

// To build and run the test harness, define RESOLVER_MAIN_TEST in the project settings
// or compile with -DRESOLVER_MAIN_TEST and run the produced binary. The resolver will
// attempt to load `syntax.rane` (or path provided as argv[1]) and execute `proc main()`.

