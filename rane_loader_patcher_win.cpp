// ============================================================================
// File: rane_loader_patcher_win.cpp   (C++20, Windows-only, ready-to-compile)
// ============================================================================
//
// Adds:
//  1) Windows-only VirtualProtect + FlushInstructionCache
//  2) Import resolver (name -> addr) using LoadLibraryA/GetProcAddress
//  3) Optional CFG/DEP-friendly import thunks (direct call -> thunk, thunk -> target)
//
// ExecMeta format matches earlier:
//  - SymRec.kind == ImportThunk means "this symbol is an imported function"
//  - RelocRec kinds supported: Rel32_Call and Abs64_Imm
//
// Import name format (supported):
//  - "KERNEL32.dll!ExitProcess"
//  - "kernel32!ExitProcess"
//  - "KERNEL32.dll::ExitProcess"
//  - "ExitProcess" (fallback: tries in already-loaded modules list you provide)
//  - You can also store plain function names if you resolve by a fixed DLL list.
//
// Thunk strategy (CFG/DEP-friendly):
//  - We generate *in-memory* thunks:
//        mov rax, imm64
//        jmp rax
//    Call sites are patched as *direct* Rel32_Call to the thunk address.
//  - DEP: thunks are allocated RW during build, then made RX.
//  - CFG: if CFG is enabled for the process, dynamic code pages may require
//    marking thunk entrypoints as valid call targets via SetProcessValidCallTargets.
//    We attempt to do that if the API exists; otherwise we keep working best-effort.
//
// Build (MSVC):
//   cl /std:c++20 /O2 /W4 rane_loader_patcher_win.cpp
//
// Build (clang-cl):
//   clang-cl /std:c++20 /O2 /W4 rane_loader_patcher_win.cpp
//
// Build (mingw g++):
//   g++ -std=c++20 -O2 -Wall -Wextra rane_loader_patcher_win.cpp -o rane_loader_patcher_win.exe
//
// ============================================================================

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <optional>
#include <iostream>

namespace rane::execmeta {

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using i32 = int32_t;
    using u64 = uint64_t;

    static constexpr u32 kMagic = 0x314D4552u; // 'R''E''M''1'

    enum class SymKind : u8 { Proc = 0, Global = 1, ImportThunk = 2 };
    enum class RelocKind : u8 { Rel32_Call = 0, Abs64_Imm = 1, RipRel32 = 2 /*unused*/ };

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
            for (; i < max; ++i) if (s[i] == '\0') break;
            if (i == max) throw std::runtime_error("ExecMeta: unterminated string");
            return std::string_view(s, i);
        }
    };

    struct ExecMeta {
        Header hdr{};
        std::vector<ProcRec>  procs;
        std::vector<SymRec>   syms;
        std::vector<RelocRec> relocs;
        std::string_view      strtab_base;
        size_t                strtab_size = 0;
        std::unordered_map<u32, const SymRec*> sym_by_id;
    };

    static ExecMeta parse_execmeta(const void* data, size_t size) {
        View v(data, size);
        ExecMeta em{};

        const Header* H = v.at<Header>(0);
        em.hdr = *H;

        if (em.hdr.magic != kMagic) throw std::runtime_error("ExecMeta: bad magic");
        if (em.hdr.endian != 1) throw std::runtime_error("ExecMeta: unsupported endian");
        if (em.hdr.version != 1) throw std::runtime_error("ExecMeta: unsupported version");
        if (em.hdr.header_size < sizeof(Header)) throw std::runtime_error("ExecMeta: bad header_size");
        if ((size_t)em.hdr.str_off + (size_t)em.hdr.str_bytes > size) throw std::runtime_error("ExecMeta: bad strtab bounds");

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

        // build sym index + validate string offsets
        for (auto& s : em.syms) {
            if (s.name_str_off >= em.hdr.str_bytes) throw std::runtime_error("ExecMeta: SymRec bad name_str_off");
            (void)v.cstr_at(em.hdr.str_off + s.name_str_off);
            em.sym_by_id.emplace(s.symbol_id, &s);
        }

        return em;
    }

    static inline std::string_view sym_name(const ExecMeta& em, const SymRec& s) {
        const char* base = em.strtab_base.data();
        return std::string_view(base + s.name_str_off);
    }

} // namespace rane::execmeta

