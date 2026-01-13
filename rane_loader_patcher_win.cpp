// ============================================================================
// File: rane_loader_patcher.cpp   (C++20, ready-to-compile)
// ============================================================================
//
// Full loader-side patcher for ExecMeta as defined in rane_execmeta.hpp.
//
// What it does:
// - Reads ExecMeta blob (Header + ProcRec + caps + SymRec + RelocRec + strings)
// - Uses a user-provided resolver callback to map SymRec -> absolute address
// - Applies relocations into the module's .text memory
//
// Supports (exact rules):
// - Rel32_Call: write i32 rel = (sym_addr + addend) - (patch_site + 4)
//               where patch_site is address of the rel32 immediate (E8 imm32)
// - Abs64_Imm : write u64 imm = (sym_addr + addend) into the 8-byte immediate
//
// Notes:
// - This is not a PE loader. You call this after you have copied .text into RW memory.
// - After patching, you should make .text RX (and optionally flush i-cache).
//
// Build:
//   g++ -std=c++20 -O2 -Wall -Wextra rane_loader_patcher.cpp -o rane_loader_patcher
// or MSVC/clang-cl.

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <iostream>

namespace rane::execmeta {

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using i32 = int32_t;
    using u64 = uint64_t;
    using i64_t = int64_t;

    static constexpr u32 kMagic = 0x314D4552u; // 'R''E''M''1'

    enum class SymKind : u8 { Proc = 0, Global = 1, ImportThunk = 2 };

    enum class RelocKind : u8 {
        Rel32_Call = 0,
        Abs64_Imm = 1,
        RipRel32 = 2, // unused here
    };

#pragma pack(push, 1)
    struct Header {
        u32 magic = 0;
        u16 version = 0;
        u16 endian = 0;
        u32 header_size = 0;
        u32 proc_count = 0;
        u32 sym_count = 0;
        u32 reloc_count = 0;
        u32 str_bytes = 0;
        u32 procs_off = 0;
        u32 syms_off = 0;
        u32 relocs_off = 0;
        u32 str_off = 0;
    };

    struct ProcRec {
        u32 proc_symbol_id = 0;
        u32 name_str_off = 0;
        u32 code_off = 0;
        u32 code_size = 0;
        u32 caps_word_off = 0;
        u32 caps_word_count = 0;
        u32 frame_size = 0;
        u32 reserved = 0;
    };

    struct SymRec {
        u32 symbol_id = 0;
        u32 name_str_off = 0;
        u8  kind = 0;
        u8  reserved0[3] = { 0,0,0 };
        u32 aux0 = 0;
        u32 aux1 = 0;
    };

    struct RelocRec {
        u32 at_code_off = 0;  // absolute offset into module .text
        u32 symbol_id = 0;  // SymbolId.v
        u8  kind = 0;  // RelocKind
        u8  reserved[3] = { 0,0,0 };
        i32 addend = 0;
    };
#pragma pack(pop)

    // ------------------------------
    // Safe span reader for ExecMeta
    // ------------------------------
    struct View {
        const u8* p = nullptr;
        size_t    n = 0;

        View() = default;
        View(const void* data, size_t size) : p((const u8*)data), n(size) {}

        template <class T>
        const T* at(size_t off) const {
            if (off + sizeof(T) > n) throw std::runtime_error("ExecMeta: OOB read");
            return reinterpret_cast<const T*>(p + off);
        }

        const u8* bytes_at(size_t off, size_t len) const {
            if (off + len > n) throw std::runtime_error("ExecMeta: OOB read");
            return p + off;
        }

        std::string_view cstr_at(size_t off) const {
            if (off >= n) throw std::runtime_error("ExecMeta: OOB string");
            const char* s = reinterpret_cast<const char*>(p + off);
            size_t max = n - off;
            size_t i = 0;
            for (; i < max; ++i) {
                if (s[i] == '\0') break;
            }
            if (i == max) throw std::runtime_error("ExecMeta: unterminated string");
            return std::string_view(s, i);
        }
    };

    // ------------------------------
    // Decoded ExecMeta tables
    // ------------------------------
    struct ExecMeta {
        Header hdr{};
        std::vector<ProcRec>  procs;
        std::vector<SymRec>   syms;
        std::vector<RelocRec> relocs;
        std::string_view      strtab_base;
        size_t                strtab_size = 0;

        // quick lookup: symbol_id -> SymRec*
        std::unordered_map<u32, const SymRec*> sym_by_id;

