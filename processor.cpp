// Processor.cpp
// Supplementary: Enhanced syntax, grammar, capabilities, tooling, techniques, features, assets, and optimizations
// This file is a foundation for advanced processing, analysis, and transformation
// in the Rane Processing Language toolchain. All additions are non-breaking and
// do not interfere with any future or existing code.

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <cctype>
#include <random>
#include <iomanip>
#include <type_traits>

// --- Syntax/Grammar: AST node base for extensible processing ---
struct ProcessorASTNode {
    virtual ~ProcessorASTNode() = default;
    virtual void print(std::ostream& os, int indent = 0) const = 0;
    // New: Accept visitor for extensibility
    virtual void accept(class ProcessorASTVisitor& v) const = 0;
};

using ProcessorASTNodePtr = std::shared_ptr<ProcessorASTNode>;

// Example: Expression node
struct ProcessorExprNode : public ProcessorASTNode {
    std::string value;
    std::vector<ProcessorASTNodePtr> children;

    ProcessorExprNode(const std::string& v, const std::vector<ProcessorASTNodePtr>& c)
        : value(v), children(c) {}

    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Expr: " << value << "\n";
        for (const auto& c : children) c->print(os, indent + 2);
    }
    void accept(class ProcessorASTVisitor& v) const override;
};

// Statement node
struct ProcessorStmtNode : public ProcessorASTNode {
    std::string kind;
    std::vector<ProcessorASTNodePtr> children;
    ProcessorStmtNode(const std::string& k, const std::vector<ProcessorASTNodePtr>& c)
        : kind(k), children(c) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Stmt: " << kind << "\n";
        for (const auto& c : children) c->print(os, indent + 2);
    }
    void accept(class ProcessorASTVisitor& v) const override;
};

// Attribute/annotation node
struct ProcessorAttrNode : public ProcessorASTNode {
    std::string name;
    std::vector<std::string> args;
    ProcessorAttrNode(const std::string& n, const std::vector<std::string>& a) : name(n), args(a) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Attr: @" << name;
        if (!args.empty()) {
            os << "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) os << ", ";
                os << args[i];
            }
            os << ")";
        }
        os << "\n";
    }
    void accept(class ProcessorASTVisitor& v) const override;
};

// --- Visitor pattern for AST traversal/analysis/optimization ---
struct ProcessorASTVisitor {
    virtual void visit(const ProcessorExprNode& node) {}
    virtual void visit(const ProcessorStmtNode& node) {}
    virtual void visit(const ProcessorAttrNode& node) {}
    virtual void visit(const ProcessorASTNode& node) {}
    virtual ~ProcessorASTVisitor() = default;
};
void ProcessorExprNode::accept(ProcessorASTVisitor& v) const { v.visit(*this); }
void ProcessorStmtNode::accept(ProcessorASTVisitor& v) const { v.visit(*this); }
void ProcessorAttrNode::accept(ProcessorASTVisitor& v) const { v.visit(*this); }

// --- Grammar: Simple parser combinator utilities ---
using ProcessorToken = std::string;
using ProcessorTokenStream = std::vector<ProcessorToken>;

class ProcessorParser {
public:
    ProcessorParser(const ProcessorTokenStream& tokens) : tokens(tokens), pos(0) {}
    bool match(const std::string& t) { if (pos < tokens.size() && tokens[pos] == t) { ++pos; return true; } return false; }
    bool at_end() const { return pos >= tokens.size(); }
    ProcessorToken peek() const { return at_end() ? "" : tokens[pos]; }
    void rewind(size_t to) { pos = to; }
    size_t position() const { return pos; }
    ProcessorToken lookahead(size_t n = 1) const { return (pos + n < tokens.size()) ? tokens[pos + n] : ""; }
private:
    const ProcessorTokenStream& tokens;
    size_t pos;
};