// ============================================================================
// Windows memory helpers: VirtualProtect + FlushInstructionCache
// ============================================================================
namespace rane::winmem {

    static void protect(void* addr, size_t size, DWORD newProtect) {
        DWORD oldProtect = 0;
        if (!::VirtualProtect(addr, size, newProtect, &oldProtect)) {
            throw std::runtime_error("VirtualProtect failed");
        }
    }

    static void flush_icache(void* addr, size_t size) {
        HANDLE proc = ::GetCurrentProcess();
        if (!::FlushInstructionCache(proc, addr, size)) {
            throw std::runtime_error("FlushInstructionCache failed");
        }
    }

} // namespace rane::winmem

// ============================================================================
// CFG-aware thunk allocation / marking
// ============================================================================
namespace rane::thunks {

    using u8 = uint8_t;
    using u64 = uint64_t;

    struct Thunk {
        u8* entry = nullptr;
        u64 target = 0;
    };

    // Dynamically load SetProcessValidCallTargets if present.
    using SetProcessValidCallTargets_t = BOOL(WINAPI*)(
        HANDLE, PVOID, SIZE_T, ULONG, PCFG_CALL_TARGET_INFO
        );

    static SetProcessValidCallTargets_t load_SetProcessValidCallTargets() {
        HMODULE k32 = ::GetModuleHandleA("kernel32.dll");
        if (!k32) return nullptr;
        return reinterpret_cast<SetProcessValidCallTargets_t>(
            ::GetProcAddress(k32, "SetProcessValidCallTargets")
            );
    }

    static bool try_mark_cfg_valid(void* base, size_t size, void* entry) {
        auto fn = load_SetProcessValidCallTargets();
        if (!fn) return false;

        // entry offset in region
        uintptr_t b = (uintptr_t)base;
        uintptr_t e = (uintptr_t)entry;
        if (e < b || e >= b + size) return false;

        CFG_CALL_TARGET_INFO info{};
        info.Offset = (ULONG)(e - b);
        info.Flags = CFG_CALL_TARGET_VALID;

        BOOL ok = fn(::GetCurrentProcess(), base, size, 1, &info);
        return ok == TRUE;
    }

    // A DEP/CFG-aware thunk pool.
    // - build(): RW memory
    // - finalize(): RX memory + optional CFG marking
    struct ThunkPool {
        u8* base = nullptr;
        size_t cap = 0;
        size_t used = 0;
        bool cfg_mark = false;
        std::vector<Thunk> thunks;

        explicit ThunkPool(bool cfg_mark_calltargets) : cfg_mark(cfg_mark_calltargets) {}

        ~ThunkPool() {
            if (base) ::VirtualFree(base, 0, MEM_RELEASE);
        }

        void ensure(size_t bytes) {
            if (base && used + bytes <= cap) return;

            // Allocate a new region (simple strategy). You can improve with grow+copy if desired.
            // For now, allocate enough for current + requested.
            size_t newCap = (cap == 0) ? 0x4000 : cap * 2;
            while (newCap < used + bytes) newCap *= 2;

            u8* newBase = (u8*)::VirtualAlloc(nullptr, newCap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!newBase) throw std::runtime_error("VirtualAlloc for thunk pool failed");

            // If we already had a pool, copy and free old (stable addresses would change).
            // To keep addresses stable, we instead *do not grow in place* in this minimal implementation.
            // So: require one-time sizing or create pool big enough initially.
            if (base) {
                ::VirtualFree(newBase, 0, MEM_RELEASE);
                throw std::runtime_error("ThunkPool grow not supported in this minimal loader; pre-size pool");
            }

            base = newBase;
            cap = newCap;
            used = 0;
        }