        std::string_view sym_name(const SymRec& s) const {
            return std::string_view(strtab_base.data() + s.name_str_off,
                std::strlen(strtab_base.data() + s.name_str_off));
        }
    };

    // Parse/validate ExecMeta
    static ExecMeta parse_execmeta(const void* data, size_t size) {
        View v(data, size);
        ExecMeta em{};

        const Header* H = v.at<Header>(0);
        em.hdr = *H;

        if (em.hdr.magic != kMagic) throw std::runtime_error("ExecMeta: bad magic");
        if (em.hdr.endian != 1) throw std::runtime_error("ExecMeta: unsupported endian");
        if (em.hdr.version != 1) throw std::runtime_error("ExecMeta: unsupported version");
        if (em.hdr.header_size < sizeof(Header)) throw std::runtime_error("ExecMeta: bad header_size");
        if (em.hdr.str_off + em.hdr.str_bytes > size) throw std::runtime_error("ExecMeta: bad strtab bounds");

        // procs
        {
            size_t need = (size_t)em.hdr.proc_count * sizeof(ProcRec);
            const u8* p = v.bytes_at(em.hdr.procs_off, need);
            em.procs.resize(em.hdr.proc_count);
            std::memcpy(em.procs.data(), p, need);
        }
        // syms
        {
            size_t need = (size_t)em.hdr.sym_count * sizeof(SymRec);
            const u8* p = v.bytes_at(em.hdr.syms_off, need);
            em.syms.resize(em.hdr.sym_count);
            std::memcpy(em.syms.data(), p, need);
        }
        // relocs
        {
            size_t need = (size_t)em.hdr.reloc_count * sizeof(RelocRec);
            const u8* p = v.bytes_at(em.hdr.relocs_off, need);
            em.relocs.resize(em.hdr.reloc_count);
            std::memcpy(em.relocs.data(), p, need);
        }

        em.strtab_base = std::string_view(reinterpret_cast<const char*>(v.bytes_at(em.hdr.str_off, em.hdr.str_bytes)),
            em.hdr.str_bytes);
        em.strtab_size = em.hdr.str_bytes;

        // build sym index
        for (auto& s : em.syms) {
            // validate name offset
            if (s.name_str_off >= em.hdr.str_bytes) throw std::runtime_error("ExecMeta: SymRec bad name_str_off");
            // validate string is terminated (cstr_at will check bounds)
            (void)v.cstr_at(em.hdr.str_off + s.name_str_off);
            em.sym_by_id.emplace(s.symbol_id, &s);
        }

        return em;
    }

    // ------------------------------
    // Symbol resolution interface
    // ------------------------------
    //
    // You provide this. It maps a SymRec (kind+name+aux) to an absolute address.
    // Typical implementations:
    // - Proc: address = text_base + ProcRec.code_off (if intra-module call)
    // - Global: address = global_base + offset (or a runtime table)
    // - ImportThunk: address = import_thunk_addr (IAT slot or a code stub)
    //
    // If resolution fails, throw or return 0.
    using ResolveFn = std::function<u64(const SymRec&, std::string_view name)>;

    // ------------------------------
    // Relocation application
    // ------------------------------
    static inline void write_u32(void* dst, u32 v) {
        std::memcpy(dst, &v, sizeof(v));
    }
    static inline void write_i32(void* dst, i32 v) {
        std::memcpy(dst, &v, sizeof(v));
    }
    static inline void write_u64(void* dst, u64 v) {
        std::memcpy(dst, &v, sizeof(v));
    }

    struct PatchStats {
        u32 rel32_calls = 0;
        u32 abs64_imms = 0;
    };