// --- Capabilities: Tokenizer for whitespace, identifiers, numbers, symbols, string/char literals, comments ---
ProcessorTokenStream processor_tokenize(const std::string& src) {
    ProcessorTokenStream tokens;
    size_t i = 0, n = src.size();
    while (i < n) {
        if (isspace(src[i])) { ++i; continue; }
        // Line comment
        if (src[i] == '/' && i + 1 < n && src[i + 1] == '/') {
            i += 2;
            while (i < n && src[i] != '\n') ++i;
            continue;
        }
        // Block comment
        if (src[i] == '/' && i + 1 < n && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            i += 2;
            continue;
        }
        if (isalpha(src[i]) || src[i] == '_') {
            size_t j = i + 1;
            while (j < n && (isalnum(src[j]) || src[j] == '_')) ++j;
            tokens.push_back(src.substr(i, j - i));
            i = j;
            continue;
        }
        if (isdigit(src[i])) {
            size_t j = i + 1;
            while (j < n && (isdigit(src[j]) || src[j] == '_')) ++j;
            // Floating point
            if (j < n && src[j] == '.') {
                ++j;
                while (j < n && isdigit(src[j])) ++j;
            }
            tokens.push_back(src.substr(i, j - i));
            i = j;
            continue;
        }
        // String literal
        if (src[i] == '"') {
            size_t j = i + 1;
            while (j < n && src[j] != '"') {
                if (src[j] == '\\' && j + 1 < n) ++j;
                ++j;
            }
            if (j < n) ++j;
            tokens.push_back(src.substr(i, j - i));
            i = j;
            continue;
        }
        // Char literal
        if (src[i] == '\'') {
            size_t j = i + 1;
            if (j < n && src[j] == '\\') ++j;
            if (j < n) ++j;
            if (j < n && src[j] == '\'') ++j;
            tokens.push_back(src.substr(i, j - i));
            i = j;
            continue;
        }
        // Symbols (multi-char: '==', '!=', '->', '::', '...', etc.)
        if (i + 2 < n && src.substr(i, 3) == "...") { tokens.push_back("..."); i += 3; continue; }
        if (i + 2 < n && src.substr(i, 3) == "->*") { tokens.push_back("->*"); i += 3; continue; }
        if (i + 2 < n && src.substr(i, 3) == "::=") { tokens.push_back("::="); i += 3; continue; }
        if (i + 2 < n && src.substr(i, 3) == "<=>") { tokens.push_back("<=>"); i += 3; continue; }
        if (i + 1 < n && src.substr(i, 2) == "==") { tokens.push_back("=="); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "!=") { tokens.push_back("!="); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "->") { tokens.push_back("->"); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "::") { tokens.push_back("::"); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "&&") { tokens.push_back("&&"); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "||") { tokens.push_back("||"); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "??") { tokens.push_back("??"); i += 2; continue; }
        if (i + 1 < n && src.substr(i, 2) == "%%") { tokens.push_back("%%"); i += 2; continue; }
        // Single-char symbol
        tokens.push_back(std::string(1, src[i]));
        ++i;
    }
    return tokens;
}

// --- Tooling: AST pretty-printer ---
void processor_print_ast(const ProcessorASTNodePtr& node) {
    if (node) node->print(std::cout, 0);
}

// --- Tooling: AST to JSON (for analysis or tooling) ---
void processor_ast_to_json(const ProcessorASTNodePtr& node, std::ostream& os, int indent = 0) {
    if (!node) { os << "null"; return; }
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto attr = std::dynamic_pointer_cast<ProcessorAttrNode>(node);
    std::string ind(indent, ' ');
    if (expr) {
        os << ind << "{ \"type\": \"Expr\", \"value\": \"" << expr->value << "\", \"children\": [";
        for (size_t i = 0; i < expr->children.size(); ++i) {
            if (i) os << ", ";
            processor_ast_to_json(expr->children[i], os, 0);
        }
        os << "] }";
    } else if (stmt) {
        os << ind << "{ \"type\": \"Stmt\", \"kind\": \"" << stmt->kind << "\", \"children\": [";
        for (size_t i = 0; i < stmt->children.size(); ++i) {
            if (i) os << ", ";
            processor_ast_to_json(stmt->children[i], os, 0);
        }
        os << "] }";
    } else if (attr) {
        os << ind << "{ \"type\": \"Attr\", \"name\": \"" << attr->name << "\", \"args\": [";
        for (size_t i = 0; i < attr->args.size(); ++i) {
            if (i) os << ", ";
            os << "\"" << attr->args[i] << "\"";
        }
        os << "] }";
    } else {
        os << ind << "\"UnknownNode\"";
    }
}

// --- Feature: Source file loader and saver ---
std::string processor_load_file(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
void processor_save_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

// --- Feature: File globbing for batch processing ---
std::vector<std::string> processor_glob(const std::string& pattern) {
    std::vector<std::string> results;
    std::regex re(std::regex_replace(pattern, std::regex(R"(\*)"), ".*"));
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
        if (std::regex_match(entry.path().filename().string(), re))
            results.push_back(entry.path().string());
    }
    return results;
}

