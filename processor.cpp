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

// (existing code remains unchanged below)