    static PatchStats apply_relocs(
        const ExecMeta& em,
        u8* text_base,          // writable pointer to .text contents
        size_t text_size,       // size of .text blob
        const ResolveFn& resolve
    ) {
        PatchStats st{};

        for (const auto& r : em.relocs) {
            // Bounds check patch site.
            if (r.at_code_off >= text_size) throw std::runtime_error("ExecMeta: reloc patch OOB (at_code_off)");
            u8* patch_site = text_base + r.at_code_off;

            // Lookup symbol.
            auto it = em.sym_by_id.find(r.symbol_id);
            if (it == em.sym_by_id.end()) throw std::runtime_error("ExecMeta: reloc references unknown symbol_id");
            const SymRec& sym = *it->second;

            // Name view
            const char* name_c = em.strtab_base.data() + sym.name_str_off;
            std::string_view name{ name_c };

            // Resolve address.
            u64 sym_addr = resolve(sym, name);
            if (sym_addr == 0) throw std::runtime_error("ExecMeta: resolve returned 0 address");

            // Apply according to kind.
            RelocKind kind = (RelocKind)r.kind;
            switch (kind) {
            case RelocKind::Rel32_Call: {
                // Patch assumes the bytes are: E8 <imm32>  and at_code_off points to imm32.
                // So RIP after imm is patch_site + 4.
                if ((size_t)r.at_code_off + 4 > text_size) throw std::runtime_error("ExecMeta: Rel32_Call patch OOB");
                u64 rip_after = (u64)(patch_site + 4);
                i64_t target = (i64_t)sym_addr + (i64_t)r.addend;
                i64_t rel64 = target - (i64_t)rip_after;

                // Must fit in signed 32-bit.
                if (rel64 < (i64_t)INT32_MIN || rel64 >(i64_t)INT32_MAX)
                    throw std::runtime_error("ExecMeta: Rel32_Call out of range");

                write_i32(patch_site, (i32)rel64);
                st.rel32_calls++;
            } break;

            case RelocKind::Abs64_Imm: {
                // Patch 8-byte immediate.
                if ((size_t)r.at_code_off + 8 > text_size) throw std::runtime_error("ExecMeta: Abs64_Imm patch OOB");
                u64 v = sym_addr + (i64_t)r.addend;
                write_u64(patch_site, v);
                st.abs64_imms++;
            } break;

            default:
                throw std::runtime_error("ExecMeta: unsupported reloc kind");
            }
        }

        return st;
    }

} // namespace rane::execmeta

// ============================================================================
// Example: a minimal resolver that supports intra-module proc symbols and imports
// ============================================================================
//
// This is just to show usage. In your real loader, you will:
// - map proc_symbol_id -> ProcRec so Proc symbols can resolve to code offsets
// - build import table and resolve ImportThunk symbols to IAT/thunk addresses

namespace rane::loader_demo {

    using namespace rane::execmeta;

    struct ModuleContext {
        u8* text_base = nullptr;
        size_t text_size = 0;

        // Map symbol_id -> ProcRec for intra-module procs
        std::unordered_map<u32, ProcRec> proc_by_symbol;

        // Map import name -> address (from GetProcAddress-like mechanism)
        std::unordered_map<std::string, u64> imports;

        // Map global name -> address (optional)
        std::unordered_map<std::string, u64> globals;
    };

    // Build proc_by_symbol from ExecMeta ProcRec list
    static void index_procs(ModuleContext& M, const ExecMeta& em) {
        for (const auto& p : em.procs) {
            M.proc_by_symbol.emplace(p.proc_symbol_id, p);
        }
    }

    static ResolveFn make_resolver(ModuleContext& M, const ExecMeta& em) {
        return [&](const SymRec& s, std::string_view name) -> u64 {
            SymKind k = (SymKind)s.kind;

            if (k == SymKind::Proc) {
                auto it = M.proc_by_symbol.find(s.symbol_id);
                if (it != M.proc_by_symbol.end()) {
                    const ProcRec& pr = it->second;
                    if ((size_t)pr.code_off + pr.code_size > M.text_size)
                        throw std::runtime_error("Resolver: proc code_off out of .text bounds");
                    return (u64)(M.text_base + pr.code_off);
                }
                // Some procs may be external; fall back to import map by name.
                auto it2 = M.imports.find(std::string(name));
                if (it2 != M.imports.end()) return it2->second;
                return 0;
            }

            if (k == SymKind::ImportThunk) {
                auto it = M.imports.find(std::string(name));
                if (it != M.imports.end()) return it->second;
                return 0;
            }

            if (k == SymKind::Global) {
                auto it = M.globals.find(std::string(name));
                if (it != M.globals.end()) return it->second;
                return 0;
            }

            return 0;
            };
    }

} // namespace rane::loader_demo

// ============================================================================
// Optional compile-test main()
// Remove this in your real loader.
// ============================================================================
#ifdef RANE_LOADER_PATCHER_TEST

int main() {
    using namespace rane::execmeta;
    using namespace rane::loader_demo;

    // In a real scenario, you would read these from your module file/package:
    std::vector<u8> execmeta_blob;   // filled with Writer::finalize().bytes
    std::vector<u8> text_blob;       // .text bytes emitted from your compiler
    // ... load them here ...

    // This test main can't do much without real blobs.
    std::cout << "Define RANE_LOADER_PATCHER_TEST and provide real blobs to test.\n";
    return 0;
}

#endif
