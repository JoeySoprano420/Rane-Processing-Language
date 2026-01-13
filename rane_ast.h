#// rane_ast.h
// Exact AST node shapes for RANE that satisfy the Lexical Path contract
// (parent/slot/ordinal + tok_pos per node) with zero ambiguity.
//
// Includes:
//   - Core statement/expression/type nodes
//   - Sugar nodes: with/defer/lock/spawn/join/match
//   - Node/Prose mini-AST: node blocks, node statements, start-at
//   - Explicit lowering target shapes: how node-prose becomes normal CFG
//
// Notes:
//   - This is a “shape contract” header. Implementations can add fields,
//     but MUST NOT change: parent linkage, slot, ordinal, tok_pos semantics,
//     and the child lists indicated per node type.
//   - This design uses a single arena of nodes indexed by node_id.
//   - Children are stored as node_id arrays in a NodeStore.
//   - All child lists are validated and ordinals computed by tok_pos byte_offset.
//   - No exceptions required.

#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <vector>
#include <span>
#include <array>
#include <optional>

namespace rane {

    //------------------------------------------------------------------------------
    // Core identifiers and positions
    //------------------------------------------------------------------------------
    using node_id = uint32_t;

    struct tok_pos {
        uint32_t byte_offset = 0; // first token belonging to node
        uint32_t byte_len = 0; // total span length (best effort)
    };

    enum class slot_kind : uint16_t {
        // file/module/namespace/type
        file_items = 1,
        ns_items = 2,
        type_items = 3,

        // procedures
        proc_params = 10,
        proc_requires = 11,
        proc_body = 12,

        // blocks
        block_stmts = 20,

        // statement internal slots
        let_bindings = 30,
        let_init = 31,

        assign_lhs = 32,
        assign_rhs = 33,

        if_cond = 34,
        if_then = 35,
        if_else = 36,

        while_cond = 37,
        while_body = 38,

        for_init = 39,
        for_cond = 40,
        for_step = 41,
        for_body = 42,

        return_expr = 43,
        throw_expr = 44,

        try_body = 45,
        catch_list = 46,
        finally_body = 47,

        // expression internal slots
        call_callee = 60,
        call_args = 61,

        unary_arg = 62,
        binary_lhs = 63,
        binary_rhs = 64,
        ternary_cond = 65,
        ternary_then = 66,
        ternary_else = 67,

        field_base = 68,
        index_base = 69,
        index_expr = 70,

        // sugar
        with_acquire = 90,
        with_binding = 91,
        with_body = 92,

        defer_body = 93,

        lock_target = 95,
        lock_body = 96,

        spawn_callee = 97,
        spawn_args = 98,
        join_target = 99,

        match_scrutinee = 100,
        match_arms = 101,
        match_arm_pat = 102,
        match_arm_guard = 103,
        match_arm_body = 104,

        // node/prose
        node_list = 120,
        node_header = 121,
        node_body = 122,
        node_stmt_list = 123,
        node_start_decl = 124,
    };

    struct node_link {
        node_id parent = 0;        // 0 if root
        slot_kind slot = slot_kind::file_items;
        uint32_t ordinal = 0;      // rank within (parent,slot) list
        tok_pos pos{};
    };

    //------------------------------------------------------------------------------
    // Fundamental enums
    //------------------------------------------------------------------------------
    enum class node_kind : uint16_t {
        // root-ish
        file_unit = 1,
        module_decl,
        namespace_decl,

        // declarations
        import_decl,
        proc_decl,
        struct_decl,
        enum_decl,
        variant_decl,
        union_decl,
        const_decl,
        type_alias_decl,

        // statements
        block_stmt,
        let_stmt,
        assign_stmt,
        if_stmt,
        while_stmt,
        for_stmt,
        return_stmt,
        try_stmt,
        throw_stmt,
        trap_stmt,
        halt_stmt,
        goto_stmt,
        label_stmt,

        // expressions
        ident_expr,
        lit_expr,
        call_expr,
        unary_expr,
        binary_expr,
        ternary_expr,
        field_expr,
        index_expr,
        cast_expr,

        // sugar statements/exprs
        with_stmt,
        defer_stmt,
        lock_stmt,
        spawn_expr,
        join_expr,
        match_stmt,
        match_arm,

        // node/prose
        node_module,
        node_block,
        node_start_at,

        node_stmt_set,
        node_stmt_add,
        node_stmt_say,
        node_stmt_go,
        node_stmt_halt,
        node_stmt_trap,
    };

    //------------------------------------------------------------------------------
    // Type nodes (kept simple and explicit)
    //------------------------------------------------------------------------------
    enum class type_kind : uint8_t {
        named, pointer, array, fn, generic_inst
    };

