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





Current Capabilities of RANE Resolved
What can it do right now today?
•	Parse RANE Source Code: The resolver can lex and parse RANE source code into an Abstract Syntax Tree (AST) using deterministic Pratt parsing.
•	Canonicalize AST: It transforms the AST into a canonical form, applying Contextual Inference Abstraction Macros (CIAM) to desugar syntactic sugar constructs.
•	Generate Intermediate Representation (IR): Converts the canonical AST into a minimal Control Flow Graph (CFG)-like IR.
•	Optimize IR: Applies basic optimizations to the IR, such as removing redundant instructions and simplifying control flow.
•	Generate Machine Code: Translates the optimized IR into x86-64 machine code using a two-pass approach (emitting placeholders and patching relative jumps).
•	Execute Machine Code: Allocates executable memory, loads the generated machine code, and invokes the entry point.
•	Emit Metadata: Produces runtime metadata for guards, capabilities, and execution tracing in both binary and JSON formats.
•	Support Intrinsics: Includes basic intrinsic functions like print for debugging and output.
---
What can it be used for today?
•	Language Experimentation: RANE Resolved can be used to experiment with the RANE language, testing its syntax, semantics, and constructs.
•	Code Execution: It can execute simple RANE programs, including those with arithmetic operations, control flow, and basic intrinsics.
•	Compiler Development: The project serves as a foundation for developing a full-fledged RANE compiler, with modular layers for easy extension.
•	Canonicalization and Optimization: It can canonicalize and optimize RANE programs, making them suitable for further analysis or execution.
•	Educational Purposes: The project can be used to teach concepts like lexical analysis, parsing, AST transformations, IR generation, and code optimization.
---
What can it create today?
•	Canonical Surface Representation: Outputs the canonicalized RANE program as syntax.ciam.rane.
•	Optimized Intermediate Representation: Produces the optimized IR as syntax.opt.ciam.ir.
•	Executable Machine Code: Generates x86-64 machine code for the RANE program.
•	Runtime Metadata: Creates metadata files (syntax.exec.meta) in binary and JSON formats, detailing guards, capabilities, and execution information.
---
What are its current purposes and use cases?
•	Prototyping the RANE Language: The resolver is a prototype for the RANE language, enabling experimentation with its syntax, semantics, and constructs.
•	Testing CIAM Rules: It validates the application of Contextual Inference Abstraction Macros (CIAM) for desugaring syntactic sugar constructs.
•	Code Execution: Allows execution of simple RANE programs, demonstrating the end-to-end pipeline from source to machine code.
•	Compiler Research: Serves as a research tool for exploring compiler design, including AST transformations, IR generation, and code optimization.
•	Educational Tool: Provides a buildable spine for teaching compiler construction and language design.
---
Who will use it today?
•	Language Designers: Developers experimenting with the RANE language or similar domain-specific languages.
•	Compiler Developers: Engineers working on compiler design, optimization, and code generation.
•	Educators and Students: Those teaching or learning about compiler construction, language design, and AOT compilation.
•	Hobbyists: Enthusiasts exploring programming language implementation and low-level code generation.
---
What will it be used for today?
•	Testing RANE Programs: Writing and executing simple RANE programs to validate the language's syntax and semantics.
•	Experimenting with CIAM Rules: Testing the application of CIAM rules for desugaring constructs like with, defer, and Lock.
•	Generating Machine Code: Producing executable machine code for RANE programs and analyzing its performance.
•	Optimizing Code: Applying basic optimizations to RANE programs and observing their effects on execution.
---
When will it be applied as of today?
•	During Development: When experimenting with the RANE language or extending its features.
•	For Testing: When validating the correctness of CIAM rules, IR generation, and code optimization.
•	In Educational Settings: When teaching or learning about compiler construction and language design.
---
Where will it be applied as of today?
•	Development Environments: Used in IDEs like Visual Studio for development and debugging.
•	Educational Institutions: Applied in classrooms or workshops for teaching compiler design.
•	Research Labs: Used in research settings to prototype and test new language features or compiler techniques.
---
How will it be used and applied as of today?
•	Writing RANE Programs: Users will write RANE programs in syntax.ciam.rane files.
•	Running the Resolver: The resolver will be executed from the command line or an IDE, processing the syntax.ciam.rane file through the pipeline.
•	Analyzing Outputs: Users will analyze the outputs (syntax.ciam.rane, syntax.opt.ciam.ir, syntax.exec.meta) to understand the transformations and optimizations applied.
•	Extending the Resolver: Developers will modify the resolver to add new language features, CIAM rules, or optimizations.
---
Why will it be used as of today?
•	Prototyping: To prototype and experiment with the RANE language.
•	Validation: To validate the correctness of the resolver's pipeline, from parsing to execution.
•	Learning: To learn about compiler construction, language design, and AOT compilation.
•	Optimization: To explore code optimization techniques and their impact on performance.
---
What will be required to use it as of today?
•	Development Environment: A C++20-compatible compiler (e.g., GCC, Clang, MSVC) and an IDE like Visual Studio.
•	RANE Programs: syntax.ciam.rane files containing RANE source code.
•	Command-Line Tools: Access to a terminal or command prompt for running the resolver.
•	Basic Knowledge: Familiarity with programming languages, compilers, and command-line tools.
---
What is the expected learning curve as of today?
•	For Language Users:
•	Low: Writing simple RANE programs is straightforward, with a syntax similar to other modern languages.
•	For Developers:
•	Moderate: Extending the resolver requires knowledge of C++20, compiler design, and the RANE language's architecture.
•	For Educators and Students:
•	Moderate to High: Understanding the entire pipeline (lexing, parsing, CIAM, IR, codegen) requires a solid foundation in compiler construction.
---
Conclusion
RANE Resolved is a powerful tool for prototyping, experimenting, and learning about the RANE language and compiler design. While it is still in development, it provides a robust foundation for extending the language, optimizing code, and generating machine code. With its modular pipeline and clear outputs, it is well-suited for both practical use and educational purposes.
