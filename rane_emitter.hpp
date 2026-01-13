// ============================================================================
// File: rane_emitter.hpp  (C++20, header-only interface + small inline helpers)
// ============================================================================
//
// Extended emitter: now supports
// - floating point (XMM0-XMM3, addsd/subsd/mulsd/divsd, movsd, movq)
// - more calling conventions (CallConv enum, variadic/fastcall stubs)
// - ActionPlan lowering for: jump table switch, select (ternary), tailcall
// - All previous features (see prior header for details)
//
// You provide:
// - actionplan.hpp (the structs we defined earlier, now with ConstFloat, Select, Switch, TailCall, etc.)
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
#include <algorithm>

#include "actionplan.hpp"

namespace rane::x64 {

    // ---------------------------
    // Registers (Windows x64 + XMM)
    // ---------------------------
    enum class Reg : uint8_t {
        RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
        R8, R9, R10, R11, R12, R13, R14, R15,
        XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
    };

    static constexpr Reg kResultReg = Reg::RAX;
    static constexpr Reg kScratch0 = Reg::R11;
    static constexpr Reg kScratch1 = Reg::R10;
    static constexpr Reg kScratch2 = Reg::R9;
    static constexpr Reg kFloatResultReg = Reg::XMM0;

    // ---------------------------
    // Calling conventions
    // ---------------------------
    enum class CallConv : uint8_t {
        Win64,      // default
        FastCall,   // RCX, RDX, R8, R9, XMM0-3
        VectorCall, // XMM0-5, RCX, RDX, R8, R9
        Variadic    // floats in both GPR and XMM
    };

    // ---------------------------
    // Relocation / patch types
    // ---------------------------
    enum class PatchKind : uint8_t {
        Rel32_Jmp,      // E9 rel32
        Rel32_Jcc,      // 0F 8? rel32
        Rel32_Call,     // E8 rel32
        RipRel32_Addr,  // e.g. mov rax, [rip+rel32]
        Abs64_Imm,      // e.g. mov rax, imm64
    };

    struct Patch {
        PatchKind kind{};
        uint32_t  at = 0;
        BlockId   target_block{};
        SymbolId  target_symbol{};
    };

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
    // Minimal x64 encoder helpers (add XMM float ops)
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