    struct type_ref {
        // If you store types in another arena, keep a handle here.
        // For the AST contract, we provide an explicit shape:
        type_kind kind = type_kind::named;
        // For named types: symbol id or interned name index. Keep both if you want.
        uint32_t named_sym = 0;      // e.g. "i64", "Header"
        // Pointer: points to elem type
        uint32_t elem_type = 0;      // index in type arena
        // Array:
        uint64_t array_len = 0;
        // Fn:
        uint32_t fn_sig = 0;         // index in signature arena
    };

    //------------------------------------------------------------------------------
    // Common small nodes
    //------------------------------------------------------------------------------
    struct ident {
        uint32_t sym = 0;           // interned symbol id
    };

    enum class lit_kind : uint8_t { i64, u64, f64, boolean, string, null_lit };

    struct lit {
        lit_kind kind = lit_kind::null_lit;
        union {
            int64_t  i64v;
            uint64_t u64v;
            double   f64v;
            uint8_t  bv;
        };
        uint32_t str_sym = 0;       // interned string
    };

    //------------------------------------------------------------------------------
    // Expression nodes (all children as node_id with explicit slots)
    //------------------------------------------------------------------------------
    enum class unary_op : uint8_t { neg, bitnot, logical_not };
    enum class binary_op : uint8_t {
        add, sub, mul, div, mod,
        bitand_, bitor_, bitxor_, shl, shr, sar,
        eq, ne, lt, le, gt, ge,
        land, lor
    };

    struct expr_ident { ident name; };
    struct expr_lit { lit value; };

    struct expr_call {
        node_id callee = 0;              // slot: call_callee, ordinal 0
        std::vector<node_id> args;       // slot: call_args, ordinals 0..N-1
    };

    struct expr_unary {
        unary_op op{};
        node_id arg = 0;                 // slot: unary_arg, ordinal 0
    };

    struct expr_binary {
        binary_op op{};
        node_id lhs = 0;                 // slot: binary_lhs, ordinal 0
        node_id rhs = 0;                 // slot: binary_rhs, ordinal 0
    };

    struct expr_ternary {
        node_id cond = 0;                // slot: ternary_cond
        node_id then_e = 0;              // slot: ternary_then
        node_id else_e = 0;              // slot: ternary_else
    };

    struct expr_field {
        node_id base = 0;                // slot: field_base
        ident field;                     // field name (no child node)
    };

    struct expr_index {
        node_id base = 0;                // slot: index_base
        node_id index = 0;               // slot: index_expr
    };

    struct expr_cast {
        node_id value = 0;               // slot: unary_arg (reuse) OR define cast_value slot
        type_ref to;
    };

    //------------------------------------------------------------------------------
    // Statement nodes
    //------------------------------------------------------------------------------
    struct stmt_block {
        std::vector<node_id> stmts;      // slot: block_stmts
    };

    struct bind_pattern {
        // minimal v1: ident only. Extend later for destructuring.
        ident name;
        type_ref type;                   // optional: allow inferred type by type.kind==named && sym==0
    };

    struct stmt_let {
        // Allow "let (a,b) = ..." later; currently list of binds size==1 most of the time
        std::vector<bind_pattern> binds; // slot: let_bindings (bind nodes can be embedded or separate)
        node_id init = 0;                // slot: let_init (0 if none)
    };

    struct stmt_assign {
        node_id lhs = 0;                 // slot: assign_lhs
        node_id rhs = 0;                 // slot: assign_rhs
    };

    struct stmt_if {
        node_id cond = 0;                // slot: if_cond
        node_id then_b = 0;              // slot: if_then (block node)
        node_id else_b = 0;              // slot: if_else (block node, 0 if absent)
    };

    struct stmt_while {
        node_id cond = 0;                // slot: while_cond
        node_id body = 0;                // slot: while_body (block)
    };

    struct stmt_for {
        node_id init = 0;                // slot: for_init (let/assign/expr as stmt node)
        node_id cond = 0;                // slot: for_cond (expr)
        node_id step = 0;                // slot: for_step (expr)
        node_id body = 0;                // slot: for_body (block)
    };

    struct stmt_return {
        node_id expr = 0;                // slot: return_expr (0 if none)
    };

    struct stmt_throw {
        node_id expr = 0;                // slot: throw_expr
    };

    struct stmt_trap {
        // trap [code]
        node_id code_expr = 0;           // reuse throw_expr slot if you want; keep explicit in lowering
    };

    struct stmt_halt {};

    struct stmt_label {
        ident name;
    };

    struct stmt_goto {
        // goto (cond) -> L_true, L_false;   OR  goto 1 -> L_end, L_end;
        node_id cond = 0;                  // slot: goto_cond (expr)
        std::vector<ident> targets;         // slot: goto_targets (labels in order)
    };

