# **RANE Language Specification**  
*Version 0.1 — Living Draft*

---

## **0. Introduction**

### **0.1 Purpose of This Document**
This specification defines the RANE programming language: its lexical structure, grammar, type system, semantics, and compilation pipeline.  
All normative rules use **must**, **must not**, **shall**, and **is defined as**.

### **0.2 Design Goals**
- Deterministic execution  
- Explicit capabilities and effects  
- Inspectable multi‑stage compilation  
- Predictable memory and ownership  
- Simple, teachable surface syntax  
- Industrial‑grade ABI stability  

### **0.3 The RANE Pipeline (Normative Overview)**
RANE source code is compiled through the following mandatory stages:

1. **SMD (Structured Module Document)**  
2. **Typed CIL (Core Intermediate Language)**  
3. **OSW (Optimized Structure Web)**  
4. **Backend Codegen → PE/ELF/Mach‑O**

Each stage is a stable, inspectable artifact.

---

# **1. Lexical Structure**

## **1.1 Character Set**
RANE source files use UTF‑8 encoding.

## **1.2 Tokens**
RANE tokens consist of:
- Identifiers  
- Keywords  
- Literals  
- Operators  
- Delimiters  

## **1.3 Identifiers**
Identifiers must match:

```
identifier ::= letter ( letter | digit | '_' )*
```

Identifiers are case‑sensitive.

## **1.4 Keywords**
Reserved keywords include:

```
proc struct enum match with defer async await
return break continue if else while for
requires capability import module
true false null
```

## **1.5 Literals**
- Integer literals  
- Floating‑point literals  
- String literals  
- Boolean literals  
- Null literal  

## **1.6 Comments**
```
line_comment     ::= '//' <any until newline>
block_comment    ::= '/*' <any> '*/'
```

---

# **2. Grammar (Minimal EBNF)**

This section defines the **surface grammar** of RANE.  
This is intentionally minimal — enough to anchor the language without over‑specifying.

## **2.1 Modules**
```
module        ::= { import_decl | top_level_decl }
import_decl   ::= 'import' identifier ('.' identifier)* ';'
```

## **2.2 Top‑Level Declarations**
```
top_level_decl ::= proc_decl
                 | struct_decl
                 | enum_decl
                 | capability_decl
```

## **2.3 Procedures**
```
proc_decl     ::= 'proc' identifier '(' param_list? ')'
                   return_type?
                   requires_clause?
                   block

param_list    ::= param { ',' param }
param         ::= identifier ':' type

return_type   ::= '->' type

requires_clause ::= 'requires' capability_set

capability_set ::= identifier { ',' identifier }
```

## **2.4 Structs**
```
struct_decl   ::= 'struct' identifier '{' struct_field* '}'
struct_field  ::= identifier ':' type ';'
```

## **2.5 Enums**
```
enum_decl     ::= 'enum' identifier '{' enum_variant* '}'
enum_variant  ::= identifier ( '(' type ')' )? ','
```

## **2.6 Statements**
```
statement     ::= block
                | let_stmt
                | assign_stmt
                | expr_stmt
                | return_stmt
                | if_stmt
                | while_stmt
                | for_stmt
                | match_stmt
                | defer_stmt
                | with_stmt

block         ::= '{' statement* '}'

let_stmt      ::= 'let' identifier ':' type '=' expression ';'
assign_stmt   ::= expression '=' expression ';'
expr_stmt     ::= expression ';'
return_stmt   ::= 'return' expression? ';'
```

## **2.7 Expressions**
```
expression    ::= primary_expr
                | expression binary_op expression
                | unary_op expression
                | call_expr
                | member_expr

primary_expr  ::= identifier
                | literal
                | '(' expression ')'

call_expr     ::= expression '(' arg_list? ')'
arg_list      ::= expression { ',' expression }

member_expr   ::= expression '.' identifier
```

## **2.8 Match**
```
match_stmt    ::= 'match' expression '{' match_arm* '}'
match_arm     ::= pattern '=>' block

pattern       ::= identifier
                | literal
                | identifier '(' pattern_list? ')'
pattern_list  ::= pattern { ',' pattern }
```

## **2.9 With / Defer**
```
with_stmt     ::= 'with' expression 'as' identifier block
defer_stmt    ::= 'defer' block
```

---

# **3. Types**

## **3.1 Primitive Types**
- `i32`, `i64`, `f32`, `f64`
- `bool`
- `string`
- `void`

## **3.2 Composite Types**
- Struct types  
- Enum types  
- Function types  
- Pointer types (if included later)

## **3.3 Function Types**
A function type is defined as:

```
fn_type ::= '(' type_list? ')' '->' type capability_annotation?
```

## **3.4 Capability Annotations**
```
capability_annotation ::= '[' capability_set ']'
```

---

# **4. Capability & Effect System**

## **4.1 Capabilities**
A capability is a named symbol declared at top level:

```
capability_decl ::= 'capability' identifier ';'
```

## **4.2 Effect Sets**
Every function type carries an effect set.

## **4.3 Static Rule**
A caller **must** have an effect set that is a superset of the callee’s.

---

# **5. Memory & Ownership Model**

## **5.1 Linear vs Non‑Linear Values**
- Linear values must be used exactly once.  
- Non‑linear values may be copied freely.

## **5.2 Allocation & Deallocation**
Allocation and freeing are explicit operations in Typed CIL.

## **5.3 Borrowing**
Borrowing rules are enforced at the CIL level.

---

# **6. SMD (Structured Module Document)**

## **6.1 Purpose**
SMD is the first IR after parsing.  
It preserves surface structure but resolves names and imports.

## **6.2 Invariants**
- All identifiers are resolved or marked unresolved.  
- No macros or templates.  
- Module structure is explicit.

## **6.3 Node Kinds**
- `Module`  
- `ProcDecl`  
- `StructDecl`  
- `EnumDecl`  
- `Block`  
- `Expr` nodes  

---

# **7. Typed CIL (Core Intermediate Language)**

## **7.1 Purpose**
Typed CIL is a typed, capability‑aware, ownership‑checked IR.

## **7.2 Invariants**
- All types resolved.  
- All effect sets attached.  
- All linearity constraints validated.  
- No syntactic sugar remains.

## **7.3 Core Instructions**
- `allocate`, `free`  
- `borrow`, `release`  
- `call`, `return`  
- `match_lowered`  
- `async_state_machine`  

---

# **8. OSW (Optimized Structure Web)**

## **8.1 Purpose**
OSW is a graph‑structured optimization IR.

## **8.2 Pass Categories**
- Constant folding  
- DCE  
- Match lowering  
- Async lowering  
- Borrow/ownership optimization  
- Backend prep  

---

# **9. Runtime & ABI**

## **9.1 Calling Convention**
RANE uses the platform’s native ABI (MSVC on Windows).

## **9.2 Type Layout**
Structs and enums have stable, deterministic layouts.

## **9.3 Runtime Services**
- Allocation  
- File I/O  
- Network I/O  
- Time  
- Logging  

All gated by capabilities.

---

# **10. Errors & Diagnostics**

## **10.1 Categories**
- Lexical errors  
- Parse errors  
- Type errors  
- Capability violations  
- Ownership violations  
- Backend errors  

## **10.2 Required Diagnostics**
Every error must include:
- Error code  
- Source span  
- Human‑readable message  

---

# **11. Appendix: Full Grammar (To Be Expanded)**

A complete grammar will be maintained here as the language evolves.

---

