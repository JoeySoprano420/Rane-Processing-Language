// actionplan.hpp (C++20)

#pragma once
#include <cstdint>
#include <vector>
#include <string_view>
#include <variant>
#include <optional>

namespace rane {

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using i64 = int64_t;

    struct Span {
        u32 line = 0, col = 0, len = 0;
    };

    struct SymbolId { u32 v = 0; };   // interned: proc/global/field names
    struct TypeId { u32 v = 0; };   // interned type ref
    struct BlockId { u32 v = 0; };   // block index
    struct ValueId { u32 v = 0; };   // semantic value node id (not SSA)
    struct CapId { u32 v = 0; };   // capability index (global)

    enum class ValueKind : u8 {
        Invalid,
        ConstInt, ConstBool, ConstNull,
        VarRef,        // resolved local binding
        GlobalRef,     // resolved global
        FieldRef,      // base.field
        IndexRef,      // base[idx]
        Call, Compare, Binary, Unary,
        Cast,
    };

    enum class CmpOp : u8 { EQ, NE, LT, LE, GT, GE };
    enum class BinOp : u8 { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Sar };
    enum class UnOp : u8 { Neg, Not, BitNot };

    struct ConstInt { i64 value; };
    struct ConstBool { bool value; };
    struct ConstNull {};

    struct VarRef { SymbolId local; };          // resolved local symbol
    struct GlobalRef { SymbolId global; };         // resolved global symbol
    struct FieldRef { ValueId base; SymbolId field; };
    struct IndexRef { ValueId base; ValueId index; };

    struct Call {
        SymbolId callee;               // resolved symbol (or import thunk symbol)
        std::vector<ValueId> args;     // semantic args
    };

    struct Compare { CmpOp op; ValueId a; ValueId b; };
    struct Binary { BinOp op; ValueId a; ValueId b; };
    struct Unary { UnOp  op; ValueId a; };
    struct Cast { ValueId a; TypeId to; };

    struct ValueNode {
        ValueKind kind = ValueKind::Invalid;
        TypeId    type{};
        Span      span{};
        u64       req_caps_mask_hash = 0; // quick-check hash of required caps set for this value/action
        std::variant<
            std::monostate,
            ConstInt, ConstBool, ConstNull,
            VarRef, GlobalRef, FieldRef, IndexRef,
            Call, Compare, Binary, Unary, Cast
        > as;
    };
    enum class ActionKind : u8 {
        Nop,
        Eval,         // evaluate value for side effects (calls etc.)
        Assign,       // target = value
        Jump,         // unconditional jump
        CondJump,     // if (cond == 0) goto if_false else if_true
        Trap, Halt,
    };

    struct EvalAction {
        ValueId expr;
    };

    struct AssignAction {
        // target is an lvalue ValueId: VarRef / FieldRef / IndexRef (already validated by resolver)
        ValueId target;
        ValueId value;
    };

    struct JumpAction {
        BlockId target;
    };

    struct CondJumpAction {
        ValueId cond;        // bool-like (resolver enforces)
        BlockId if_false;
        BlockId if_true;
    };

    struct TrapAction { std::optional<ValueId> payload; };
    struct HaltAction {};

    struct Action {
        ActionKind kind = ActionKind::Nop;
        Span span{};
        u64  req_caps_mask_hash = 0; // required caps for this action (enforced vs proc caps)
        std::variant<
            std::monostate,
            EvalAction, AssignAction,
            JumpAction, CondJumpAction,
            TrapAction, HaltAction
        > as;
    };
    struct Block {
        BlockId id{};
        std::string_view label;       // stable name for debugging (interned string view)
        std::vector<Action> actions;  // terminator must be Jump / CondJump / Trap / Halt
    };

    struct CapSet {
        // a compact bitset stored as u64 words (deterministic order)
        std::vector<u64> words;
    };

    struct ProcPlan {
        SymbolId proc_symbol{};
        TypeId   ret_type{};
        std::vector<SymbolId> params;     // resolved param symbols
        std::vector<TypeId>   param_types;

        CapSet declared_caps;             // from requires(...)
        std::vector<Block> blocks;        // CFG blocks
        BlockId entry{};                  // typically 0

        // deterministic locals layout is decided later (emit), but the resolver can predeclare locals list
        std::vector<SymbolId> locals;      // resolved locals in deterministic order
        std::vector<TypeId>   local_types;
    };

    struct ActionPlan {
        std::vector<ValueNode> values;   // arena of semantic values
        std::vector<ProcPlan>  procs;
        std::vector<std::string_view> cap_names; // cap index -> name (interned)
    };

} // namespace rane
