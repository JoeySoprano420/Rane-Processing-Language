// ============================================================================
// File: rane_emitter.hpp  (C++20, header-only interface + small inline helpers)
// ============================================================================
//
// Ready-to-compile emitter skeleton that matches the ActionPlan templates:
// - emit_value(ValueId) -> leaves integer/bool result in RAX (bool canonicalized 0/1)
// - emits blocks with Jump / CondJump (JmpIfZero = test rax,rax ; jz)
// - deterministic scratch regs: R11 then R10 then R9
// - deterministic temps on stack for nesting / call hazards
// - Windows x64 ABI: 32-byte shadow space always reserved in frame
//
// You provide:
// - actionplan.hpp (the structs we defined earlier)
// - a minimal type-layout provider (sizes/field offsets) and symbol resolver.
//
// Build style:
//   g++ -std=c++20 -O2 -Wall -Wextra your.cpp
// or MSVC / clang-cl.

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <variant>
#include <cassert>
#include <cstring>

#include "actionplan.hpp" // from your prior definitions (rane::ActionPlan, ProcPlan, ValueId etc.)

namespace rane::x64 {

    // ---------------------------
    // Registers (Windows x64)
    // ---------------------------
    enum class Reg : uint8_t {
        RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
        R8, R9, R10, R11, R12, R13, R14, R15
    };

    static constexpr Reg kResultReg = Reg::RAX;
    static constexpr Reg kScratch0 = Reg::R11;
    static constexpr Reg kScratch1 = Reg::R10;
    static constexpr Reg kScratch2 = Reg::R9;

    // ---------------------------
    // Relocation / patch types
    // ---------------------------
    enum class PatchKind : uint8_t {
        Rel32_Jmp,      // E9 rel32
        Rel32_Jcc,      // 0F 8? rel32
        Rel32_Call,     // E8 rel32
        RipRel32_Addr,  // e.g. mov rax, [rip+rel32]  (if you add it)
        Abs64_Imm,      // e.g. mov rax, imm64
    };

    struct Patch {
        PatchKind kind{};
        uint32_t  at = 0;           // offset within code buffer where the rel32/imm begins
        BlockId   target_block{};   // for block patches
        SymbolId  target_symbol{};  // for symbol patches
    };

    // ---------------------------
    // Label positions (blocks)
    // ---------------------------
    struct Label {
        bool     bound = false;
        uint32_t pos = 0;
    };

    // ---------------------------
    // Type layout hooks (you own these)
    // ---------------------------
    struct TypeLayout {
        uint32_t size = 0;     // in bytes
        uint32_t align = 1;    // in bytes
    };

    struct ILayoutProvider {
        virtual ~ILayoutProvider() = default;
        virtual TypeLayout type_layout(TypeId t) const = 0;
        virtual uint32_t   field_offset(TypeId struct_type, SymbolId field) const = 0;
        virtual uint32_t   element_size(TypeId element_type) const = 0;
    };

    // ---------------------------
    // Symbol resolution hooks (you own these)
    // ---------------------------
    enum class SymKind : uint8_t { Proc, Global, ImportThunk };

    struct SymbolInfo {
        SymKind kind{};
        std::string_view name{};
        // For imports/globals, loader will patch relocations; emitter only records symbol ids.
    };

    struct ISymbolResolver {
        virtual ~ISymbolResolver() = default;
        virtual SymbolInfo symbol_info(SymbolId s) const = 0;
    };

    // ---------------------------
    // Frame layout (locals + temps + shadow + align)
    // ---------------------------
    struct FrameLayout {
        // rbp-based frame: [rbp - offset] for locals/temps
        uint32_t shadow_bytes = 32;      // Windows x64 ABI
        uint32_t locals_bytes = 0;
        uint32_t temps_bytes = 0;
        uint32_t saved_nv_bytes = 0;     // if you decide to push r12.. etc
        uint32_t total_bytes = 0;       // rounded up to 16_drop alignment
        uint32_t align = 16;

        // deterministic local offsets:
        // offset_from_rbp = base + idx*slot (var sized)
        std::unordered_map<SymbolId, uint32_t> local_off; // rbp - off
        // temps are [rbp - temp_off_base - i*8]
        uint32_t temp_base_off = 0;

        uint32_t local_offset(SymbolId sym) const {
            auto it = local_off.find(sym);
            assert(it != local_off.end());
            return it->second;
        }
        uint32_t temp_offset(uint32_t i) const {
            return temp_base_off + i * 8;
        }
    };