// --- Technique: Timer utility for profiling ---
class ProcessorTimer {
public:
    ProcessorTimer() : start(std::chrono::high_resolution_clock::now()) {}
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
private:
    std::chrono::high_resolution_clock::time_point start;
};

// --- Asset: Example built-in grammar for demonstration ---
const char* processor_builtin_grammar = R"(
    expr ::= term (('+' | '-') term)*
    term ::= factor (('*' | '/') factor)*
    factor ::= NUMBER | IDENT | '(' expr ')'
)";

// --- Asset: Syntax coverage (syntax.rane) as a string asset ---
const char* processor_syntax_coverage = R"(
// (Insert the exhaustive syntax.rane content here, or load from file as needed.)
)";

// --- Optimization: String interning for identifiers ---
class ProcessorStringTable {
public:
    const std::string& intern(const std::string& s) {
        auto it = table.find(s);
        if (it != table.end()) return *it->second;
        auto ptr = std::make_shared<std::string>(s);
        table[s] = ptr;
        return *ptr;
    }
private:
    std::map<std::string, std::shared_ptr<std::string>> table;
};

// --- Advanced: Parallel processing utility ---
template<typename Func>
void processor_parallel_for(size_t count, Func fn) {
    size_t nthreads = std::thread::hardware_concurrency();
    if (nthreads == 0) nthreads = 2;
    std::vector<std::thread> threads;
    std::atomic<size_t> idx(0);
    for (size_t t = 0; t < nthreads; ++t) {
        threads.emplace_back([&]() {
            size_t i;
            while ((i = idx.fetch_add(1)) < count) fn(i);
        });
    }
    for (auto& th : threads) th.join();
}

// --- Feature: Error reporting with line/column info ---
struct ProcessorError {
    std::string message;
    int line = 0, col = 0;
    void print() const {
        std::cerr << "Error at " << line << ":" << col << ": " << message << "\n";
    }
};

// --- Technique: Simple macro expansion (for preprocessor-like features) ---
std::string processor_expand_macros(const std::string& src, const std::map<std::string, std::string>& macros) {
    std::string out = src;
    for (const auto& kv : macros) {
        size_t pos = 0;
        std::string key = "$(" + kv.first + ")";
        while ((pos = out.find(key, pos)) != std::string::npos) {
            out.replace(pos, key.length(), kv.second);
            pos += kv.second.length();
        }
    }
    return out;
}

// --- Asset: Example macro table ---
std::map<std::string, std::string> processor_default_macros = {
    {"DATE", "2026-01-07"},
    {"VERSION", "1.0.0"}
};

// --- Feature: Syntax highlighting (ANSI color demo, with keywords and comments) ---
void processor_print_highlight(const std::string& src) {
    static const std::set<std::string> keywords = {
        "let", "if", "then", "else", "while", "do", "for", "break", "continue", "return", "proc",
        "import", "export", "struct", "enum", "match", "case", "default", "try", "catch", "throw",
        "const", "constexpr", "constinit", "inline", "extern", "static", "public", "private", "protected"
    };
    std::istringstream iss(src);
    std::string word;
    while (iss >> word) {
        if (word.rfind("//", 0) == 0) {
            std::cout << "\033[32m" << word;
            std::string rest;
            std::getline(iss, rest);
            std::cout << rest << "\033[0m ";
            break;
        }
        if (keywords.count(word)) std::cout << "\033[35m" << word << "\033[0m ";
        else if (!word.empty() && std::isdigit(word[0])) std::cout << "\033[36m" << word << "\033[0m ";
        else std::cout << word << " ";
    }
    std::cout << std::endl;
}

// --- Feature: Grammar rule registry for extensibility ---
using ProcessorGrammarRule = std::function<bool(ProcessorParser&, ProcessorASTNodePtr&)>;
std::map<std::string, ProcessorGrammarRule> processor_grammar_rules;

// --- Technique: Register a grammar rule ---
void processor_register_rule(const std::string& name, ProcessorGrammarRule rule) {
    processor_grammar_rules[name] = rule;
}

// --- Example: Register a simple expression rule ---
void processor_register_builtin_rules() {
    processor_register_rule("number", [](ProcessorParser& p, ProcessorASTNodePtr& out) {
        if (!p.at_end() && isdigit(p.peek()[0])) {
            out = std::make_shared<ProcessorExprNode>(ProcessorExprNode{p.peek(), {}});
            p.match(p.peek());
            return true;
        }
        return false;
    });
}

