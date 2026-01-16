# 🌳 **1. Full Syntax Family Tree Showing RANE’s Influences**

```
                           ┌──────────────────────────┐
                           │     Human Readability     │
                           └────────────┬─────────────┘
                                        │
         ┌──────────────────────────────┼──────────────────────────────┐
         │                              │                              │
   Python lineage                 ML / Haskell lineage           Lisp / IR lineage
 (indentation, clarity)     (pattern forms, purity cues)   (determinism, structure)

         │                              │                              │
         └───────────────┬──────────────┴───────────────┬─────────────┘
                         │                                │
                         ▼                                ▼

                RANE Surface Syntax (syntax.rane)
                ─────────────────────────────────
                • indentation‑structured
                • no operator precedence
                • explicit branching forms
                • explicit type forms
                • deterministic grammar
                • sugar‑free core

                         │
                         ▼

                RANE Typed CIL (TCIL)
                • canonical IR
                • explicit control flow
                • typed SSA‑like structure

                         │
                         ▼

                RANE Machine Lowering
                • register allocation
                • calling convention
                • deterministic codegen
```

---

# 🔻 **2. Mapping from Syntax → IR → Machine Code**

### **Example: A simple `decide` expression**

### **Syntax (surface)**
```
decide x
    when 0 -> "zero"
    when 1 -> "one"
    else -> "other"
```

### **IR (Typed CIL)**
```
switch x:
    case 0: return "zero"
    case 1: return "one"
    default: return "other"
```

### **Machine Code (conceptual)**
```
cmp r0, 0
je  L_zero
cmp r0, 1
je  L_one
jmp L_other
```

The key idea:  
**Every RANE construct lowers to a single canonical IR form.**  
No ambiguity.

---

# 📘 **3. Syntax File as a Human‑Friendly Guide**

Below is a clean, onboarding‑friendly structure in `syntax.rane`.

### **Lexical Layer**
- Identifiers: letters, digits, `_`, starting with a letter  
- Literals:  
  - integers  
  - floats  
  - strings  
  - booleans  
  - null  

### **Expressions**
- variable reference  
- literal  
- function call  
- field access  
- list and map literals  
- unary and binary forms (explicit, no precedence)  
- parenthesized expressions  

### **Statements**
- let‑binding  
- assignment  
- return  
- expression statement  

### **Blocks**
- indentation defines scope  
- no braces  
- no semicolons  

### **Control Flow**
- `decide` — intent‑level branching  
- `match` — structural pattern matching  
- `switch` — machine‑dispatch branching  

### **Functions**
- explicit parameters  
- explicit return type  
- indentation‑structured body  

### **Modules**
- import statements  
- module declarations  

This rewrite is meant to be read by humans first, machines second.

---

# 🗺️ **4. Diagram of the Grammar Hierarchy**

```
Program
 ├── Module
 │    ├── Import*
 │    └── Declaration*
 │
 ├── Declaration
 │    ├── FunctionDecl
 │    ├── TypeDecl
 │    └── ConstDecl
 │
 ├── Statement
 │    ├── Let
 │    ├── Assign
 │    ├── Return
 │    └── ExprStmt
 │
 ├── Expression
 │    ├── Literal
 │    ├── Identifier
 │    ├── Call
 │    ├── Access
 │    ├── Unary
 │    ├── Binary
 │    └── Grouped
 │
 └── ControlFlow
      ├── Decide
      ├── Match
      └── Switch
```



---

# 🧬 **5. Mapping Syntax Rules → Compiler Stages**

| Syntax Construct | Parser | Resolver | Type Checker | IR Lowering | Codegen |
|------------------|--------|----------|--------------|-------------|---------|
| Identifiers | tokenized | scope resolution | type inference | symbol → IR var | register |
| Literals | tokenized | n/a | typed literal | constant node | immediate |
| Function | AST node | bind name | check params/return | IR function | prologue/epilogue |
| Decide | AST node | n/a | ensure branch types | switch IR | jump table |
| Match | AST node | pattern binding | pattern typing | match IR | compare/jump |
| Switch | AST node | n/a | type of scrutinee | switch IR | jump table |
| Blocks | AST node | scope creation | type of block | IR block | labels |

RANE’s determinism means each construct has **one** lowering path.

---

# 📖 **6. Human‑Friendly Reader’s Guide to `syntax.rane`**

### **How to read the file**
- Top‑to‑bottom: each section builds on the previous one  
- Each rule is a *contract* between the parser and the language  
- Examples clarify intent  
- No rule is optional—everything is explicit  

### **How to extend it**
- Add new constructs by adding new rules  
- Keep grammar deterministic  
- Avoid precedence tables  
- Ensure every new syntax form has a clear IR lowering  
- Update the Typed CIL file in parallel  

### **How to maintain it**
- Treat it like a spec, not a code file  
- Keep sections grouped by conceptual domain  
- Add comments explaining intent, not just mechanics  

---

# 🧭 **7. Beginner’s Learning Path for RANE Syntax**

### **Day 1**
- literals  
- identifiers  
- basic expressions  
- indentation rules  

### **Day 2**
- let‑bindings  
- function definitions  
- calling conventions  

### **Day 3**
- decide / match / switch  
- blocks and scope  

### **Day 4**
- types  
- modules  
- imports  

### **Day 5**
- reading the syntax file  
- writing small programs  
- understanding IR lowering  

This is a gentle but complete onboarding arc.

---

# ⚡ **8. Syntax Cheat Sheet**

### **Bindings**
```
let x = 10
x = x + 1
```

### **Functions**
```
fn add(a: Int, b: Int) -> Int
    return a + b
```

### **Decide**
```
decide x
    when 0 -> "zero"
    else -> "other"
```

### **Match**
```
match value
    case [a, b] -> a + b
```

### **Switch**
```
switch opcode
    case 1 -> handle1()
```

### **Types**
```
Int, Float, Bool, String
List[T], Map[K, V]
```

---

# 🧵 **9. Side‑by‑Side Comparison: RANE vs Python vs Rust vs Haskell**

| Feature | RANE | Python | Rust | Haskell |
|--------|------|--------|------|---------|
| Indentation | required | required | braces | optional |
| Precedence | none | yes | yes | yes |
| Branching forms | decide/match/switch | if/elif | match/if | case/of |
| Type system | explicit | optional | explicit | inferred |
| Determinism | very high | medium | high | high |
| Syntax noise | low | low | medium | medium |
| IR mapping | 1:1 | implicit | explicit | implicit |
| Learning curve | moderate | easy | steep | steep |

RANE sits in a unique quadrant:  
**Python’s readability + Rust’s determinism + Haskell’s clarity, without their complexity.**

---