    // ---------------------------
    // Machine code buffer
    // ---------------------------
    struct CodeBuf {
        std::vector<uint8_t> b;

        uint32_t size() const { return (uint32_t)b.size(); }
        uint8_t* data() { return b.data(); }
        const uint8_t* data() const { return b.data(); }

        void u8(uint8_t v) { b.push_back(v); }
        void u32(uint32_t v) {
            b.push_back((uint8_t)(v));
            b.push_back((uint8_t)(v >> 8));
            b.push_back((uint8_t)(v >> 16));
            b.push_back((uint8_t)(v >> 24));
        }
        void u64(uint64_t v) {
            for (int i = 0; i < 8; ++i) b.push_back((uint8_t)(v >> (i * 8)));
        }
        void bytes(std::initializer_list<uint8_t> xs) { b.insert(b.end(), xs.begin(), xs.end()); }

        void patch_u32(uint32_t at, uint32_t v) {
            assert(at + 4 <= b.size());
            b[at + 0] = (uint8_t)(v);
            b[at + 1] = (uint8_t)(v >> 8);
            b[at + 2] = (uint8_t)(v >> 16);
            b[at + 3] = (uint8_t)(v >> 24);
        }
        void patch_i32(uint32_t at, int32_t v) { patch_u32(at, (uint32_t)v); }
        void patch_u64(uint32_t at, uint64_t v) {
            assert(at + 8 <= b.size());
            for (int i = 0; i < 8; ++i) b[at + i] = (uint8_t)(v >> (i * 8));
        }
    };

    // ---------------------------
    // Minimal x64 encoder helpers
    // (subset enough to match the templates above)
    // ---------------------------
    namespace enc {

        static inline uint8_t rex(bool w, bool r, bool x, bool b) {
            return (uint8_t)(0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0));
        }

        static inline bool is_ext(Reg r) { return (uint8_t)r >= (uint8_t)Reg::R8; }
        static inline uint8_t reg3(Reg r) { return (uint8_t)r & 7; }

        // mov r64, imm64  => REX.W + B8+rd imm64
        static inline void mov_ri64(CodeBuf& c, Reg dst, uint64_t imm) {
            bool b = is_ext(dst);
            c.u8(rex(true, false, false, b));
            c.u8((uint8_t)(0xB8 + reg3(dst)));
            c.u64(imm);
        }

        // mov r/m64, r64  (store) 89 /r
        // using [rbp - disp32] with modrm: mod=10 rm=101 (rbp) reg=src
        static inline void mov_mrbp_r64(CodeBuf& c, int32_t disp, Reg src) {
            // REX.W
            bool r = is_ext(src);
            c.u8(rex(true, r, false, false));
            c.u8(0x89);
            // ModRM: mod=10, reg=src, rm=RBP(101)
            c.u8((uint8_t)(0b10'000'101 | (reg3(src) << 3)));
            c.u32((uint32_t)disp);
        }

        // mov r64, r/m64  (load) 8B /r
        static inline void mov_r64_mrbp(CodeBuf& c, Reg dst, int32_t disp) {
            bool r = is_ext(dst);
            c.u8(rex(true, r, false, false));
            c.u8(0x8B);
            c.u8((uint8_t)(0b10'000'101 | (reg3(dst) << 3)));
            c.u32((uint32_t)disp);
        }

        // mov r64, r64  (register move) 8B /r with mod=11
        static inline void mov_rr(CodeBuf& c, Reg dst, Reg src) {
            bool r = is_ext(dst);
            bool b = is_ext(src);
            c.u8(rex(true, r, false, b));
            c.u8(0x8B);
            c.u8((uint8_t)(0b11'000'000 | (reg3(dst) << 3) | reg3(src)));
        }

        // add r64, r64  01 /r (add r/m64, r64) with dst in r/m
        static inline void add_rr(CodeBuf& c, Reg dst, Reg src) {
            bool r = is_ext(src);
            bool b = is_ext(dst);
            c.u8(rex(true, r, false, b));
            c.u8(0x01);
            c.u8((uint8_t)(0b11'000'000 | (reg3(src) << 3) | reg3(dst)));
        }

        // sub r64, r64  29 /r
        static inline void sub_rr(CodeBuf& c, Reg dst, Reg src) {
            bool r = is_ext(src);
            bool b = is_ext(dst);
            c.u8(rex(true, r, false, b));
            c.u8(0x29);
            c.u8((uint8_t)(0b11'000'000 | (reg3(src) << 3) | reg3(dst)));
        }

