Project Overview: RANE Resolved
The RANE Resolved project is a layered Ahead-of-Time (AOT) resolver for the RANE language. It is designed to process RANE source code through multiple stages, transforming it into optimized machine code and executing it. The project is structured to support a modular, extensible pipeline, with each layer handling a specific aspect of the language's compilation and execution.
---
Pipeline Overview
The RANE AOT Resolver pipeline consists of the following layers:
1.	Source Parsing:
•	Lexical analysis and parsing of RANE source code into an Abstract Syntax Tree (AST).
•	Handles full expression grammar with deterministic precedence (Pratt parsing).
2.	Canonical AST + CIAM Desugaring:
•	Converts the AST into a canonical form.
•	Applies Contextual Inference Abstraction Macros (CIAM) to desugar syntactic sugar constructs like with, defer, Lock, etc.
•	Outputs the canonical surface representation (syntax.ciam.rane).
3.	Intermediate Representation (IR):
•	Transforms the canonical AST into a minimal Control Flow Graph (CFG)-like IR.
•	Supports basic operations, calls, and intrinsic functions like print.
4.	Lowering Hooks:
•	Partially implemented hooks for lowering constructs like match, spawn, join, Lock, with, and defer.
5.	Optimized IR Writer:
•	Writes the optimized IR (syntax.opt.ciam.ir) with stable formatting and embedded BNF headers.
6.	Code Generation:
•	Translates the optimized IR into x86-64 machine code.
•	Uses a two-pass approach: emits bytes with placeholders, then patches relative jumps (jmp rel32, jcc rel32).
7.	Executor:
•	Allocates executable memory, loads the generated machine code, and invokes the entry point.
•	Supports runtime metadata for guards, capabilities, and execution tracing.
---
Key Components
1. Lexer
•	Tokenizes the RANE source code into a stream of tokens.
•	Supports indentation-based syntax with indent and DEDENT tokens.
•	Handles keywords, operators, literals, and identifiers.
•	Enforces deterministic rules (e.g., no tabs allowed).
2. Parser
•	Converts the token stream into an Abstract Syntax Tree (AST).
•	Implements Pratt parsing for deterministic operator precedence.
•	Supports constructs like proc, return _Myios::rdbuf, let, if, match, with, defer, etc.
•	Handles both canonical calls (f(a, b)) and sugar calls (f x y).
3. CIAM Engine
•	Applies Contextual Inference Abstraction Macros (CIAM) to desugar syntactic sugar constructs.
•	Rewrites constructs like:
•	defer X → wraps the remaining block in try/finally { X; }.
•	lock m: body → mutex_lock(m); try { body } finally { mutex_unlock(m); }.
•	with openE as f: body → let f = openE; try { body } finally { close(f); }.
•	Outputs the canonical surface representation (syntax.ciam.rane).
4. Intermediate Representation (IR)
•	Represents the program as a minimal CFG-like IR.
•	Supports basic operations (add, sub, mul, div), comparisons, jumps, and calls.
•	Includes intrinsic operations like call.print_i64.
5. Optimizer
•	Applies deterministic transformations to the IR.
•	Removes redundant instructions and simplifies control flow.
6. Code Generator
•	Translates the optimized IR into x86-64 machine code.
•	Uses native stack operations (push, pop) for expression evaluation.
•	Allocates local variables in rbp-based frame slots.
•	Handles comparisons using cmp, SetCC, and movzx.
7. Executor
•	Allocates executable memory for the generated machine code.
•	Loads the code, patches placeholders, and invokes the entry point.
•	Supports runtime metadata for guards, capabilities, and execution tracing.
---
Key Files
1. Rane_resolver.cpp
•	The main implementation file for the RANE AOT Resolver.
•	Implements the pipeline from source parsing to execution.
•	Includes the lexer, parser, CIAM engine, IR generator, optimizer, code generator, and executor.
2. ciam_engine.h
•	Declares the CIAM engine interfaces.
•	Includes functions for desugaring, guard emission, capability requirements, and rule application.
3. ciam_ids.h
•	Defines identifiers and constants used across the CIAM engine.
4. Grammar.ebnf
•	Specifies the Extended Backus-Naur Form (EBNF) grammar for the RANE language.
5. syntax.ciam.rane
•	The canonical surface representation of the program after CIAM desugaring.
6. syntax.opt.ciam.ir
•	The optimized IR representation of the program.
7. ciam_loader_patcher.cpp
•	Handles loading and patching of the generated machine code.
8. ciam_loader_patcher_win.cpp
•	Windows-specific implementation for loading and patching machine code.
9. .gitattributes
•	Configures Git behavior for the repository (e.g., line endings, merge drivers).
10. .gitignore
•	Specifies files and directories to be ignored by Git.
11. CIAM_LOWERING_RULE_TABLE.txt
•	Defines the rules for CIAM desugaring and lowering.
12. License.md
•	Contains the project's license information.
13. disambiguation_companion.txt
•	Provides additional context or notes for disambiguating constructs in the RANE language.
14. actionplan.hpp
•	Likely contains the action plan or roadmap for the project.
15. rane_emitter.hpp
•	Handles the emission of the canonical surface representation and IR.
16. rane_loader_patcher.cpp
•	Implements the loader and patcher for the generated machine code.
---
Current Issues
1. Missing Declarations
•	Functions like ciam_desugar_block, ciam_pass_run, lower_ast_to_ir, and optimize_ir are used but not declared in the appropriate header files.
2. Duplicate Definitions
•	Functions like ciam_require_cap, ciam_emit_guard, ciam_desugar_block, and ciam_pass_run have duplicate definitions in Rane_resolver.cpp.
3. Missing Definitions
•	Functions like rane_host_print_i64 are used but not defined.
4. Build Errors
•	Errors related to missing or duplicate function definitions prevent the project from building successfully.
---
Next Steps
1.	Resolve Missing Declarations:
•	Add declarations for all missing functions in the appropriate header files (e.g., ciam_engine.h).
2.	Remove Duplicate Definitions:
•	Ensure each function is defined only once in Rane_resolver.cpp.
3.	Define Missing Functions:
•	Implement missing functions like rane_host_print_i64.
4.	Refactor Code:
•	Organize the code to ensure all functions are declared and defined in the appropriate files.
5.	Test the Pipeline:
•	Verify that the pipeline works end-to-end, from source parsing to execution.
6.	Optimize Performance:
•	Apply further optimizations to the IR and code generation stages.
---
Conclusion
The RANE Resolved project is a sophisticated AOT resolver for the RANE language, with a modular pipeline that transforms source code into optimized machine code. While the project is well-structured, several issues need to be addressed to ensure successful builds and execution. Once resolved, the project will serve as a robust foundation for extending the RANE language and its tooling.