        // Thunk encoding:
        //   48 B8 imm64     ; mov rax, imm64
        //   FF E0          ; jmp rax
        // Total: 12 bytes
        Thunk* add_thunk(u64 target) {
            constexpr size_t kThunkSize = 12;
            ensure(kThunkSize);

            u8* p = base + used;

            // mov rax, imm64
            p[0] = 0x48; p[1] = 0xB8;
            std::memcpy(p + 2, &target, 8);
            // jmp rax
            p[10] = 0xFF; p[11] = 0xE0;

            used += kThunkSize;

            Thunk t{ p, target };
            thunks.push_back(t);

            return &thunks.back();
        }

        void finalize_to_rx() {
            if (!base || used == 0) return;

            rane::winmem::protect(base, cap, PAGE_EXECUTE_READ);
            rane::winmem::flush_icache(base, used);

            if (cfg_mark) {
                // Mark each entrypoint as valid call target if API available.
                for (auto& t : thunks) {
                    (void)try_mark_cfg_valid(base, cap, t.entry);
                }
            }
        }
    };

} // namespace rane::thunks

// ============================================================================
// Import resolver (LoadLibraryA/GetProcAddress) + optional thunks
// ============================================================================
namespace rane::imports {

    using namespace rane::execmeta;

    struct ImportSpec {
        std::string dll;
        std::string func;
    };

    // Parse supported formats:
    //  - "KERNEL32.dll!ExitProcess"
    //  - "KERNEL32.dll::ExitProcess"
    //  - "ExitProcess" (dll empty)
    static ImportSpec parse_import_name(std::string_view s) {
        ImportSpec out{};
        auto bang = s.find('!');
        auto dcol = s.find("::");

        if (bang != std::string_view::npos) {
            out.dll = std::string(s.substr(0, bang));
            out.func = std::string(s.substr(bang + 1));
            return out;
        }
        if (dcol != std::string_view::npos) {
            out.dll = std::string(s.substr(0, dcol));
            out.func = std::string(s.substr(dcol + 2));
            return out;
        }
        out.func = std::string(s);
        return out;
    }

    static FARPROC resolve_one(const ImportSpec& spec, const std::vector<std::string>& default_dlls) {
        auto try_one = [&](const char* dll, const char* fn) -> FARPROC {
            HMODULE h = ::GetModuleHandleA(dll);
            if (!h) h = ::LoadLibraryA(dll);
            if (!h) return nullptr;
            return ::GetProcAddress(h, fn);
            };

        if (!spec.dll.empty()) {
            return try_one(spec.dll.c_str(), spec.func.c_str());
        }

        // No dll specified: try default list
        for (const auto& d : default_dlls) {
            if (auto p = try_one(d.c_str(), spec.func.c_str())) return p;
        }

        return nullptr;
    }

    struct ImportTable {
        // key is exact original symbol name string (e.g., "kernel32.dll!ExitProcess")
        std::unordered_map<std::string, u64> addr_by_name;
        std::unordered_map<std::string, u64> thunk_by_name; // if thunks enabled
    };

    // Build a name->addr table by scanning ExecMeta symbols with kind ImportThunk.
    // If use_thunks:
    //   - creates a thunk per import and stores thunk addr
    //   - addr_by_name still stores the *real* target address
    static ImportTable build_import_table(
        const ExecMeta& em,
        const std::vector<std::string>& default_dlls,
        bool use_thunks,
        thunks::ThunkPool* pool_or_null
    ) {
        ImportTable T{};

        for (const auto& s : em.syms) {
            if ((SymKind)s.kind != SymKind::ImportThunk) continue;

            std::string_view nm = sym_name(em, s);
            ImportSpec spec = parse_import_name(nm);

            FARPROC p = resolve_one(spec, default_dlls);
            if (!p) {
                std::string msg = "Import resolve failed: " + std::string(nm);
                throw std::runtime_error(msg);
            }

            u64 addr = (u64)p;
            T.addr_by_name.emplace(std::string(nm), addr);

            if (use_thunks) {
                if (!pool_or_null) throw std::runtime_error("use_thunks requested but no ThunkPool provided");
                auto* th = pool_or_null->add_thunk(addr);
                T.thunk_by_name.emplace(std::string(nm), (u64)th->entry);
            }
        }

        return T;
    }

} // namespace rane::imports

