// executor.cpp
// Executes the complete RANE syntax coverage file (syntax.rane) using the RANE toolchain.
// Also embeds the ANTLR4 grammar for the RANE language for documentation, syntax highlighting, and tooling.
// The system will parse, typecheck, lower, optimize, compile, and execute the entire syntax.rane file.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

// --- RANE toolchain includes ---
extern "C" {
#include "rane_parser.h"
#include "rane_typecheck.h"
#include "rane_tir.h"
#include "rane_ssa.h"
#include "rane_regalloc.h"
#include "rane_optimize.h"
#include "rane_aot.h"
#include "rane_vm.h"
#include "rane_rt.h"
}

// --- Embedded ANTLR4 grammar for documentation and tooling ---
// (This is not compiled, but is present for IDEs, documentation, and external tools.)
namespace rane_grammar_doc {
const char* antlr4_grammar = R"ANTLR4(
// ANTLR4 grammar for the RANE bootstrap language as implemented in this repository.
//
// IMPORTANT:
// - This grammar is intended to match the *implemented* parser in `rane_parser.cpp`.
// - The lexer in this repo tokenizes many additional keywords (reserved/planned). They are
//   included here as tokens so editors/highlighters can recognize them, but most are NOT
//   reachable from `stmt` (same as the current C++ parser).
//
// Targets: documentation, syntax highlighting, and external tooling.
// Not currently wired into the compiler build.

grammar Rane;

@header {
#include "rane_parser.h"
}

// Parser Rules
stmt   : expr ';'                  # exprStmt
       | 'let' ID '=' expr ';'    # varDecl
       | 'print' '(' expr ')' ';' # printStmt
       | otherKeywords // Accept other keywords as valid statements
       ;

expr   : ID                     # id
       | INT                    # int
       | STRING                  # string
       | BOOL                   # boolean
       | FLOAT                   # float
       | '(' expr ')'           # grouping
       | '-' expr                 # negation
       | '+' expr                 # logical negation
       | expr op:('*'|'/') expr   # multiplication/division
       | expr op:'+' expr        # addition
       | expr op:'-' expr        # subtraction
       | expr op:'<' expr        # less than
       | expr op:'>' expr        # greater than
       | expr op:'<=' expr       # less than or equal
       | expr op:'>=' expr       # greater than or equal
       | expr '=' expr           # equals
       | expr '!=' expr          # not equals
       ;

otherKeywords
  : 'if' '(' expr ')' '{' stmt+ '}'
  | 'if' '(' expr ')' '{' stmt+ '}' 'else' '{' stmt+ '}'
  | 'while' '(' expr ')' '{' stmt+ '}'
  | 'return' expr? ';'
  | 'break' ';'
  | 'continue' ';'
  ;

ID      : [a-zA-Z_][a-zA-Z0-9_]* ;
INT     : [0-9]+ ;
STRING  : '"' (~["\\] | '\\' .)* '"' ;
BOOL    : 'true' | 'false' ;
FLOAT   : [0-9]+ '.' [0-9]* ([eE] [+\-]? [0-9]+)? ;
COMMENT : '//' ~[\r\n]* -> skip ;
WS      : [ \t\r\n]+ -> skip ;
)ANTLR4";
}

// --- Embedded syntax.rane for documentation and tooling ---
// (This is not compiled, but is present for IDEs, documentation, and external tools.)
namespace rane_syntax_doc {
const char* syntax_rane = R"RANE(
// syntax.rane
// Complete, exhaustive syntax coverage file for the RANE bootstrap compiler in this repo.
// This file serves as a detailed reference for the syntax and structure of RANE programs.
// It includes examples and is used for testing and validating the RANE toolchain.
//
// See also: grammar.g4 (ANTLR grammar for RANE)

// --- Examples ---
// Note: These are just examples. The actual syntax and semantics are defined by the RANE toolchain.
//
// Hello, World! program:
print("Hello, world!");

// Variable declaration and basic types:
let x = 42;
let y = 3.14;
let name = "RANE";
let flag = true;

// Conditional statements:
if (x < 10) {
    print("x is less than 10");
} else {
    print("x is 10 or more");
}

// Loops:
while (x > 0) {
    print(x);
    x = x - 1;
}

// Functions:
let fact = fn(n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
};

print(fact(5));

// --- Syntax Summary ---
// The syntax of RANE programs is designed to be simple and minimalistic, yet powerful.
//
// - Programs are composed of expressions and statements.
// - Expressions can be literals (numbers, strings, booleans), identifiers, and complex
//   expressions using operators and function calls.
// - Statements perform actions (e.g., print, variable declaration, control flow).
//
// For a complete definition of the syntax, see the RANE language specification and the
// implementation in the RANE toolchain.

)RANE";
}