// --- Tooling: Batch file processor for syntax checking or transformation ---
void processor_batch_process(const std::vector<std::string>& files, std::function<void(const std::string&)> fn) {
    for (const auto& file : files) {
        std::string content = processor_load_file(file);
        fn(content);
    }
}

// --- Feature: Reflection-like AST node registry for dynamic construction ---
using ProcessorASTFactory = std::function<ProcessorASTNodePtr(const std::vector<std::string>&)>;
std::map<std::string, ProcessorASTFactory> processor_ast_factories;

void processor_register_ast_factory(const std::string& kind, ProcessorASTFactory fn) {
    processor_ast_factories[kind] = fn;
}

// --- Technique: AST node visitor pattern for analysis/optimization ---
void processor_visit_ast(const ProcessorASTNodePtr& node, ProcessorASTVisitor& visitor) {
    if (!node) return;
    node->accept(visitor);
}

// --- Feature: Linting and static analysis utility ---
void processor_lint(const std::string& src) {
    if (src.find("goto") != std::string::npos) {
        std::cout << "[lint] Warning: use of 'goto' detected.\n";
    }
    if (src.find("var ") != std::string::npos) {
        std::cout << "[lint] Suggestion: use 'let' instead of 'var'.\n";
    }
}

// --- Feature: Metrics and statistics for code analysis ---
struct ProcessorMetrics {
    size_t line_count = 0;
    size_t token_count = 0;
    size_t function_count = 0;
    void print() const {
        std::cout << "Lines: " << line_count << ", Tokens: " << token_count << ", Functions: " << function_count << "\n";
    }
};

ProcessorMetrics processor_analyze(const std::string& src) {
    ProcessorMetrics m;
    std::istringstream iss(src);
    std::string line;
    while (std::getline(iss, line)) {
        ++m.line_count;
        std::istringstream lss(line);
        std::string token;
        while (lss >> token) ++m.token_count;
        if (line.find("proc ") != std::string::npos) ++m.function_count;
    }
    return m;
}

// --- Feature: File watcher for live reload (development tooling) ---
void processor_watch_file(const std::string& path, std::function<void()> on_change) {
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    fs::file_time_type last_write = fs::last_write_time(path);
    while (true) {
        std::this_thread::sleep_for(1000ms);
        auto now = fs::last_write_time(path);
        if (now != last_write) {
            last_write = now;
            on_change();
        }
    }
}

// --- Feature: Preprocessor directive expansion (simple) ---
std::string processor_expand_preprocessor(const std::string& src, const std::map<std::string, std::string>& defines) {
    std::istringstream iss(src);
    std::ostringstream oss;
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("#define") == 0) continue;
        for (const auto& kv : defines) {
            size_t pos = 0;
            while ((pos = line.find(kv.first, pos)) != std::string::npos) {
                line.replace(pos, kv.first.length(), kv.second);
                pos += kv.second.length();
            }
        }
        oss << line << "\n";
    }
    return oss.str();
}