        // imul r64, r/m64  0F AF /r  (dst *= src)
        static inline void imul_rr(CodeBuf& c, Reg dst, Reg src) {
            bool r = is_ext(dst);
            bool b = is_ext(src);
            c.u8(rex(true, r, false, b));
            c.bytes({ 0x0F, 0xAF });
            c.u8((uint8_t)(0b11'000'000 | (reg3(dst) << 3) | reg3(src)));
        }

        // cmp r64, r64  39 /r  (cmp r/m64, r64) with left in r/m
        static inline void cmp_rr(CodeBuf& c, Reg left, Reg right) {
            bool r = is_ext(right);
            bool b = is_ext(left);
            c.u8(rex(true, r, false, b));
            c.u8(0x39);
            c.u8((uint8_t)(0b11'000'000 | (reg3(right) << 3) | reg3(left)));
        }

        // test r64, r64  85 /r
        static inline void test_rr(CodeBuf& c, Reg a, Reg b_) {
            bool r = is_ext(b_);
            bool b = is_ext(a);
            c.u8(rex(true, r, false, b));
            c.u8(0x85);
            c.u8((uint8_t)(0b11'000'000 | (reg3(b_) << 3) | reg3(a)));
        }

        // setcc r/m8  0F 9? /r ; we'll target AL only using ModRM=11, rm=0 (AL)
        enum class SetCC : uint8_t { E = 0x94, NE = 0x95, L = 0x9C, LE = 0x9E, G = 0x9F, GE = 0x9D };
        static inline void setcc_al(CodeBuf& c, SetCC cc) {
            c.bytes({ 0x0F, (uint8_t)cc });
            c.u8(0b11'000'000); // ModRM: mod=11 reg=000 rm=000 => AL
        }

        // movzx r64, r/m8  => 48 0F B6 C0 (movzx rax, al)
        static inline void movzx_rax_al(CodeBuf& c) {
            c.bytes({ 0x48, 0x0F, 0xB6, 0xC0 });
        }

        // jmp rel32: E9 rel32
        static inline uint32_t jmp_rel32(CodeBuf& c) {
            c.u8(0xE9);
            uint32_t at = c.size();
            c.u32(0);
            return at;
        }

        // jcc rel32: 0F 8? rel32  (we use JZ and JNZ only here)
        enum class Jcc : uint8_t { JZ = 0x84, JNZ = 0x85 };
        static inline uint32_t jcc_rel32(CodeBuf& c, Jcc cc) {
            c.bytes({ 0x0F, (uint8_t)cc });
            uint32_t at = c.size();
            c.u32(0);
            return at;
        }

        // call rel32: E8 rel32
        static inline uint32_t call_rel32(CodeBuf& c) {
            c.u8(0xE8);
            uint32_t at = c.size();
            c.u32(0);
            return at;
        }