    struct catch_clause {
        ident binder;         // catch(e)
        node_id body = 0;     // block
    };

    struct stmt_try {
        node_id body = 0;                 // slot: try_body
        std::vector<catch_clause> catches;// slot: catch_list
        node_id finally_b = 0;            // slot: finally_body (0 if none)
    };

    //------------------------------------------------------------------------------
    // Sugar nodes (must be explicit, not inferred)
    //------------------------------------------------------------------------------
    struct stmt_with {
        node_id acquire = 0;              // slot: with_acquire (expr) (e.g. open(path))
        // binder can be absent: with open(path) { ... }
        std::optional<ident> binder;      // slot: with_binding (conceptually ordinal 0 if present)
        node_id body = 0;                 // slot: with_body (block)
    };

    struct stmt_defer {
        node_id cleanup = 0;              // slot: defer_body (block; parser wraps single stmt into block)
    };

    struct stmt_lock {
        node_id target = 0;               // slot: lock_target (expr)
        node_id body = 0;                 // slot: lock_body (block)
    };

    struct expr_spawn {
        node_id callee = 0;               // slot: spawn_callee (expr or call expr)
        std::vector<node_id> args;        // slot: spawn_args (optional; often empty if callee is call)
    };

    struct expr_join {
        node_id target = 0;               // slot: join_target
    };

    // Pattern nodes for match
    enum class pat_kind : uint8_t { int_lit, ident_bind, wildcard, variant_ctor, default_pat };

    struct match_pattern {
        pat_kind kind = pat_kind::wildcard;
        // For int_lit: use lit
        lit litv{};
        // For ident_bind: binder name
        ident bind{};
        // For variant_ctor: ctor name + optional binder
        ident ctor{};
        std::optional<ident> ctor_bind{};
    };

    struct stmt_match_arm {
        match_pattern pat;                // slot: match_arm_pat (conceptual; not node_id)
        node_id guard = 0;                // slot: match_arm_guard (0 if absent)
        node_id body = 0;                 // slot: match_arm_body (block)
    };

    struct stmt_match {
        node_id scrutinee = 0;            // slot: match_scrutinee (expr)
        std::vector<node_id> arms;        // slot: match_arms (each is node_kind::match_arm)
    };

    //------------------------------------------------------------------------------
    // Node/Prose mini-AST (explicit and executable)
    //------------------------------------------------------------------------------
    struct node_stmt_set {
        // set <name>: <Type> to <ExprOrStructLit>
        ident name;
        type_ref type;                    // optional (may be inferred, but present in AST)
        node_id value = 0;                // expression node
    };

    struct node_stmt_add {
        // add <lvalue> by <expr>
        node_id lvalue = 0;               // expression node (must be field/index/ident)
        node_id delta = 0;               // expression node
    };

    struct node_stmt_say {
        node_id value = 0;                // expression node (string usually)
    };

    struct node_stmt_go {
        ident target_node;                // target node label
    };

    struct node_stmt_halt {};

    struct node_stmt_trap {
        node_id code_expr = 0;            // optional numeric expr; 0 => default trap
    };

    struct node_block {
        ident name;                       // node header label
        std::vector<node_id> stmts;       // slot: node_stmt_list (node_stmt_* nodes)
    };

    struct node_start_at {
        ident entry;                      // name of entry node
    };

    // Node module: contains node blocks and optional start decl
    struct node_module {
        std::vector<node_id> nodes;       // slot: node_list (node_kind::node_block)
        std::optional<node_id> start;     // slot: node_start_decl (node_kind::node_start_at)
    };

    //------------------------------------------------------------------------------
    // Declarations
    //------------------------------------------------------------------------------
    struct import_decl { ident module; };

    struct proc_param { ident name; type_ref type; };

    struct proc_decl {
        ident name;
        std::vector<proc_param> params;   // slot: proc_params
        type_ref ret;
        std::vector<uint16_t> requires_;   // capability enum values; slot: proc_requires
        node_id body = 0;                 // slot: proc_body (block)
    };

    struct struct_field { ident name; type_ref type; };

    struct struct_decl {
        ident name;
        std::vector<struct_field> fields; // slot: type_items
    };

    struct enum_member { ident name; lit value; };

    struct enum_decl {
        ident name;
        type_ref underlying;
        std::vector<enum_member> members; // slot: type_items
    };

    struct variant_decl {
        ident name;
        // v1: store textual variants; expand later
        // slot: type_items
    };

    struct union_decl {
        ident name;
        std::vector<struct_field> fields; // slot: type_items
    };

    struct namespace_decl {
        ident name;
        std::vector<node_id> items;       // slot: ns_items
    };