// --- Feature: Asset loader for embedded or external resources ---
std::string processor_load_asset(const std::string& asset_name) {
    std::ifstream f(asset_name);
    if (f) {
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    if (asset_name == "syntax.rane") return processor_syntax_coverage;
    if (asset_name == "builtin_grammar") return processor_builtin_grammar;
    return "";
}

// --- Feature: Randomized test input generator for fuzzing ---
std::string processor_generate_random_code(size_t length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789_+-*/=;:{}[]()<>!&|^%\"' \n";
    std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
    std::string out;
    for (size_t i = 0; i < length; ++i) {
        out += charset[rng() % (sizeof(charset) - 1)];
    }
    return out;
}

// --- Feature: Code minification utility ---
std::string processor_minify(const std::string& src) {
    std::ostringstream oss;
    bool in_string = false;
    for (char c : src) {
        if (c == '"') in_string = !in_string;
        if (!in_string && isspace(c)) continue;
        oss << c;
    }
    return oss.str();
}

// --- Feature: Code formatting utility (basic pretty-printer) ---
std::string processor_format(const std::string& src) {
    std::ostringstream oss;
    int indent = 0;
    bool new_line = true;
    for (char c : src) {
        if (c == '{') {
            oss << " {\n";
            ++indent;
            new_line = true;
        } else if (c == '}') {
            oss << "\n";
            if (indent > 0) --indent;
            oss << std::string(indent * 2, ' ') << "}\n";
            new_line = true;
        } else if (c == ';') {
            oss << ";\n" << std::string(indent * 2, ' ');
            new_line = true;
        } else {
            if (new_line) oss << std::string(indent * 2, ' ');
            oss << c;
            new_line = false;
        }
    }
    return oss.str();
}

// --- End of supplementary features ---

// --- Additional: More syntax, grammar, capabilities, tooling, techniques, features, assets, and optimizations ---
// All additions are non-breaking and do not interfere with any existing code or logic.

#include <iomanip>
#include <type_traits>

// --- Feature: AST node for types (for type-aware analysis) ---
struct ProcessorTypeNode : public ProcessorASTNode {
    std::string type_name;
    std::vector<ProcessorASTNodePtr> params;
    ProcessorTypeNode(const std::string& t, const std::vector<ProcessorASTNodePtr>& p = {})
        : type_name(t), params(p) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Type: " << type_name << "\n";
        for (const auto& c : params) c->print(os, indent + 2);
    }
    void accept(class ProcessorASTVisitor& v) const override;
};

// --- Feature: AST node for literals (for richer grammar) ---
struct ProcessorLiteralNode : public ProcessorASTNode {
    std::string literal;
    ProcessorLiteralNode(const std::string& l) : literal(l) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Literal: " << literal << "\n";
    }
    void accept(class ProcessorASTVisitor& v) const override;
};

// --- Visitor extension for new node types ---
struct ProcessorASTVisitorExt : public ProcessorASTVisitor {
    virtual void visit(const ProcessorTypeNode& node) {}
    virtual void visit(const ProcessorLiteralNode& node) {}
};
void ProcessorTypeNode::accept(ProcessorASTVisitor& v) const {
    if (auto* ext = dynamic_cast<ProcessorASTVisitorExt*>(&v)) ext->visit(*this);
    else v.visit(*this);
}
void ProcessorLiteralNode::accept(ProcessorASTVisitor& v) const {
    if (auto* ext = dynamic_cast<ProcessorASTVisitorExt*>(&v)) ext->visit(*this);
    else v.visit(*this);
}

// --- Feature: AST node for comments (for round-trip formatting) ---
struct ProcessorCommentNode : public ProcessorASTNode {
    std::string text;
    ProcessorCommentNode(const std::string& t) : text(t) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Comment: " << text << "\n";
    }
    void accept(class ProcessorASTVisitor& v) const override { v.visit(*this); }
};

// --- Feature: AST node for preprocessor directives ---
struct ProcessorPreprocNode : public ProcessorASTNode {
    std::string directive;
    ProcessorPreprocNode(const std::string& d) : directive(d) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Preproc: " << directive << "\n";
    }
    void accept(class ProcessorASTVisitor& v) const override { v.visit(*this); }
};

// --- Feature: AST node for error recovery (for robust parsing) ---
struct ProcessorErrorNode : public ProcessorASTNode {
    std::string error;
    ProcessorErrorNode(const std::string& e) : error(e) {}
    void print(std::ostream& os, int indent = 0) const override {
        os << std::string(indent, ' ') << "Error: " << error << "\n";
    }
    void accept(class ProcessorASTVisitor& v) const override { v.visit(*this); }
};

// --- Feature: Grammar rule for type parsing (demo) ---
void processor_register_type_rule() {
    processor_register_rule("type", [](ProcessorParser& p, ProcessorASTNodePtr& out) {
        if (!p.at_end() && (p.peek() == "u32" || p.peek() == "i32" || p.peek() == "string")) {
            out = std::make_shared<ProcessorTypeNode>(p.peek());
            p.match(p.peek());
            return true;
        }
        return false;
    });
}

// --- Feature: Grammar rule for literal parsing (demo) ---
void processor_register_literal_rule() {
    processor_register_rule("literal", [](ProcessorParser& p, ProcessorASTNodePtr& out) {
        if (!p.at_end() && (p.peek().size() > 0 && (p.peek()[0] == '"' || isdigit(p.peek()[0])))) {
            out = std::make_shared<ProcessorLiteralNode>(p.peek());
            p.match(p.peek());
            return true;
        }
        return false;
    });
}