// ============================================================================
// Relocation applier (same exact rules) + resolver wiring
// ============================================================================
namespace rane::patcher {

    using namespace rane::execmeta;

    static inline void write_i32(void* dst, i32 v) { std::memcpy(dst, &v, sizeof(v)); }
    static inline void write_u64(void* dst, u64 v) { std::memcpy(dst, &v, sizeof(v)); }

    struct PatchStats { u32 rel32_calls = 0; u32 abs64_imms = 0; };

    struct ModuleContext {
        u8* text_base = nullptr;
        size_t text_size = 0;

        // SymbolId -> ProcRec (for intra-module procs)
        std::unordered_map<u32, ProcRec> proc_by_symbol;

        // Import tables (built from ExecMeta)
        rane::imports::ImportTable imports;
        bool use_import_thunks = false;

        // (Optional) globals by name
        std::unordered_map<std::string, u64> globals;
    };

    static void index_procs(ModuleContext& M, const ExecMeta& em) {
        for (const auto& p : em.procs) {
            M.proc_by_symbol.emplace(p.proc_symbol_id, p);
        }
    }

    using ResolveFn = std::function<u64(const SymRec&, std::string_view)>;

    static ResolveFn make_resolver(ModuleContext& M, const ExecMeta& em) {
        return [&](const SymRec& s, std::string_view name) -> u64 {
            SymKind k = (SymKind)s.kind;

            if (k == SymKind::Proc) {
                // intra-module proc address if present
                auto it = M.proc_by_symbol.find(s.symbol_id);
                if (it != M.proc_by_symbol.end()) {
                    const ProcRec& pr = it->second;
                    if ((size_t)pr.code_off + pr.code_size > M.text_size)
                        throw std::runtime_error("Resolver: proc code_off out of .text bounds");
                    return (u64)(M.text_base + pr.code_off);
                }
                // else fallthrough: allow external procs resolved as imports by exact name
                auto it2 = M.imports.addr_by_name.find(std::string(name));
                if (it2 != M.imports.addr_by_name.end()) return it2->second;
                return 0;
            }

            if (k == SymKind::ImportThunk) {
                std::string key(name);
                if (M.use_import_thunks) {
                    auto it = M.imports.thunk_by_name.find(key);
                    if (it != M.imports.thunk_by_name.end()) return it->second;
                }
                auto it = M.imports.addr_by_name.find(key);
                if (it != M.imports.addr_by_name.end()) return it->second;
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

    static PatchStats apply_relocs(
        const ExecMeta& em,
        u8* text_base,
        size_t text_size,
        const ResolveFn& resolve
    ) {
        PatchStats st{};

        for (const auto& r : em.relocs) {
            if (r.at_code_off >= text_size) throw std::runtime_error("ExecMeta: reloc patch OOB (at_code_off)");
            u8* patch_site = text_base + r.at_code_off;

            auto it = em.sym_by_id.find(r.symbol_id);
            if (it == em.sym_by_id.end()) throw std::runtime_error("ExecMeta: reloc references unknown symbol_id");
            const SymRec& sym = *it->second;

            std::string_view name = sym_name(em, sym);

            u64 sym_addr = resolve(sym, name);
            if (sym_addr == 0) throw std::runtime_error("ExecMeta: resolve returned 0 address");

            RelocKind kind = (RelocKind)r.kind;
            switch (kind) {
            case RelocKind::Rel32_Call: {
                // at_code_off points to imm32 of E8 <imm32>
                if ((size_t)r.at_code_off + 4 > text_size) throw std::runtime_error("ExecMeta: Rel32_Call patch OOB");
                u64 rip_after = (u64)(patch_site + 4);
                long long target = (long long)sym_addr + (long long)r.addend;
                long long rel64 = target - (long long)rip_after;
                if (rel64 < (long long)INT32_MIN || rel64 >(long long)INT32_MAX)
                    throw std::runtime_error("ExecMeta: Rel32_Call out of range");
                write_i32(patch_site, (i32)rel64);
                st.rel32_calls++;
            } break;

            case RelocKind::Abs64_Imm: {
                if ((size_t)r.at_code_off + 8 > text_size) throw std::runtime_error("ExecMeta: Abs64_Imm patch OOB");
                u64 v = (u64)((long long)sym_addr + (long long)r.addend);
                write_u64(patch_site, v);
                st.abs64_imms++;
            } break;

            default:
                throw std::runtime_error("ExecMeta: unsupported reloc kind");
            }
        }

        return st;
    }

} // namespace rane::patcher

// ============================================================================
// End-to-end loader hook you call
// ============================================================================
namespace rane::loader {

    using namespace rane::execmeta;

    // Patch plan:
    // - Make .text RW while patching
    // - Build import table (+ optional thunks)
    // - Apply relocations
    // - Make .text RX
    // - Flush instruction cache for .text (and thunks if any)
    //
    // default_dlls is used only when an import symbol omits the DLL name.
    struct LoaderOptions {
        std::vector<std::string> default_dlls = { "kernel32.dll", "user32.dll", "ntdll.dll" };

        bool use_import_thunks = true;

        // If true, attempt to mark thunk entrypoints as CFG-valid call targets.
        // (Requires SetProcessValidCallTargets; best-effort if missing.)
        bool cfg_mark_thunks = true;
    };

    // Returns patch stats + built import/thunk tables in case you want diagnostics.
    struct LoaderResult {
        rane::patcher::PatchStats stats{};
        rane::imports::ImportTable imports{};
        size_t thunk_bytes_used = 0;
    };

    static LoaderResult patch_module_text_with_execmeta(
        const void* execmeta_blob,
        size_t execmeta_size,
        void* text_mem,
        size_t text_size,
        const LoaderOptions& opt
    ) {
        LoaderResult R{};

        auto em = rane::execmeta::parse_execmeta(execmeta_blob, execmeta_size);

        // 1) Make text RW (temporarily) for patching
        rane::winmem::protect(text_mem, text_size, PAGE_READWRITE);

        // 2) Prepare module context
        rane::patcher::ModuleContext M{};
        M.text_base = (u8*)text_mem;
        M.text_size = text_size;

        rane::patcher::index_procs(M, em);

        // 3) Optional thunk pool
        std::optional<rane::thunks::ThunkPool> pool;
        if (opt.use_import_thunks) {
            pool.emplace(opt.cfg_mark_thunks);

            // ThunkPool grow is disabled in this minimal version; pre-size once.
            // Ensure space for ~ (imports * 12) rounded up; we conservatively reserve 64KB.
            pool->ensure(64 * 1024);
        }

        // 4) Resolve imports (build name->addr and (optional) name->thunk)
        {
            rane::thunks::ThunkPool* pool_ptr = pool ? &*pool : nullptr;
            M.imports = rane::imports::build_import_table(em, opt.default_dlls, opt.use_import_thunks, pool_ptr);
            M.use_import_thunks = opt.use_import_thunks;

            // finalize thunks to RX now (so their addresses are executable)
            if (pool) {
                pool->finalize_to_rx();
                R.thunk_bytes_used = pool->used;
            }

            R.imports = M.imports;
        }

        // 5) Apply relocations
        auto resolver = rane::patcher::make_resolver(M, em);
        R.stats = rane::patcher::apply_relocs(em, (u8*)text_mem, text_size, resolver);

        // 6) Make .text RX and flush icache
        rane::winmem::protect(text_mem, text_size, PAGE_EXECUTE_READ);
        rane::winmem::flush_icache(text_mem, text_size);

        return R;
    }

} // namespace rane::loader

// ============================================================================
// Optional test main (remove in real build)
// ============================================================================
#ifdef RANE_LOADER_PATCHER_TEST
int main() {
    std::cout << "Provide real ExecMeta + .text buffers to test.\n";
    return 0;
}
#endif