    struct module_decl { ident name; };

    struct file_unit {
        std::vector<node_id> items;       // slot: file_items
    };

    //------------------------------------------------------------------------------
    // Node storage: tagged union
    //------------------------------------------------------------------------------
    struct node {
        node_kind kind{};
        node_link link{};

        union {
            file_unit as_file;
            module_decl as_module;
            namespace_decl as_ns;

            import_decl as_import;
            proc_decl as_proc;
            struct_decl as_struct;
            enum_decl as_enum;
            variant_decl as_variant;
            union_decl as_union;

            stmt_block as_block;
            stmt_let as_let;
            stmt_assign as_assign;
            stmt_if as_if;
            stmt_while as_while;
            stmt_for as_for;
            stmt_return as_return;
            stmt_try as_try;
            stmt_throw as_throw;
            stmt_trap as_trap;
            stmt_halt as_halt;
            stmt_goto as_goto;
            stmt_label as_label;

            expr_ident as_ident;
            expr_lit as_lit;
            expr_call as_call;
            expr_unary as_unary;
            expr_binary as_binary;
            expr_ternary as_ternary;
            expr_field as_field;
            expr_index as_index;
            expr_cast as_cast;

            stmt_with as_with;
            stmt_defer as_defer;
            stmt_lock as_lock;
            expr_spawn as_spawn;
            expr_join as_join;

            stmt_match as_match;
            stmt_match_arm as_match_arm;

            node_module as_node_mod;
            node_block  as_node_block;
            node_start_at as_node_start;

            node_stmt_set as_node_set;
            node_stmt_add as_node_add;
            node_stmt_say as_node_say;
            node_stmt_go  as_node_go;
            node_stmt_halt as_node_halt;
            node_stmt_trap as_node_trap;
        };

        // IMPORTANT:
        // C++ unions with non-trivial members require manual lifetime management.
        // In production, you’ll likely use std::variant or a custom tagged arena.
        // This header is shape-correct; storage strategy is implementation-defined.
    };

    //------------------------------------------------------------------------------
    // Lexical path compliance helpers (contract-critical)
    //------------------------------------------------------------------------------
    //
    // For every node, the parser MUST set:
    //   node.link.parent
    //   node.link.slot
    //   node.link.ordinal   (computed by token-pos ranking within parent+slot)
    //   node.link.pos.byte_offset (first token of node)
    //
    // Children MUST appear in the correct slots exactly as described above.
    //
    // Example: for a call:
    //   call node is parent of callee (slot call_callee, ord 0)
    //   call node is parent of args[i] (slot call_args, ord i, by lexical order)
    //
    //------------------------------------------------------------------------------

    //==============================================================================
    // NODE-PROSE LOWERING → NORMAL CFG (exact lowering shapes)
    //==============================================================================
    //
    // Node-prose is executable TODAY by lowering into a normal procedure + CFG.
    //
    // Input: node_module with K node_block(s) and one node_start_at.
    // Output: synthesized proc "__node_dispatch" + call from main (or injected entry).
    //
    // Lowering contract (exact):
    //
    // 1) Create proc __node_dispatch(entry: u32) -> void (noreturn if you enforce).
    // 2) Build a dispatcher loop:
    //
    //    let cur: u32 = entry;
    //    loop:
    //      switch cur:
    //        case hash("start"): goto BB_start
    //        case hash("end_node"): goto BB_end_node
    //        default: trap
    //
    // 3) For each node_block "NAME":
    //    Create basic block BB_NAME:
    //      For each node_stmt in node_block.stmts in lexical order:
    //        - node_stmt_set: becomes let/assign in canonical AST:
    //            if var not declared in scope: emit let_stmt
    //            else: emit assign_stmt
    //        - node_stmt_add: becomes assign (lhs = lhs + delta)
    //        - node_stmt_say: becomes call print(value)
    //        - node_stmt_go: sets cur = hash(target); goto loop_dispatch
    //        - node_stmt_halt: becomes halt_stmt (terminator)
    //        - node_stmt_trap: becomes trap_stmt (terminator)
    //      If block falls through with no go/halt/trap:
    //        trap (because node blocks must terminate deterministically)
    //
    // 4) Inject start:
    //    If program has "start at node X":
    //      main (or module init) calls __node_dispatch(hash(X)).
    //
    // Hashing for node names:
    //   Use a stable compile-time hash (e.g., FNV-1a 32) of the node label text,
    //   or assign stable ids from symbol table. Must be stable across builds.
    //
    // CIAM involvement:
    //   - CIAM treats node_module as sugar and performs this lowering in PASS 1/2.
    //   - The resulting canonical AST is then typechecked/IR-lowered normally.
    //
    //==============================================================================

} // namespace ranepragma once