// --- Feature: Grammar rule for comment parsing (demo) ---
void processor_register_comment_rule() {
    processor_register_rule("comment", [](ProcessorParser& p, ProcessorASTNodePtr& out) {
        if (!p.at_end() && p.peek().rfind("//", 0) == 0) {
            out = std::make_shared<ProcessorCommentNode>(p.peek());
            p.match(p.peek());
            return true;
        }
        return false;
    });
}

// --- Feature: Grammar rule for preprocessor parsing (demo) ---
void processor_register_preproc_rule() {
    processor_register_rule("preproc", [](ProcessorParser& p, ProcessorASTNodePtr& out) {
        if (!p.at_end() && p.peek().size() > 0 && p.peek()[0] == '#') {
            out = std::make_shared<ProcessorPreprocNode>(p.peek());
            p.match(p.peek());
            return true;
        }
        return false;
    });
}

// --- Feature: Register all extended rules ---
void processor_register_all_rules() {
    processor_register_builtin_rules();
    processor_register_type_rule();
    processor_register_literal_rule();
    processor_register_comment_rule();
    processor_register_preproc_rule();
}

// --- Feature: AST node graph visualization (DOT format) ---
void processor_ast_to_dot(const ProcessorASTNodePtr& node, std::ostream& os, int& id, int parent = -1) {
    if (!node) return;
    int my_id = id++;
    os << "  n" << my_id << " [label=\"";
    node->print(os, 0);
    os << "\"];\n";
    if (parent >= 0) os << "  n" << parent << " -> n" << my_id << ";\n";
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    if (expr) for (const auto& c : expr->children) processor_ast_to_dot(c, os, id, my_id);
    if (stmt) for (const auto& c : stmt->children) processor_ast_to_dot(c, os, id, my_id);
    if (type) for (const auto& c : type->params) processor_ast_to_dot(c, os, id, my_id);
}

// --- Feature: AST node count utility ---
size_t processor_ast_count(const ProcessorASTNodePtr& node) {
    if (!node) return 0;
    size_t count = 1;
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    if (expr) for (const auto& c : expr->children) count += processor_ast_count(c);
    if (stmt) for (const auto& c : stmt->children) count += processor_ast_count(c);
    if (type) for (const auto& c : type->params) count += processor_ast_count(c);
    return count;
}

// --- Feature: AST depth utility ---
size_t processor_ast_depth(const ProcessorASTNodePtr& node) {
    if (!node) return 0;
    size_t maxd = 0;
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    if (expr) for (const auto& c : expr->children) maxd = std::max(maxd, processor_ast_depth(c));
    if (stmt) for (const auto& c : stmt->children) maxd = std::max(maxd, processor_ast_depth(c));
    if (type) for (const auto& c : type->params) maxd = std::max(maxd, processor_ast_depth(c));
    return 1 + maxd;
}

// --- Feature: AST pretty-printer with color (for terminals) ---
void processor_print_ast_color(const ProcessorASTNodePtr& node, int indent = 0) {
    if (!node) return;
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    auto lit = std::dynamic_pointer_cast<ProcessorLiteralNode>(node);
    auto attr = std::dynamic_pointer_cast<ProcessorAttrNode>(node);
    auto comment = std::dynamic_pointer_cast<ProcessorCommentNode>(node);
    auto preproc = std::dynamic_pointer_cast<ProcessorPreprocNode>(node);
    auto err = std::dynamic_pointer_cast<ProcessorErrorNode>(node);
    std::string ind(indent, ' ');
    if (expr) {
        std::cout << ind << "\033[36mExpr: " << expr->value << "\033[0m\n";
        for (const auto& c : expr->children) processor_print_ast_color(c, indent + 2);
    } else if (stmt) {
        std::cout << ind << "\033[35mStmt: " << stmt->kind << "\033[0m\n";
        for (const auto& c : stmt->children) processor_print_ast_color(c, indent + 2);
    } else if (type) {
        std::cout << ind << "\033[33mType: " << type->type_name << "\033[0m\n";
        for (const auto& c : type->params) processor_print_ast_color(c, indent + 2);
    } else if (lit) {
        std::cout << ind << "\033[32mLiteral: " << lit->literal << "\033[0m\n";
    } else if (attr) {
        std::cout << ind << "\033[34mAttr: @" << attr->name << "\033[0m\n";
    } else if (comment) {
        std::cout << ind << "\033[90mComment: " << comment->text << "\033[0m\n";
    } else if (preproc) {
        std::cout << ind << "\033[95mPreproc: " << preproc->directive << "\033[0m\n";
    } else if (err) {
        std::cout << ind << "\033[91mError: " << err->error << "\033[0m\n";
    } else {
        std::cout << ind << "UnknownNode\n";
    }
}