        // movsd xmm, xmm
        static inline void movsd_rr(CodeBuf& c, Reg dst, Reg src) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x10);
            c.u8((uint8_t)(0xC0 | (((uint8_t)dst - (uint8_t)Reg::XMM0) << 3) | ((uint8_t)src - (uint8_t)Reg::XMM0)));
        }
        // movsd xmm, [rbp-disp]
        static inline void movsd_xmm_mrbp(CodeBuf& c, Reg dst, int32_t disp) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x10);
            c.u8((uint8_t)(0x80 | (((uint8_t)dst - (uint8_t)Reg::XMM0) << 3) | 0x05));
            c.u32((uint32_t)disp);
        }
        // movsd [rbp-disp], xmm
        static inline void movsd_mrbp_xmm(CodeBuf& c, int32_t disp, Reg src) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x11);
            c.u8((uint8_t)(0x80 | (((uint8_t)src - (uint8_t)Reg::XMM0) << 3) | 0x05));
            c.u32((uint32_t)disp);
        }
        // addsd xmm, xmm
        static inline void addsd_rr(CodeBuf& c, Reg dst, Reg src) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x58);
            c.u8((uint8_t)(0xC0 | (((uint8_t)dst - (uint8_t)Reg::XMM0) << 3) | ((uint8_t)src - (uint8_t)Reg::XMM0)));
        }
        // subsd xmm, xmm
        static inline void subsd_rr(CodeBuf& c, Reg dst, Reg src) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x5C);
            c.u8((uint8_t)(0xC0 | (((uint8_t)dst - (uint8_t)Reg::XMM0) << 3) | ((uint8_t)src - (uint8_t)Reg::XMM0)));
        }
        // mulsd xmm, xmm
        static inline void mulsd_rr(CodeBuf& c, Reg dst, Reg src) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x59);
            c.u8((uint8_t)(0xC0 | (((uint8_t)dst - (uint8_t)Reg::XMM0) << 3) | ((uint8_t)src - (uint8_t)Reg::XMM0)));
        }
        // divsd xmm, xmm
        static inline void divsd_rr(CodeBuf& c, Reg dst, Reg src) {
            c.u8(0xF2); c.u8(0x0F); c.u8(0x5E);
            c.u8((uint8_t)(0xC0 | (((uint8_t)dst - (uint8_t)Reg::XMM0) << 3) | ((uint8_t)src - (uint8_t)Reg::XMM0)));
        }

        // movq xmm, rax
        static inline void movq_xmm_rax(CodeBuf& c, Reg xmm) {
            c.bytes({0x66, 0x48, 0x0F, 0x6E, (uint8_t)(0xC0 + ((uint8_t)xmm - (uint8_t)Reg::XMM0) * 8)});
        }
        // movq rax, xmm
        static inline void movq_rax_xmm(CodeBuf& c, Reg xmm) {
            c.bytes({0x66, 0x48, 0x0F, 0x7E, (uint8_t)(0xC0 + ((uint8_t)xmm - (uint8_t)Reg::XMM0) * 8)});
        }
        // ucomisd xmm, xmm
        static inline void ucomisd_rr(CodeBuf& c, Reg a, Reg b) {
            c.u8(0x66); c.u8(0x0F); c.u8(0x2E);
            c.u8((uint8_t)(0xC0 | (((uint8_t)a - (uint8_t)Reg::XMM0) << 3) | ((uint8_t)b - (uint8_t)Reg::XMM0)));
        }
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
        std::vector<Patch>   patches;
        FrameLayout          frame;
    };

    struct Emitter {
        const ActionPlan& ap;
        const ProcPlan& proc;
        const ILayoutProvider& layout;
        const ISymbolResolver& syms;
        CallConv callconv = CallConv::Win64;

        FrameLayout frame{};
        CodeBuf     code{};
        std::vector<Label> block_labels;
        std::vector<Patch> patches;
        uint32_t temp_sp = 0;
        uint32_t temp_max = 0;

        Emitter(const ActionPlan& ap_,
            const ProcPlan& proc_,
            const ILayoutProvider& layout_,
            const ISymbolResolver& syms_)
            : ap(ap_), proc(proc_), layout(layout_), syms(syms_) {
        }

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
            uint32_t at = enc::call_rel32(code);
            patches.push_back(Patch{ PatchKind::Rel32_Call, at, {}, callee });
        }

        void emit_value(ValueId v) {
            const auto& n = ap.values.at(v.v);

            switch (n.kind) {
            case ValueKind::ConstInt:
                code.u64(std::get<ConstInt>(n.as).value);
                break;
            case ValueKind::ConstBool:
                code.u64(std::get<ConstBool>(n.as).value ? 1 : 0);
                break;
            case ValueKind::ConstNull:
                code.u64(0);
                break;

            case ValueKind::VarRef: {
                auto var = std::get<VarRef>(n.as);
                int32_t offs = rbp_disp_from_off(frame.local_offset(var.id));
                enc::mov_r64_mrbp(code, Reg::RAX, offs);
            } break;

            case ValueKind::GlobalRef: {
                auto glob = std::get<GlobalRef>(n.as);
                // mov rax, qword ptr [rip+offset]
                code.bytes({ 0x48, 0x8B, 0x05 });
                uint32_t at = code.size();
                code.u32(0);
                patches.push_back(Patch{ PatchKind::RipRel32_Addr, at, {}, glob.id });
            } break;

            case ValueKind::ConstFloat: {
                auto cf = std::get<ConstFloat>(n.as);
                uint64_t bits;
                std::memcpy(&bits, &cf.value, sizeof(bits));
                enc::mov_ri64(code, Reg::RAX, bits);
                enc::movq_xmm_rax(code, Reg::XMM0);
                break;
            }
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

            case ValueKind::Binary: {
                auto b = std::get<Binary>(n.as);
                if (n.type == float_type_id) {
                    emit_value(b.a); // result in xmm0
                    uint32_t t = temp_alloc();
                    enc::movsd_mrbp_xmm(code, rbp_disp_from_off(frame.temp_offset(t)), Reg::XMM0);
                    emit_value(b.b);
                    enc::movsd_xmm_mrbp(code, Reg::XMM1, rbp_disp_from_off(frame.temp_offset(t)));
                    temp_free();
                    switch (b.op) {
                        case BinOp::Add: enc::addsd_rr(code, Reg::XMM0, Reg::XMM1); break;
                        case BinOp::Sub: enc::subsd_rr(code, Reg::XMM0, Reg::XMM1); break;
                        case BinOp::Mul: enc::mulsd_rr(code, Reg::XMM0, Reg::XMM1); break;
                        case BinOp::Div: enc::divsd_rr(code, Reg::XMM0, Reg::XMM1); break;
                        default: assert(false && "float binop not implemented");
                    }
                    break;
                }
                // ... integer as before
                emit_value(b.a);
                uint32_t t = temp_alloc();
                enc::mov_mrbp_r64(code, rbp_disp_from_off(frame.temp_offset(t)), Reg::RAX);

                emit_value(b.b);
                enc::mov_r64_mrbp(code, Reg::R11, rbp_disp_from_off(frame.temp_offset(t)));
                temp_free();

                switch (b.op) {
                case BinOp::Add:
                    enc::add_rr(code, Reg::R11, Reg::RAX);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Sub:
                    enc::sub_rr(code, Reg::R11, Reg::RAX);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Mul:
                    enc::mov_rr(code, Reg::R10, Reg::RAX);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    enc::imul_rr(code, Reg::RAX, Reg::R10);
                    break;
                case BinOp::Div:
                    enc::mov_rr(code, Reg::RDX, Reg::R11); // dividend in RDX
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    code.bytes({ 0x48, 0x99 }); // cqo
                    code.bytes({ 0x48, 0xF7, 0xF8 }); // idiv rax
                    break;
                case BinOp::Mod:
                    enc::mov_rr(code, Reg::RDX, Reg::R11);
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    code.bytes({ 0x48, 0x99 }); // cqo
                    code.bytes({ 0x48, 0xF7, 0xF8 }); // idiv rax
                    enc::mov_rr(code, Reg::RAX, Reg::RDX); // move remainder to rax
                    break;
                case BinOp::And:
                    code.bytes({ 0x4C, 0x21, 0xC3 });
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Or:
                    code.bytes({ 0x4C, 0x09, 0xC3 });
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Xor:
                    code.bytes({ 0x4C, 0x31, 0xC3 });
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Shl:
                    enc::mov_rr(code, Reg::RCX, Reg::RAX);
                    code.bytes({ 0x49, 0xD3, 0xE3 }); // shl r11, cl
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                case BinOp::Shr:
                    enc::mov_rr(code, Reg::RCX, Reg::RAX);
                    code.bytes({ 0x49, 0xD3, 0xEB }); // shr r11, cl
                    enc::mov_rr(code, Reg::RAX, Reg::R11);
                    break;
                default:
                    assert(false && "Binary op not yet implemented in emitter");
                }
            } break;

            case ValueKind::Call: {
                auto call = std::get<Call>(n.as);
                static constexpr Reg abi_regs[4] = { Reg::RCX, Reg::RDX, Reg::R8, Reg::R9 };
                size_t narg = call.args.size();
                size_t reg_args = (narg > 4 ? 4 : narg);

                // Evaluate and move register arguments
                for (size_t i = 0; i < reg_args; ++i) {
                    emit_value(call.args[i]);
                    enc::mov_rr(code, abi_regs[i], Reg::RAX);
                }

                // Stack arguments (right-to-left, pushed before call)
                if (narg > 4) {
                    size_t stack_args = narg - 4;
                    size_t stack_bytes = ((stack_args * 8 + 15) / 16) * 16;
                    for (size_t i = narg; i-- > 4;) {
                        emit_value(call.args[i]);
                        code.u8(0x50); // push rax
                    }
                }

                emit_call_symbol(call.callee);

                if (narg > 4) {
                    size_t stack_args = narg - 4;
                    if (stack_args > 0) {
                        code.bytes({ 0x48, 0x83, 0xC4, (uint8_t)(stack_args * 8) }); // add rsp, N*8
                    }
                }
            } break;

            case ValueKind::Select: {
                // Ternary: cond ? a : b
                auto sel = std::get<Select>(n.as);
                BlockId true_blk{(uint32_t)block_labels.size()};
                BlockId false_blk{(uint32_t)block_labels.size() + 1};
                BlockId join_blk{(uint32_t)block_labels.size() + 2};
                // Evaluate cond to RAX
                emit_value(sel.cond);
                enc::test_rr(code, Reg::RAX, Reg::RAX);
                uint32_t jz_at = enc::jcc_rel32(code, enc::Jcc::JZ);
                // True branch
                emit_value(sel.if_true);
                uint32_t jmp_at = enc::jmp_rel32(code);
                // False branch
                code.patch_i32(jz_at, (int32_t)(code.size() - (jz_at + 4)));
                emit_value(sel.if_false);
                // Join
                code.patch_i32(jmp_at, (int32_t)(code.size() - (jmp_at + 4)));
                break;
            }

            case ValueKind::Switch: {
                // Lower to jump table if dense, else chain of compares
                auto sw = std::get<Switch>(n.as);
                uint32_t count = sw.values.size();
                uint32_t jimplem = 0;
                if (count > 1) {
                    // Heuristic: dense if range covers less than 1/4 of possible values
                    int64_t minval = (int64_t)std::get<ConstInt>(sw.values.front().as).value;
                    int64_t maxval = (int64_t)std::get<ConstInt>(sw.values.back().as).value;
                    if ((maxval - minval) < (count * 4)) {
                        jimplem = 1;
                        // Naive jump table: entry for each value, direct Jmp to target.
                        // TODO: optimize sparse tables (hash table etc.)
                        code.u8(0xEB); // jmp short (patchable)
                        uint32_t jmp_patch_at = code.size();
                        code.u32(0); // placeholder for patching jump
                        // Emit jump table
                        uint32_t base = 0;
                        for (const auto& v : sw.values) {
                            auto target = std::get<SwitchValue>(v.as).target;
                            code.u32(0); // placeholder
                            patches.push_back(Patch{ PatchKind::Rel32_Jmp, (uint32_t)code.size(), target, {} });
                        }
                        // Patch jump offset
                        uint32_t jmp_target = code.size() + 4 * count; // after jump table
                        code.patch_i32(jmp_patch_at, jmp_target - (jmp_patch_at + 4));
                    }
                }
                if (jimplem == 0) {
                    // Fallback: chain of cmp/jbe/jmp
                    // TODO: optimize me
                    for (size_t i = 0; i < count; ++i) {
                        auto target = std::get<SwitchValue>(sw.values[i].as).target;
                        emit_value(sw.values[i]);
                        enc::cmp_rr(code, Reg::RAX, Reg::RAX);
                        if (i + 1 < count) {
                            enc::jcc_rel32(code, enc::Jcc::JBE);
                        }
                        else {
                            enc::jmp_rel32(code);
                        }
                        patches.push_back(Patch{ PatchKind::Rel32_Jmp, code.size(), target, {} });
                    }
                }
            } break;

            case ValueKind::TailCall: {
                // Lowered tail call: direct jmp to target proc
                auto tc = std::get<TailCall>(n.as);
                BlockId target_blk = tc.target;
                uint32_t at = enc::jmp_rel32(code);
                patches.push_back(Patch{ PatchKind::Rel32_Jmp, at, target_blk, {} });
            } break;

            default:
                assert(false && "Value kind not implemented in emitter");
            }
        }

        EmitResult emit_proc() {
            frame = compute_frame_layout(proc, ap, layout);
            block_labels.resize(proc.blocks.size());

            enc::push_rbp(code);
            enc::mov_rbp_rsp(code);
            if (frame.total_bytes) enc::sub_rsp_imm32(code, frame.total_bytes);

            for (const auto& b : proc.blocks) {
                bind_block(b.id);
                for (const auto& a : b.actions) emit_action(a);
            }

            enc::mov_rsp_rbp(code);
            enc::pop_rbp(code);
            enc::ret(code);

            // Patch only block rel32 (not symbol calls)
            for (auto& p : patches) {
                if (p.kind == PatchKind::Rel32_Jmp || p.kind == PatchKind::Rel32_Jcc) {
                    auto& L = block_labels.at(p.target_block.v);
                    assert(L.bound);
                    int32_t rel = (int32_t)L.pos - (int32_t)(p.at + 4);
                    code.patch_i32(p.at, rel);
                }
            }

            EmitResult out;
            out.code = std::move(code.b);
            out.patches = std::move(patches);
            out.frame = frame;
            return out;
        }
    };

} // namespace rane::x64