// --- Utility: Load the full syntax.rane file as a string ---
static std::string load_syntax_rane() {
    std::ifstream f("syntax.rane");
    if (!f) {
        std::cerr << "Could not open syntax.rane for reading.\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// --- Main executor: Compile and execute the full syntax.rane ---
int main(int argc, char** argv) {
    // Optionally print grammar or syntax for documentation
    if (argc > 1 && std::string(argv[1]) == "--print-grammar") {
        std::cout << rane_grammar_doc::antlr4_grammar << std::endl;
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--print-syntax") {
        std::cout << rane_syntax_doc::syntax_rane << std::endl;
        return 0;
    }

    std::string src = load_syntax_rane();
    if (src.empty()) {
        std::cerr << "syntax.rane is empty or missing.\n";
        return 1;
    }

    // Parse
    rane_stmt_t* ast = nullptr;
    rane_diag_t diag = {};
    rane_error_t err = rane_parse_source_len_ex(src.c_str(), src.size(), &ast, &diag);
    if (err != RANE_OK) {
        std::cerr << "Parse error: " << diag.message << "\n";
        return 2;
    }

    // Typecheck
    diag = {};
    err = rane_typecheck_ast_ex(ast, &diag);
    if (err != RANE_OK) {
        std::cerr << "Typecheck error: " << diag.message << "\n";
        return 3;
    }

    // Lower to TIR
    rane_tir_module_t tir_mod = {};
    err = rane_lower_ast_to_tir(ast, &tir_mod);
    if (err != RANE_OK) {
        std::cerr << "Lowering error\n";
        return 4;
    }

    // SSA, regalloc, optimize
    rane_build_ssa(&tir_mod);
    rane_allocate_registers(&tir_mod);
    err = rane_optimize_tir_with_level(&tir_mod, 2);
    if (err != RANE_OK) {
        std::cerr << "Optimize error\n";
        return 5;
    }

    // AOT compile
    void* code = nullptr;
    size_t code_size = 0;
    err = rane_aot_compile(&tir_mod, &code, &code_size);
    if (err != RANE_OK || !code) {
        std::cerr << "AOT compile error\n";
        return 6;
    }

    // Execute in VM (if supported)
    rane_vm_t vm = {};
    rane_vm_init(&vm, &tir_mod, code, code_size, nullptr);
    int vm_result = rane_vm_run(&vm, "main", nullptr, 0);
    rane_vm_free(&vm);
    free(code);

    std::cout << "syntax.rane executed with result: " << vm_result << "\n";
    return 0;
}

// executor.cpp
// Executes the complete RANE syntax coverage file (syntax.rane) using the RANE toolchain.
// This file is self-contained and will parse, typecheck, lower, optimize, compile, and execute the entire syntax.rane file.
// It also references the ANTLR grammar for documentation and tooling purposes.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

// --- RANE toolchain includes ---
extern "C" {
#include "rane_parser.h"
#include "rane_typecheck.h"
#include "rane_tir.h"
#include "rane_ssa.h"
#include "rane_regalloc.h"
#include "rane_optimize.h"
#include "rane_aot.h"
#include "rane_vm.h"
#include "rane_rt.h"
}

// --- Documentation: Reference the full syntax and grammar for tooling/highlighting ---
// (These are not compiled, but are present for IDEs, documentation, and external tools.)
#if 0
#include "syntax.rane"
#include "grammar.g4"
#endif

// --- Utility: Load the full syntax.rane file as a string ---
static std::string load_syntax_rane() {
    std::ifstream f("syntax.rane");
    if (!f) {
        std::cerr << "Could not open syntax.rane for reading.\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// --- Main executor: Compile and execute the full syntax.rane ---
int main(int argc, char** argv) {
    std::string src = load_syntax_rane();
    if (src.empty()) {
        std::cerr << "syntax.rane is empty or missing.\n";
        return 1;
    }

    // Parse
    rane_stmt_t* ast = nullptr;
    rane_diag_t diag = {};
    rane_error_t err = rane_parse_source_len_ex(src.c_str(), src.size(), &ast, &diag);
    if (err != RANE_OK) {
        std::cerr << "Parse error: " << diag.message << "\n";
        return 2;
    }

    // Typecheck
    diag = {};
    err = rane_typecheck_ast_ex(ast, &diag);
    if (err != RANE_OK) {
        std::cerr << "Typecheck error: " << diag.message << "\n";
        return 3;
    }

    // Lower to TIR
    rane_tir_module_t tir_mod = {};
    err = rane_lower_ast_to_tir(ast, &tir_mod);
    if (err != RANE_OK) {
        std::cerr << "Lowering error\n";
        return 4;
    }

    // SSA, regalloc, optimize
    rane_build_ssa(&tir_mod);
    rane_allocate_registers(&tir_mod);
    err = rane_optimize_tir_with_level(&tir_mod, 2);
    if (err != RANE_OK) {
        std::cerr << "Optimize error\n";
        return 5;
    }

    // AOT compile
    void* code = nullptr;
    size_t code_size = 0;
    err = rane_aot_compile(&tir_mod, &code, &code_size);
    if (err != RANE_OK || !code) {
        std::cerr << "AOT compile error\n";
        return 6;
    }

    // Execute in VM (if supported)
    rane_vm_t vm = {};
    rane_vm_init(&vm, &tir_mod, code, code_size, nullptr);
    int vm_result = rane_vm_run(&vm, "main", nullptr, 0);
    rane_vm_free(&vm);
    free(code);

    std::cout << "syntax.rane executed with result: " << vm_result << "\n";
    return 0;
}

// executor.cpp
// Executes the complete RANE syntax coverage file (syntax.rane) using the RANE toolchain.
// Also embeds and references the ANTLR4 grammar (grammar.g4) for documentation, syntax highlighting, and tooling.
// The system will parse, typecheck, lower, optimize, compile, and execute the entire syntax.rane file.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

// --- RANE toolchain includes ---
extern "C" {
#include "rane_parser.h"
#include "rane_typecheck.h"
#include "rane_tir.h"
#include "rane_ssa.h"
#include "rane_regalloc.h"
#include "rane_optimize.h"
#include "rane_aot.h"
#include "rane_vm.h"
#include "rane_rt.h"
}

// --- Embedded grammar.g4 for documentation and tooling ---
// (This is not compiled, but is present for IDEs, documentation, and external tools.)
static std::string load_grammar_g4() {
    std::ifstream f("grammar.g4");
    if (!f) {
        std::cerr << "Could not open grammar.g4 for reading.\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// --- Embedded syntax.rane for documentation and tooling ---
// (This is not compiled, but is present for IDEs, documentation, and external tools.)
static std::string load_syntax_rane() {
    std::ifstream f("syntax.rane");
    if (!f) {
        std::cerr << "Could not open syntax.rane for reading.\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// --- Main executor: Compile and execute the full syntax.rane ---
int main(int argc, char** argv) {
    // Optionally print grammar or syntax for documentation
    if (argc > 1 && std::string(argv[1]) == "--print-grammar") {
        std::string grammar = load_grammar_g4();
        if (!grammar.empty())
            std::cout << grammar << std::endl;
        else
            std::cerr << "grammar.g4 is empty or missing.\n";
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--print-syntax") {
        std::string syntax = load_syntax_rane();
        if (!syntax.empty())
            std::cout << syntax << std::endl;
        else
            std::cerr << "syntax.rane is empty or missing.\n";
        return 0;
    }

    std::string src = load_syntax_rane();
    if (src.empty()) {
        std::cerr << "syntax.rane is empty or missing.\n";
        return 1;
    }

    // Parse
    rane_stmt_t* ast = nullptr;
    rane_diag_t diag = {};
    rane_error_t err = rane_parse_source_len_ex(src.c_str(), src.size(), &ast, &diag);
    if (err != RANE_OK) {
        std::cerr << "Parse error: " << diag.message << "\n";
        return 2;
    }

    // Typecheck
    diag = {};
    err = rane_typecheck_ast_ex(ast, &diag);
    if (err != RANE_OK) {
        std::cerr << "Typecheck error: " << diag.message << "\n";
        return 3;
    }

    // Lower to TIR
    rane_tir_module_t tir_mod = {};
    err = rane_lower_ast_to_tir(ast, &tir_mod);
    if (err != RANE_OK) {
        std::cerr << "Lowering error\n";
        return 4;
    }

    // SSA, regalloc, optimize
    rane_build_ssa(&tir_mod);
    rane_allocate_registers(&tir_mod);
    err = rane_optimize_tir_with_level(&tir_mod, 2);
    if (err != RANE_OK) {
        std::cerr << "Optimize error\n";
        return 5;
    }

    // AOT compile
    void* code = nullptr;
    size_t code_size = 0;
    err = rane_aot_compile(&tir_mod, &code, &code_size);
    if (err != RANE_OK || !code) {
        std::cerr << "AOT compile error\n";
        return 6;
    }

    // Execute in VM (if supported)
    rane_vm_t vm = {};
    rane_vm_init(&vm, &tir_mod, code, code_size, nullptr);
    int vm_result = rane_vm_run(&vm, "main", nullptr, 0);
    rane_vm_free(&vm);
    free(code);

    std::cout << "syntax.rane executed with result: " << vm_result << "\n";
    return 0;
}