// --- Feature: AST error node creation utility ---
ProcessorASTNodePtr processor_make_error_node(const std::string& msg) {
    return std::make_shared<ProcessorErrorNode>(msg);
}

// --- Feature: AST node hashing (for deduplication/caching) ---
size_t processor_ast_hash(const ProcessorASTNodePtr& node) {
    if (!node) return 0;
    std::hash<std::string> h;
    size_t result = 0;
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    if (expr) {
        result ^= h(expr->value);
        for (const auto& c : expr->children) result ^= processor_ast_hash(c);
    } else if (stmt) {
        result ^= h(stmt->kind);
        for (const auto& c : stmt->children) result ^= processor_ast_hash(c);
    } else if (type) {
        result ^= h(type->type_name);
        for (const auto& c : type->params) result ^= processor_ast_hash(c);
    }
    return result;
}

// --- Feature: AST node equality (for testing/analysis) ---
bool processor_ast_equal(const ProcessorASTNodePtr& a, const ProcessorASTNodePtr& b) {
    if (!a || !b) return a == b;
    if (typeid(*a) != typeid(*b)) return false;
    auto ea = std::dynamic_pointer_cast<ProcessorExprNode>(a);
    auto eb = std::dynamic_pointer_cast<ProcessorExprNode>(b);
    if (ea && eb) {
        if (ea->value != eb->value || ea->children.size() != eb->children.size()) return false;
        for (size_t i = 0; i < ea->children.size(); ++i)
            if (!processor_ast_equal(ea->children[i], eb->children[i])) return false;
        return true;
    }
    auto sa = std::dynamic_pointer_cast<ProcessorStmtNode>(a);
    auto sb = std::dynamic_pointer_cast<ProcessorStmtNode>(b);
    if (sa && sb) {
        if (sa->kind != sb->kind || sa->children.size() != sb->children.size()) return false;
        for (size_t i = 0; i < sa->children.size(); ++i)
            if (!processor_ast_equal(sa->children[i], sb->children[i])) return false;
        return true;
    }
    auto ta = std::dynamic_pointer_cast<ProcessorTypeNode>(a);
    auto tb = std::dynamic_pointer_cast<ProcessorTypeNode>(b);
    if (ta && tb) {
        if (ta->type_name != tb->type_name || ta->params.size() != tb->params.size()) return false;
        for (size_t i = 0; i < ta->params.size(); ++i)
            if (!processor_ast_equal(ta->params[i], tb->params[i])) return false;
        return true;
    }
    return false;
}

// --- Feature: AST node pretty-printer to HTML (for web tooling) ---
void processor_ast_to_html(const ProcessorASTNodePtr& node, std::ostream& os, int indent = 0) {
    if (!node) return;
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    auto lit = std::dynamic_pointer_cast<ProcessorLiteralNode>(node);
    auto attr = std::dynamic_pointer_cast<ProcessorAttrNode>(node);
    auto comment = std::dynamic_pointer_cast<ProcessorCommentNode>(node);
    auto preproc = std::dynamic_pointer_cast<ProcessorPreprocNode>(node);
    auto err = std::dynamic_pointer_cast<ProcessorErrorNode>(node);
    std::string ind(indent * 2, ' ');
    if (expr) {
        os << ind << "<span class='expr'>Expr: " << expr->value << "</span><br/>\n";
        for (const auto& c : expr->children) processor_ast_to_html(c, os, indent + 1);
    } else if (stmt) {
        os << ind << "<span class='stmt'>Stmt: " << stmt->kind << "</span><br/>\n";
        for (const auto& c : stmt->children) processor_ast_to_html(c, os, indent + 1);
    } else if (type) {
        os << ind << "<span class='type'>Type: " << type->type_name << "</span><br/>\n";
        for (const auto& c : type->params) processor_ast_to_html(c, os, indent + 1);
    } else if (lit) {
        os << ind << "<span class='literal'>Literal: " << lit->literal << "</span><br/>\n";
    } else if (attr) {
        os << ind << "<span class='attr'>Attr: @" << attr->name << "</span><br/>\n";
    } else if (comment) {
        os << ind << "<span class='comment'>Comment: " << comment->text << "</span><br/>\n";
    } else if (preproc) {
        os << ind << "<span class='preproc'>Preproc: " << preproc->directive << "</span><br/>\n";
    } else if (err) {
        os << ind << "<span class='error'>Error: " << err->error << "</span><br/>\n";
    } else {
        os << ind << "<span class='unknown'>UnknownNode</span><br/>\n";
    }
}