        // prologue/epilogue
        static inline void push_rbp(CodeBuf& c) { c.u8(0x55); }
        static inline void pop_rbp(CodeBuf& c) { c.u8(0x5D); }
        static inline void mov_rbp_rsp(CodeBuf& c) { c.bytes({ 0x48, 0x89, 0xE5 }); } // mov rbp, rsp
        // sub rsp, imm32: 48 81 EC imm32
        static inline void sub_rsp_imm32(CodeBuf& c, uint32_t imm) { c.bytes({ 0x48, 0x81, 0xEC }); c.u32(imm); }
        // mov rsp, rbp: 48 89 EC
        static inline void mov_rsp_rbp(CodeBuf& c) { c.bytes({ 0x48, 0x89, 0xEC }); }
        // ret
        static inline void ret(CodeBuf& c) { c.u8(0xC3); }

    } // namespace enc

    // ---------------------------
    // Deterministic frame size computation
    // ---------------------------

    static inline uint32_t align_up(uint32_t v, uint32_t a) {
        return (v + (a - 1)) & ~(a - 1);
    }

    // Conservative temp-depth analysis so emit_value can spill intermediate results deterministically.
    // You can tighten this later, but it is correct.
    struct TempAnalysis {
        const ActionPlan& ap;

        explicit TempAnalysis(const ActionPlan& ap_) : ap(ap_) {}

        // Max simultaneous stack temps needed for a value subtree.
        // Our emit_value uses one temp slot when it needs to preserve LHS while evaluating RHS, etc.
        uint32_t temp_depth(ValueId v) const {
            const auto& n = ap.values.at(v.v);
            switch (n.kind) {
            case ValueKind::ConstInt:
            case ValueKind::ConstBool:
            case ValueKind::ConstNull:
            case ValueKind::VarRef:
            case ValueKind::GlobalRef:
                return 0;

            case ValueKind::FieldRef: {
                auto fr = std::get<FieldRef>(n.as);
                return temp_depth(fr.base);
            }
            case ValueKind::IndexRef: {
                auto ir = std::get<IndexRef>(n.as);
                uint32_t a = temp_depth(ir.base);
                uint32_t b = temp_depth(ir.index);
                return (a > b ? a : b) + 1; // preserve base while computing index (conservative)
            }

            case ValueKind::Unary: {
                auto u = std::get<Unary>(n.as);
                return temp_depth(u.a);
            }
            case ValueKind::Cast: {
                auto c = std::get<Cast>(n.as);
                return temp_depth(c.a);
            }

            case ValueKind::Binary:
            case ValueKind::Compare: {
                ValueId a{}, b{};
                if (n.kind == ValueKind::Binary) {
                    auto x = std::get<Binary>(n.as);
                    a = x.a; b = x.b;
                }
                else {
                    auto x = std::get<Compare>(n.as);
                    a = x.a; b = x.b;
                }
                uint32_t da = temp_depth(a);
                uint32_t db = temp_depth(b);
                // we often: eval a -> temp, eval b, load a, op
                return (da > db ? da : db) + 1;
            }

            case ValueKind::Call: {
                auto c = std::get<Call>(n.as);
                uint32_t m = 0;
                for (auto arg : c.args) {
                    uint32_t d = temp_depth(arg);
                    if (d > m) m = d;
                }
                // We don't keep temps across calls; but args evaluation may require temps.
                return m;
            }

            default:
                return 0;
            }
        }

        uint32_t proc_max_temp_slots(const ProcPlan& p) const {
            uint32_t m = 0;
            for (const auto& b : p.blocks) {
                for (const auto& a : b.actions) {
                    switch (a.kind) {
                    case ActionKind::Eval: {
                        auto e = std::get<EvalAction>(a.as);
                        m = std::max(m, temp_depth(e.expr));
                    } break;
                    case ActionKind::Assign: {
                        auto asg = std::get<AssignAction>(a.as);
                        m = std::max(m, temp_depth(asg.value));
                        // target lvalue subexpressions may also require temps:
                        m = std::max(m, temp_depth(asg.target));
                    } break;
                    case ActionKind::CondJump: {
                        auto cj = std::get<CondJumpAction>(a.as);
                        m = std::max(m, temp_depth(cj.cond));
                    } break;
                    default: break;
                    }
                }
            }
            // temp_depth returns "max needed simultaneously"; that maps 1:1 to number of 8-byte slots
            return m;
        }
    };

    // Compute deterministic local offsets and FRAME_SIZE (locals + temps + shadow + align).
    // Locals are placed in resolver-determined order: ProcPlan.locals[0..].
    // Each local occupies align_up(size,8) and aligned to min(type.align,8) within the frame.
    // Temps are 8-byte slots (RAX spills), placed after locals.
    // Shadow space is reserved as part of the frame (recommended).
    static inline FrameLayout compute_frame_layout(
        const ProcPlan& proc,
        const ActionPlan& ap,
        const ILayoutProvider& layout
    ) {
        FrameLayout fr{};
        fr.shadow_bytes = 32;
        fr.align = 16;

        // 1) locals
        uint32_t off = 0; // counts bytes beneath rbp: [rbp - off]
        for (size_t i = 0; i < proc.locals.size(); ++i) {
            SymbolId sym = proc.locals[i];
            TypeId   ty = proc.local_types[i];
            auto tl = layout.type_layout(ty);

            uint32_t slot_align = std::min<uint32_t>(tl.align, 8);
            uint32_t slot_size = align_up(tl.size, 8);

            // ensure alignment by padding "off" up to slot_align
            off = align_up(off, slot_align);
            off += slot_size;

            fr.local_off.emplace(sym, off); // rbp - off
        }
        fr.locals_bytes = align_up(off, 8);

        // 2) temps
        TempAnalysis ta(ap);
        uint32_t temp_slots = ta.proc_max_temp_slots(proc);
        fr.temps_bytes = temp_slots * 8;

        // temps are placed after locals (further below rbp)
        fr.temp_base_off = fr.locals_bytes + 8; // first temp: [rbp - (locals_bytes + 8)]
        // note: we use temp_offset(i) = temp_base_off + i*8

        // 3) total frame
        // We'll allocate: locals + temps + shadow + any padding to maintain 16-byte stack alignment.
        uint32_t raw = fr.locals_bytes + fr.temps_bytes + fr.shadow_bytes;

        // After push rbp (8 bytes), RSP is misaligned by 8 vs 16. We want post-sub rsp to be 16-aligned.
        // Common rule: allocate raw such that (raw % 16) == 8 to compensate push rbp.
        // Equivalent: total_bytes = align_up(raw + 8, 16) - 8
        fr.total_bytes = (align_up(raw + 8, 16) - 8);

        return fr;
    }

    // ---------------------------
    // Emitter
    // ---------------------------
    struct EmitResult {
        std::vector<uint8_t> code;
        std::vector<Patch>   patches;     // symbol + block patches
        FrameLayout          frame;
    };

    struct Emitter {
        const ActionPlan& ap;
        const ProcPlan& proc;
        const ILayoutProvider& layout;
        const ISymbolResolver& syms;

        FrameLayout frame{};
        CodeBuf     code{};
        std::vector<Label> block_labels;
        std::vector<Patch> patches;

        // deterministic temp stack index used by recursive emit_value
        uint32_t temp_sp = 0;
        uint32_t temp_max = 0;

        Emitter(const ActionPlan& ap_,
            const ProcPlan& proc_,
            const ILayoutProvider& layout_,
            const ISymbolResolver& syms_)
            : ap(ap_), proc(proc_), layout(layout_), syms(syms_) {
        }

        // ----- temp management -----
        uint32_t temp_alloc() {
            uint32_t idx = temp_sp++;
            if (temp_sp > temp_max) temp_max = temp_sp;
            return idx;
        }
        void temp_free() { assert(temp_sp > 0); temp_sp--; }

        int32_t rbp_disp_from_off(uint32_t off) const {
            // want [rbp - off] => disp32 = -off
            return -(int32_t)off;
        }

        // ----- emit helpers -----
        void bind_block(BlockId b) {
            auto& L = block_labels.at(b.v);
            L.bound = true;
            L.pos = code.size();
        }

        void emit_jmp_block(BlockId target) {
            uint32_t at = enc::jmp_rel32(code);
            patches.push_back(Patch{ PatchKind::Rel32_Jmp, at, target, {} });
        }

        void emit_jz_block(BlockId target) {
            uint32_t at = enc::jcc_rel32(code, enc::Jcc::JZ);
            patches.push_back(Patch{ PatchKind::Rel32_Jcc, at, target, {} });
        }

        void emit_call_symbol(SymbolId callee) {
            // For now we emit CALL rel32 to a symbol that the linker/loader resolves.
            // Strategy: treat call target as a symbol label in the same module OR an import thunk label.
            uint32_t at = enc::call_rel32(code);
            patches.push_back(Patch{ PatchKind::Rel32_Call, at, {}, callee });
        }

        // ----- core: emit_value(ValueId) -----
        // Leaves result in RAX, bool normalized to 0/1 for compares.
        void emit_value(ValueId v) {
            const auto& n = ap.values.at(v.v);

            switch (n.kind) {
            case ValueKind::ConstInt: {
                auto ci = std::get<ConstInt>(n.as);
                enc::mov_ri64(code, Reg::RAX, (uint64_t)ci.value);
            } break;

            case ValueKind::ConstBool: {
                auto cb = std::get<ConstBool>(n.as);
                enc::mov_ri64(code, Reg::RAX, cb.value ? 1u : 0u);
            } break;

            case ValueKind::ConstNull: {
                enc::mov_ri64(code, Reg::RAX, 0);
            } break;

            case ValueKind::VarRef: {
                auto vr = std::get<VarRef>(n.as);
                uint32_t off = frame.local_offset(vr.local);
                enc::mov_r64_mrbp(code, Reg::RAX, rbp_disp_from_off(off));
            } break;

            case ValueKind::FieldRef: {
                auto fr = std::get<FieldRef>(n.as);
                emit_value(fr.base);
                enc::mov_rr(code, Reg::R11, Reg::RAX);
                uint32_t k = layout.field_offset(ap.values.at(fr.base.v).type, fr.field);
                code.bytes({ 0x49, 0x8B, 0x83 });
                code.u32(k);
            } break;

            case ValueKind::IndexRef: {
                auto ir = std::get<IndexRef>(n.as);
                emit_value(ir.base);
                uint32_t t = temp_alloc();
                enc::mov_mrbp_r64(code, rbp_disp_from_off(frame.temp_offset(t)), Reg::RAX);

                emit_value(ir.index);

                enc::mov_r64_mrbp(code, Reg::R11, rbp_disp_from_off(frame.temp_offset(t)));
                temp_free();

                uint32_t S = layout.element_size(ap.values.at(ir.base.v).type);

                if ((S & (S - 1)) == 0) {
                    uint8_t sh = 0; while ((1u << sh) != S) sh++;
                    code.bytes({ 0x48, 0xC1, 0xE0, sh });
                } else {
                    code.bytes({ 0x48, 0x69, 0xC0 });
                    code.u32(S);
                }

                enc::add_rr(code, Reg::R11, Reg::RAX);
                code.bytes({ 0x49, 0x8B, 0x03 });
            } break;

            case ValueKind::Unary: {
                auto u = std::get<Unary>(n.as);
                emit_value(u.a);
                switch (u.op) {
                case UnOp::Neg:
                    // neg rax => 48 F7 D8
                    code.bytes({ 0x48, 0xF7, 0xD8 });
                    break;
                case UnOp::Not:
                    // logical not: rax = (rax==0) ? 1 : 0
                    enc::test_rr(code, Reg::RAX, Reg::RAX);
                    // setz al ; movzx rax, al
                    enc::setcc_al(code, enc::SetCC::E);
                    enc::movzx_rax_al(code);
                    break;
                case UnOp::BitNot:
                    // not rax => 48 F7 D0
                    code.bytes({ 0x48, 0xF7, 0xD0 });
                    break;
                }
            } break;

            case ValueKind::Cast: {
                auto cst = std::get<Cast>(n.as);
                // bootstrap: assume integer casts are no-ops or trunc/extend handled by resolver constraints
                emit_value(cst.a);
            } break;

            case ValueKind::Binary: {
                auto b = std::get<Binary>(n.as);

                // Evaluate left -> rax, spill to temp, eval right -> rax, load left -> r11, op -> rax.
                emit_value(b.a);
                uint32_t t = temp_alloc();
                enc::mov_mrbp_r64(code, rbp_disp_from_off(frame.temp_offset(t)), Reg::RAX);

                emit_value(b.b);
                // r11 = left
                enc::mov_r64_mrbp(code, Reg::R11, rbp_disp_from_off(frame.temp_offset(t)));
                temp_free();

                switch (b.op) {
                case BinOp::Add:
                    // rax = r11 + rax  => move rax into scratch? easiest: add r11, rax then move back
                    enc::add_rr(code, Reg::R11, Reg::RAX);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Sub:
                    // rax = r11 - rax => sub r11, rax ; mov rax,r11
                    enc::sub_rr(code, Reg::R11, Reg::RAX);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Mul:
                    // rax = r11 * rax => mov rax,r11 ; imul rax, r?? (need rax*=rhs)
                    // We'll: mov r10, rax (rhs), mov rax,r11, imul rax, r10
                    enc::mov_rr(code, Reg::R10, Reg::RAX);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    enc::imul_rr(code, Reg::RAX, Reg::R10);
                    break;
                case BinOp::And:
                    // bitwise and: and r11, rax => 49 21 C3? We'll implement via bytes: REX.W; 21 /r
                    // Use: and r11, rax (r/m64=r11, reg=rax) => 4C 21 C3  (REX.W R=1 B=1)
                    code.bytes({ 0x4C, 0x21, 0xC3 });
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Or:
                    // or r11, rax => 4C 09 C3
                    code.bytes({ 0x4C, 0x09, 0xC3 });
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Xor:
                    // xor r11, rax => 4C 31 C3
                    code.bytes({ 0x4C, 0x31, 0xC3 });
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                default:
                    // Div/Mod/Shifts: you can add exact encodings later.
                    // Keep compile-ready:
                    assert(false && "Binary op not yet implemented in bootstrap emitter");
                }
            } break;

            case ValueKind::Compare: {
                auto c = std::get<Compare>(n.as);

                // eval a -> temp, eval b -> rax, load a -> r11, cmp r11, rax, setcc al, movzx rax, al
                emit_value(c.a);
                uint32_t t = temp_alloc();
                enc::mov_mrbp_r64(code, rbp_disp_from_off(frame.temp_offset(t)), Reg::RAX);

                emit_value(c.b);
                enc::mov_r64_mrbp(code, Reg::R11, rbp_disp_from_off(frame.temp_offset(t)));
                temp_free();

                enc::cmp_rr(code, Reg::R11, Reg::RAX);

                enc::SetCC scc = enc::SetCC::E;
                switch (c.op) {
                case CmpOp::EQ: scc = enc::SetCC::E; break;
                case CmpOp::NE: scc = enc::SetCC::NE; break;
                case CmpOp::LT: scc = enc::SetCC::L; break;
                case CmpOp::LE: scc = enc::SetCC::LE; break;
                case CmpOp::GT: scc = enc::SetCC::G; break;
                case CmpOp::GE: scc = enc::SetCC::GE; break;
                }
                enc::setcc_al(code, scc);
                enc::movzx_rax_al(code);
            } break;

            case ValueKind::Call: {
                auto call = std::get<Call>(n.as);

                // flush cache hook (if you implement caching later)

                // Evaluate args left-to-right into RAX then move into ABI regs.
                // Note: this implementation supports up to 4 args in regs; extras TODO.
                static constexpr Reg abi_regs[4] = { Reg::RCX, Reg::RDX, Reg::R8, Reg::R9 };

                size_t narg = call.args.size();
                size_t reg_args = (narg > 4 ? 4 : narg);

                for (size_t i = 0; i < reg_args; ++i) {
                    emit_value(call.args[i]);
                    enc::mov_rr(code, abi_regs[i], Reg::RAX);
                }

                if (narg > 4) {
                    // TODO: stack args (right-to-left) while keeping 16-byte alignment.
                    assert(false && "Stack args not implemented yet");
                }

                emit_call_symbol(call.callee);
                // return is already in RAX
            } break;

            default:
                assert(false && "emit_value: unsupported ValueKind in bootstrap emitter");
            }
        }

        // ----- actions -----
        void emit_action(const Action& a) {
            switch (a.kind) {
            case ActionKind::Eval: {
                auto e = std::get<EvalAction>(a.as);
                emit_value(e.expr);
            } break;

            case ActionKind::Assign: {
                auto asg = std::get<AssignAction>(a.as);

                // Emit RHS -> RAX first (deterministic)
                emit_value(asg.value);

                // Store to lvalue:
                const auto& tgt = ap.values.at(asg.target.v);

                if (tgt.kind == ValueKind::VarRef) {
                    auto vr = std::get<VarRef>(tgt.as);
                    uint32_t off = frame.local_offset(vr.local);
                    enc::mov_mrbp_r64(code, off, Reg::RAX);
                    return;
                }

                // FieldRef / IndexRef targets need address compute; simplest:
                // - compute address into R11, then store [R11] = RAX
                // For bootstrap: handle IndexRef address; FieldRef assumes pointer base + offset.
                if (tgt.kind == ValueKind::FieldRef) {
                    auto fr = std::get<FieldRef>(tgt.as);
                    // compute base -> R11
                    emit_value(fr.base);
                    enc::mov_rr(code, Reg::R11, Reg::RAX);
                    // store to [r11 + k]
                    uint32_t k = layout.field_offset(ap.values.at(fr.base.v).type, fr.field);
                    // MOV [R11+disp32], RAX => 49 89 83 disp32
                    code.bytes({ 0x49, 0x89, 0x83 });
                    code.u32(k);
                    return;
                }

                if (tgt.kind == ValueKind::IndexRef) {
                    auto ir = std::get<IndexRef>(tgt.as);

                    // preserve value in temp because we must compute address
                    uint32_t tv = temp_alloc();
                    enc::mov_mrbp_r64(code, rbp_disp_from_off(frame.temp_offset(tv)), Reg::RAX);

                    // base -> rax
                    emit_value(ir.base);
                    uint32_t tb = temp_alloc();
                    enc::mov_mrbp_r64(code, rbp_disp_from_off(frame.temp_offset(tb)), Reg::RAX);

                    // index -> rax
                    emit_value(ir.index);

                    enc::mov_r64_mrbp(code, Reg::R11, rbp_disp_from_off(frame.temp_offset(tb)));
                    temp_free(); // tb

                    uint32_t S = layout.element_size(ap.values.at(ir.base.v).type);
                    if ((S & (S - 1)) == 0) {
                        uint8_t sh = 0; while ((1u << sh) != S) sh++;
                        code.bytes({ 0x48, 0xC1, 0xE0, sh });
                    }
                    else {
                        code.bytes({ 0x48, 0x69, 0xC0 });
                        code.u32(S);
                    }
                    enc::add_rr(code, Reg::R11, Reg::RAX);

                    // load value back into rax
                    enc::mov_r64_mrbp(code, Reg::RAX, rbp_disp_from_off(frame.temp_offset(tv)));
                    temp_free(); // tv

                    // MOV [R11], RAX => 49 89 03
                    code.bytes({ 0x49, 0x89, 0x03 });
                    return;
                }

                assert(false && "Assign target kind not supported");
            } break;

            case ActionKind::Jump: {
                auto j = std::get<JumpAction>(a.as);
                emit_jmp_block(j.target);
            } break;

            case ActionKind::CondJump: {
                auto cj = std::get<CondJumpAction>(a.as);
                emit_value(cj.cond);                 // -> RAX
                enc::test_rr(code, Reg::RAX, Reg::RAX);
                emit_jz_block(cj.if_false);
                emit_jmp_block(cj.if_true);
            } break;

            case ActionKind::Trap: {
                // Minimal: int3 for trap
                code.u8(0xCC);
            } break;

            case ActionKind::Halt: {
                // Minimal: ud2 for halt
                code.bytes({ 0x0F, 0x0B });
            } break;

            default:
                break;
            }
        }

        // ----- procedure emission -----
        EmitResult emit_proc() {
            frame = compute_frame_layout(proc, ap, layout);

            // Prepare block label table
            block_labels.resize(proc.blocks.size());

            // Prologue
            enc::push_rbp(code);
            enc::mov_rbp_rsp(code);
            if (frame.total_bytes) enc::sub_rsp_imm32(code, frame.total_bytes);

            // Emit blocks in order (deterministic)
            for (const auto& b : proc.blocks) {
                bind_block(b.id);
                for (const auto& a : b.actions) emit_action(a);
            }

            // Epilogue (if no explicit return yet; your plan can encode returns as assignments + Jump to epilogue)
            enc::mov_rsp_rbp(code);
            enc::pop_rbp(code);
            enc::ret(code);

            // Patch block rel32
            for (auto& p : patches) {
                if (p.kind == PatchKind::Rel32_Jmp || p.kind == PatchKind::Rel32_Jcc) {
                    auto& L = block_labels.at(p.target_block.v);
                    assert(L.bound);
                    int32_t rel = (int32_t)L.pos - (int32_t)(p.at + 4); // rel from end of imm32
                    code.patch_i32(p.at, rel);
                }
            }

            // Note: symbol call patches are left for the final link/loader step (ExecMeta relocs).

            EmitResult out;
            out.code = std::move(code.b);
            out.patches = std::move(patches);
            out.frame = frame;
            return out;
        }
    };

} // namespace rane::x64