// --- Feature: AST statistics (node type histogram) ---
std::map<std::string, size_t> processor_ast_histogram(const ProcessorASTNodePtr& node) {
    std::map<std::string, size_t> hist;
    std::function<void(const ProcessorASTNodePtr&)> walk = [&](const ProcessorASTNodePtr& n) {
        if (!n) return;
        std::string t = typeid(*n).name();
        hist[t]++;
        auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(n);
        auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(n);
        auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(n);
        if (expr) for (const auto& c : expr->children) walk(c);
        if (stmt) for (const auto& c : stmt->children) walk(c);
        if (type) for (const auto& c : type->params) walk(c);
    };
    walk(node);
    return hist;
}

// --- Feature: AST random generator (for fuzzing/robustness) ---
ProcessorASTNodePtr processor_generate_random_ast(size_t depth = 3) {
    static const std::vector<std::string> exprs = {"+", "-", "*", "/", "call", "id", "val"};
    static const std::vector<std::string> stmts = {"let", "if", "while", "return"};
    static std::mt19937 rng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
    if (depth == 0) {
        return std::make_shared<ProcessorLiteralNode>(std::to_string(rng() % 100));
    }
    if (rng() % 2) {
        std::vector<ProcessorASTNodePtr> kids;
        for (size_t i = 0; i < (rng() % 3); ++i)
            kids.push_back(processor_generate_random_ast(depth - 1));
        return std::make_shared<ProcessorExprNode>(exprs[rng() % exprs.size()], kids);
    } else {
        std::vector<ProcessorASTNodePtr> kids;
        for (size_t i = 0; i < (rng() % 2); ++i)
            kids.push_back(processor_generate_random_ast(depth - 1));
        return std::make_shared<ProcessorStmtNode>(stmts[rng() % stmts.size()], kids);
    }
}

// --- Feature: AST deep copy utility ---
ProcessorASTNodePtr processor_ast_deep_copy(const ProcessorASTNodePtr& node) {
    if (!node) return nullptr;
    auto expr = std::dynamic_pointer_cast<ProcessorExprNode>(node);
    if (expr) {
        std::vector<ProcessorASTNodePtr> kids;
        for (const auto& c : expr->children) kids.push_back(processor_ast_deep_copy(c));
        return std::make_shared<ProcessorExprNode>(expr->value, kids);
    }
    auto stmt = std::dynamic_pointer_cast<ProcessorStmtNode>(node);
    if (stmt) {
        std::vector<ProcessorASTNodePtr> kids;
        for (const auto& c : stmt->children) kids.push_back(processor_ast_deep_copy(c));
        return std::make_shared<ProcessorStmtNode>(stmt->kind, kids);
    }
    auto type = std::dynamic_pointer_cast<ProcessorTypeNode>(node);
    if (type) {
        std::vector<ProcessorASTNodePtr> kids;
        for (const auto& c : type->params) kids.push_back(processor_ast_deep_copy(c));
        return std::make_shared<ProcessorTypeNode>(type->type_name, kids);
    }
    auto lit = std::dynamic_pointer_cast<ProcessorLiteralNode>(node);
    if (lit) return std::make_shared<ProcessorLiteralNode>(lit->literal);
    auto attr = std::dynamic_pointer_cast<ProcessorAttrNode>(node);
    if (attr) return std::make_shared<ProcessorAttrNode>(attr->name, attr->args);
    auto comment = std::dynamic_pointer_cast<ProcessorCommentNode>(node);
    if (comment) return std::make_shared<ProcessorCommentNode>(comment->text);
    auto preproc = std::dynamic_pointer_cast<ProcessorPreprocNode>(node);
    if (preproc) return std::make_shared<ProcessorPreprocNode>(preproc->directive);
    auto err = std::dynamic_pointer_cast<ProcessorErrorNode>(node);
    if (err) return std::make_shared<ProcessorErrorNode>(err->error);
    return nullptr;
}

// --- End of additional features ---

// (existing code remains unchanged below)