// ============================================================================
// File: rane_execmeta.hpp  (C++20, binary writer + symbol/reloc tables)
// ============================================================================
//
// ExecMeta format (binary):
//
//   struct Header {
//     u32 magic = 'R''E''M''1';   // 0x314D4552
//     u16 version = 1;
//     u16 endian  = 1;           // 1 = little
//     u32 header_size;
//     u32 proc_count;
//     u32 sym_count;
//     u32 reloc_count;
//     u32 str_bytes;
//     u32 procs_off;
//     u32 syms_off;
//     u32 relocs_off;
//     u32 str_off;
//   }
//
//   ProcRec[proc_count]:
//     u32 proc_symbol_id;
//     u32 name_str_off;          // offset in string table
//     u32 code_off;              // offset in .text blob inside your module package
//     u32 code_size;
//     u32 caps_word_off;         // offset in caps_words area (u64 words) relative to procs_off end
//     u32 caps_word_count;
//     u32 frame_size;            // total_bytes (for debugging / stack probes)
//     u32 reserved;
//
//   SymRec[sym_count]:
//     u32 symbol_id;             // your SymbolId.v
//     u32 name_str_off;
//     u8  kind;                  // SymKind
//     u8  reserved0[3];
//     u32 aux0;                  // optional (import ordinal, dll index, etc)
//     u32 aux1;
//
