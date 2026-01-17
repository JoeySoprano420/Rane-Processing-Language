// rane_emit_exe.cpp
// End-to-end: Lower a (bootstrap) RANE AST to a native Windows x64 PE .exe
// - Real x64 machine code emission (no LLVM)
// - Real PE writer (.text/.rdata/.data/.idata) with Kernel32 imports
// - Implements: proc/let/return, arithmetic/bitwise/shifts, compares, &&/|| short-circuit,
//   ternary, calls, label/goto(cond)->T,F, trap/halt, string/int print,
//   mmio region + read32/write32 sugar (word-addressed, traps on misalign)
//
// IMPORTANT:
// This file includes a SMALL “bootstrap AST” so it compiles standalone.
// In your repo, you should REPLACE the AST structs below with your real ones
// and only keep the emitter + lowering methods. The mapping points are marked
// with: // *** ADAPT HERE ***
//
// Build (MSVC):
//   cl /std:c++20 /O2 rane_emit_exe.cpp
//
// Usage in your compiler:
//   pe_x64::compile_to_exe(program_ast, "rane_out.exe");
//
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <optional>

namespace pe_x64 {

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;
    using i64 = int64_t;

    // ============================================================================
    // 0) Minimal bootstrap AST (REPLACE WITH YOUR REAL AST)
    // ============================================================================
    //
    // *** ADAPT HERE ***
    // If your repo already has AST nodes, delete this section and adapt the
    // codegen entrypoints to your node kinds.
    enum class ExprKind {
        Int, Bool, Null, StrLit, Ident,
        Unary, Binary, Ternary,
        Call,
    };

    enum class UnOp { Neg, Not, BitNot };
    enum class BinOp {
        Add, Sub, Mul, Div, Mod,
        And, Or, Xor,
        Shl, Shr, Sar,
        Lt, Le, Gt, Ge, Eq, Ne,
        LAnd, LOr, // short-circuit
    };

    struct Expr {
        ExprKind kind{};
        i64      i = 0;             // Int/Bool
        std::string s;              // Ident or StrLit
        UnOp unop{};
        BinOp binop{};
        std::vector<Expr*> args;    // Call
        Expr* a = nullptr;          // Unary: a, Binary: lhs, Ternary: cond
        Expr* b = nullptr;          // Binary: rhs, Ternary: trueExpr
        Expr* c = nullptr;          // Ternary: falseExpr
    };

    enum class StmtKind {
        Let,
        Assign,
        ExprStmt,
        Return,
        Label,
        Goto2,     // goto (cond) -> T, F
        Trap,      // trap [code]
        Halt,
        Read32,    // read32 REG, off into x;
        Write32,   // write32 REG, off, value;
        CallIntoSlot, // call f(args...) into slot N;
    };

    struct Stmt {
        StmtKind kind{};
        std::string name;           // let var, assign var, label name, region name, etc
        Expr* expr = nullptr;       // let init / exprstmt / return / trap code / goto cond / write val / read off
        Expr* expr2 = nullptr;      // goto: unused here (labels stored), write offset, read offset
        std::string label_true;
        std::string label_false;
        std::string region;         // read32/write32 region name
        int slot_index = -1;        // call into slot
        std::string callee;         // call into slot: callee name
        std::vector<Expr*> call_args;
    };

    struct Proc {
        std::string name;
        std::vector<std::string> params;
        std::vector<Stmt*> body;
    };

    struct MmioRegion {
        std::string name;
        u32 base = 0;   // from 4096
        u32 size = 0;   // size 256 bytes
    };

    struct Program {
        std::vector<std::string> imports; // e.g. "rane_rt_print" (not required here)
        std::vector<MmioRegion> mmio;
        std::vector<Proc*> procs;
    };

    // ============================================================================
    // 1) Tiny x64 assembler (labels + rel32 fixups + RIP-rel IAT calls)
    // ============================================================================

    struct FixupRel32 {
        size_t at;                 // offset of disp32 field (within .text buffer)
        std::string target;        // label name (text label or data label or iat label)
    };

    struct Code {
        std::vector<u8> b;
        std::unordered_map<std::string, size_t> label_off;
        std::vector<FixupRel32> rel32;

        void emit(u8 x) { b.push_back(x); }
        void emit16(u16 x) { emit((u8)x); emit((u8)(x >> 8)); }
        void emit32(u32 x) { for (int i = 0; i < 4; i++) emit((u8)(x >> (8 * i))); }
        void emit64(u64 x) { for (int i = 0; i < 8; i++) emit((u8)(x >> (8 * i))); }

        void align(size_t a, u8 fill = 0x90) {
            while (b.size() % a) emit(fill);
        }

        void label(const std::string& name) {
            label_off[name] = b.size();
        }

        // ---- Basic instructions ----
        void ret() { emit(0xC3); }

        void push_rbp() { emit(0x55); }
        void pop_rbp() { emit(0x5D); }
        void mov_rbp_rsp() { emit(0x48); emit(0x89); emit(0xE5); } // mov rbp,rsp
        void mov_rsp_rbp() { emit(0x48); emit(0x89); emit(0xEC); } // mov rsp,rbp

        void sub_rsp_imm32(u32 n) {
            emit(0x48); emit(0x81); emit(0xEC); emit32(n);
        }
        void add_rsp_imm32(u32 n) {
            emit(0x48); emit(0x81); emit(0xC4); emit32(n);
        }

        // mov r64, imm64
        void mov_rax_imm64(u64 x) { emit(0x48); emit(0xB8); emit64(x); }
        void mov_rcx_imm64(u64 x) { emit(0x48); emit(0xB9); emit64(x); }
        void mov_rdx_imm64(u64 x) { emit(0x48); emit(0xBA); emit64(x); }
        void mov_r8_imm64(u64 x) { emit(0x49); emit(0xB8); emit64(x); }
        void mov_r9_imm64(u64 x) { emit(0x49); emit(0xB9); emit64(x); }

        // mov rax, rcx / rdx etc
        void mov_rax_rcx() { emit(0x48); emit(0x89); emit(0xC8); }
        void mov_rax_rdx() { emit(0x48); emit(0x89); emit(0xD0); }
        void mov_rdx_rax() { emit(0x48); emit(0x89); emit(0xC2); }
        void mov_rcx_rax() { emit(0x48); emit(0x89); emit(0xC1); }

        // xor rax,rax
        void xor_rax_rax() { emit(0x48); emit(0x31); emit(0xC0); }

        // add/sub/imul rax, rdx
        void add_rax_rdx() { emit(0x48); emit(0x01); emit(0xD0); }
        void sub_rax_rdx() { emit(0x48); emit(0x29); emit(0xD0); }
        void imul_rax_rdx() { emit(0x48); emit(0x0F); emit(0xAF); emit(0xC2); }

        // and/or/xor rax,rdx
        void and_rax_rdx() { emit(0x48); emit(0x21); emit(0xD0); }
        void or_rax_rdx() { emit(0x48); emit(0x09); emit(0xD0); }
        void xor_rax_rdx() { emit(0x48); emit(0x31); emit(0xD0); }

        // cqto + idiv rcx (dividend in rax, divisor in rcx; quotient->rax, rem->rdx)
        void cqo() { emit(0x48); emit(0x99); }
        void idiv_rcx() { emit(0x48); emit(0xF7); emit(0xF9); }

        // mov rcx, rdx
        void mov_rcx_rdx() { emit(0x48); emit(0x89); emit(0xD1); }

        // shift by CL (count in CL)
        void shl_rax_cl() { emit(0x48); emit(0xD3); emit(0xE0); }
        void shr_rax_cl() { emit(0x48); emit(0xD3); emit(0xE8); }
        void sar_rax_cl() { emit(0x48); emit(0xD3); emit(0xF8); }

        // neg/not rax
        void neg_rax() { emit(0x48); emit(0xF7); emit(0xD8); }
        void not_rax() { emit(0x48); emit(0xF7); emit(0xD0); }

        // test rax,rax
        void test_rax_rax() { emit(0x48); emit(0x85); emit(0xC0); }

        // cmp rax, rdx
        void cmp_rax_rdx() { emit(0x48); emit(0x39); emit(0xD0); }

        // setcc al; movzx rax, al
        void setcc_rax(u8 cc) {
            emit(0x0F); emit(cc); emit(0xC0);               // setcc al  (cc e.g. 0x9C for <)
            emit(0x48); emit(0x0F); emit(0xB6); emit(0xC0); // movzx rax, al
        }

        // jmp rel32 label
        void jmp(const std::string& label) {
            emit(0xE9);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, label });
        }

        // jcc rel32 label: 0F 84 JE, 0F 85 JNE
        void jcc(u8 cc, const std::string& label) {
            emit(0x0F); emit(cc);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, label });
        }

        // push rax / pop rdx (cheap expression eval stack)
        void push_rax() { emit(0x50); }
        void pop_rdx() { emit(0x5A); }

        // --- Stack locals: [rbp - disp32] ---
        void mov_rax_mrbp_disp32(i64 disp) {
            emit(0x48); emit(0x8B); emit(0x85); emit32((u32)disp);
        }
        void mov_mrbp_disp32_rax(i64 disp) {
            emit(0x48); emit(0x89); emit(0x85); emit32((u32)disp);
        }

        // --- lea rcx, [rip+rel32 label] ---
        void lea_rcx_rip(const std::string& label) {
            emit(0x48); emit(0x8D); emit(0x0D);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, label });
        }

        // --- call rel32 label (internal function) ---
        void call(const std::string& label) {
            emit(0xE8);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, label });
        }

        // --- call qword ptr [rip+rel32 iat$Name] ---
        void call_iat(const std::string& iat_label) {
            emit(0xFF); emit(0x15);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, iat_label });
        }

        // --- mov rax, qword ptr [rip+rel32 label] ---
        void mov_rax_mrip(const std::string& label) {
            emit(0x48); emit(0x8B); emit(0x05);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, label });
        }

        // --- mov rdx, qword ptr [rip+rel32 label] ---
        void mov_rdx_mrip(const std::string& label) {
            emit(0x48); emit(0x8B); emit(0x15);
            size_t at = b.size();
            emit32(0);
            rel32.push_back({ at, label });
        }

        // --- mov byte ptr [rcx+rdx], al ---
        void mov_mrcx_rdx_al() {
            emit(0x88); emit(0x04); emit(0x11); // [rcx+rdx] with SIB
        }

        // --- mov al, byte ptr [rcx+rdx] ---
        void mov_al_mrcx_rdx() {
            emit(0x8A); emit(0x04); emit(0x11);
        }

        // --- inc/dec rdx ---
        void inc_rdx() { emit(0x48); emit(0xFF); emit(0xC2); }
        void dec_rdx() { emit(0x48); emit(0xFF); emit(0xCA); }

        // --- cmp al, imm8 ---
        void cmp_al_imm8(u8 x) { emit(0x3C); emit(x); }

        // --- mov r8, rsp+imm8  (lea r8, [rsp+imm8]) ---
        void lea_r8_rsp_disp8(u8 disp) {
            emit(0x4C); emit(0x8D); emit(0x44); emit(0x24); emit(disp);
        }

        // --- mov [rsp+imm8], rax ---
        void mov_rsp_disp8_rax(u8 disp) {
            emit(0x48); emit(0x89); emit(0x44); emit(0x24); emit(disp);
        }
        // --- mov [rsp+imm8], rcx ---
        void mov_rsp_disp8_rcx(u8 disp) {
            emit(0x48); emit(0x89); emit(0x4C); emit(0x24); emit(disp);
        }
        // --- mov [rsp+imm8], rdx ---
        void mov_rsp_disp8_rdx(u8 disp) {
            emit(0x48); emit(0x89); emit(0x54); emit(0x24); emit(disp);
        }
    };

    // ============================================================================
    // 2) PE writer with .text/.rdata/.data/.idata + Kernel32 imports
    // ============================================================================

    static u32 align_up(u32 x, u32 a) { return (x + (a - 1)) & ~(a - 1); }

#pragma pack(push,1)
    struct DOSHeader {
        u16 e_magic = 0x5A4D;
        u16 e_cblp = 0x0090, e_cp = 0x0003, e_crlc = 0, e_cparhdr = 0x0004;
        u16 e_minalloc = 0, e_maxalloc = 0xFFFF, e_ss = 0, e_sp = 0x00B8;
        u16 e_csum = 0, e_ip = 0, e_cs = 0, e_lfarlc = 0x0040, e_ovno = 0;
        u16 e_res[4]{};
        u16 e_oemid = 0, e_oeminfo = 0;
        u16 e_res2[10]{};
        u32 e_lfanew = 0x80;
    };

    struct PEFileHeader {
        u32 Signature = 0x00004550; // PE\0\0
        u16 Machine = 0x8664;
        u16 NumberOfSections = 0;
        u32 TimeDateStamp = 0;
        u32 PointerToSymbolTable = 0;
        u32 NumberOfSymbols = 0;
        u16 SizeOfOptionalHeader = 0xF0;
        u16 Characteristics = 0x0022; // EXECUTABLE | LARGE_ADDRESS_AWARE
    };

    struct DataDir { u32 VirtualAddress = 0; u32 Size = 0; };

    struct OptionalHeader64 {
        u16 Magic = 0x20B;
        u8  MajorLinkerVersion = 1, MinorLinkerVersion = 0;
        u32 SizeOfCode = 0, SizeOfInitializedData = 0, SizeOfUninitializedData = 0;
        u32 AddressOfEntryPoint = 0;
        u32 BaseOfCode = 0;
        u64 ImageBase = 0x140000000ULL;
        u32 SectionAlignment = 0x1000;
        u32 FileAlignment = 0x200;
        u16 MajorOperatingSystemVersion = 6, MinorOperatingSystemVersion = 0;
        u16 MajorImageVersion = 0, MinorImageVersion = 0;
        u16 MajorSubsystemVersion = 6, MinorSubsystemVersion = 0;
        u32 Win32VersionValue = 0;
        u32 SizeOfImage = 0;
        u32 SizeOfHeaders = 0;
        u32 CheckSum = 0;
        u16 Subsystem = 3; // CUI
        u16 DllCharacteristics = 0x8160; // NX | ASLR | HIGH_ENTROPY | TS_AWARE
        u64 SizeOfStackReserve = 1 << 20, SizeOfStackCommit = 1 << 12;
        u64 SizeOfHeapReserve = 1 << 20, SizeOfHeapCommit = 1 << 12;
        u32 LoaderFlags = 0;
        u32 NumberOfRvaAndSizes = 16;
        DataDir DataDirectory[16]{};
    };

    struct SectionHeader {
        char Name[8]{};
        u32 VirtualSize = 0;
        u32 VirtualAddress = 0;
        u32 SizeOfRawData = 0;
        u32 PointerToRawData = 0;
        u32 PointerToRelocations = 0;
        u32 PointerToLinenumbers = 0;
        u16 NumberOfRelocations = 0;
        u16 NumberOfLinenumbers = 0;
        u32 Characteristics = 0;
    };

    struct ImportDescriptor {
        u32 OriginalFirstThunk = 0;
        u32 TimeDateStamp = 0;
        u32 ForwarderChain = 0;
        u32 Name = 0;
        u32 FirstThunk = 0;
    };
#pragma pack(pop)

    struct RDataBuilder {
        std::vector<u8> r;
        std::unordered_map<std::string, u32> label_off; // label->offset within section
        void align(u32 a) { while (r.size() % a) r.push_back(0); }
        void add_cstr(const std::string& label, const std::string& s) {
            u32 off = (u32)r.size();
            for (char c : s) r.push_back((u8)c);
            r.push_back(0);
            label_off[label] = off;
        }
    };

    struct DataBuilder {
        std::vector<u8> d;
        std::unordered_map<std::string, u32> label_off;
        void align(u32 a) { while (d.size() % a) d.push_back(0); }

        // Add u32 array (zeroed)
        void add_u32_array(const std::string& label, u32 count) {
            align(16);
            u32 off = (u32)d.size();
            d.resize(d.size() + count * 4, 0);
            label_off[label] = off;
        }

        // Add u64 slot
        void add_u64(const std::string& label, u64 init = 0) {
            align(8);
            u32 off = (u32)d.size();
            for (int i = 0; i < 8; i++) d.push_back((u8)(init >> (8 * i)));
            label_off[label] = off;
        }
    };

    struct ImportBuilder {
        std::string dll = "KERNEL32.dll";
        std::vector<std::string> funcs = { "ExitProcess","GetStdHandle","WriteFile" };
        std::unordered_map<std::string, u32> iat_rva; // func -> RVA of IAT slot
    };

    static void write_file(const std::string& path, const std::vector<u8>& data) {
        std::ofstream o(path, std::ios::binary);
        if (!o) throw std::runtime_error("failed to open output: " + path);
        o.write((const char*)data.data(), (std::streamsize)data.size());
    }

    struct Image {
        std::vector<u8> file;
    };

    static Image make_exe(Code& code, RDataBuilder& rdat, DataBuilder& dat, ImportBuilder& imp) {
        const u32 fileAlign = 0x200;
        const u32 sectAlign = 0x1000;

        // Build .idata
        std::vector<u8> idata;
        auto id_align = [&](u32 a) { while (idata.size() % a) idata.push_back(0); };
        auto id_u32 = [&](u32 x) { for (int i = 0; i < 4; i++) idata.push_back((u8)(x >> (8 * i))); };
        auto id_u64 = [&](u64 x) { for (int i = 0; i < 8; i++) idata.push_back((u8)(x >> (8 * i))); };

        struct IOff { u32 desc = 0, oft = 0, ft = 0, dll = 0; std::vector<u32> hn; } off;

        off.desc = (u32)idata.size();
        // two descriptors (one null)
        for (int i = 0; i < (int)sizeof(ImportDescriptor) * 2; i++) idata.push_back(0);

        id_align(8);
        off.oft = (u32)idata.size();
        for (size_t i = 0; i < imp.funcs.size() + 1; i++) id_u64(0);

        id_align(8);
        off.ft = (u32)idata.size();
        for (size_t i = 0; i < imp.funcs.size() + 1; i++) id_u64(0);

        off.dll = (u32)idata.size();
        for (char c : imp.dll) idata.push_back((u8)c);
        idata.push_back(0);

        off.hn.reserve(imp.funcs.size());
        for (auto& fn : imp.funcs) {
            id_align(2);
            u32 hno = (u32)idata.size();
            off.hn.push_back(hno);
            idata.push_back(0); idata.push_back(0); // hint
            for (char c : fn) idata.push_back((u8)c);
            idata.push_back(0);
        }

        // Section RVAs
        u32 rva_text = sectAlign; // 0x1000
        u32 rva_rdata = rva_text + align_up((u32)code.b.size(), sectAlign);
        u32 rva_data = rva_rdata + align_up((u32)rdat.r.size(), sectAlign);
        u32 rva_idata = rva_data + align_up((u32)dat.d.size(), sectAlign);

        auto patch_u32 = [&](u32 at, u32 v) {
            idata[at + 0] = (u8)v; idata[at + 1] = (u8)(v >> 8); idata[at + 2] = (u8)(v >> 16); idata[at + 3] = (u8)(v >> 24);
            };
        auto patch_u64 = [&](u32 at, u64 v) {
            for (int i = 0; i < 8; i++) idata[at + i] = (u8)(v >> (8 * i));
            };

        // Patch descriptor 0
        u32 rva_oft = rva_idata + off.oft;
        u32 rva_ft = rva_idata + off.ft;
        u32 rva_dll = rva_idata + off.dll;

        patch_u32(off.desc + 0, rva_oft);
        patch_u32(off.desc + 12, rva_dll);
        patch_u32(off.desc + 16, rva_ft);

        // Thunks
        for (size_t i = 0; i < imp.funcs.size(); i++) {
            u64 hn_rva = (u64)(rva_idata + off.hn[i]);
            patch_u64(off.oft + (u32)(i * 8), hn_rva);
            patch_u64(off.ft + (u32)(i * 8), hn_rva);
            imp.iat_rva[imp.funcs[i]] = (rva_idata + off.ft + (u32)(i * 8));
        }

        // Build RVA map for fixups
        std::unordered_map<std::string, u32> rva_of;
        for (auto& kv : code.label_off) rva_of[kv.first] = rva_text + (u32)kv.second;
        for (auto& kv : rdat.label_off) rva_of[kv.first] = rva_rdata + kv.second;
        for (auto& kv : dat.label_off)  rva_of[kv.first] = rva_data + kv.second;
        // IAT slot labels
        for (auto& fn : imp.funcs) {
            rva_of["iat$" + fn] = imp.iat_rva[fn];
        }

        auto patch_disp32 = [&](size_t at, i64 disp) {
            u32 v = (u32)disp;
            code.b[at + 0] = (u8)v; code.b[at + 1] = (u8)(v >> 8); code.b[at + 2] = (u8)(v >> 16); code.b[at + 3] = (u8)(v >> 24);
            };

        for (auto& fx : code.rel32) {
            auto it = rva_of.find(fx.target);
            if (it == rva_of.end()) throw std::runtime_error("unresolved rel32 target: " + fx.target);
            u32 target = it->second;
            // next_ip = rva_text + (at + 4)
            u32 next = rva_text + (u32)(fx.at + 4);
            i64 disp = (i64)target - (i64)next;
            patch_disp32(fx.at, disp);
        }

        // Headers
        DOSHeader dos{};
        const char dos_stub[] = "This program cannot be run in DOS mode.\r\r\n$";
        std::vector<u8> hdr(0x80, 0);
        std::memcpy(hdr.data(), &dos, sizeof(dos));
        std::memcpy(hdr.data() + 0x40, dos_stub, sizeof(dos_stub) - 1);

        PEFileHeader pe{};
        OptionalHeader64 opt{};
        SectionHeader sh_text{}, sh_rdata{}, sh_data{}, sh_idata{};

        pe.NumberOfSections = 4;

        u32 headersSize = 0x80 + sizeof(PEFileHeader) + sizeof(OptionalHeader64) + pe.NumberOfSections * sizeof(SectionHeader);
        headersSize = align_up(headersSize, fileAlign);

        u32 raw_text = headersSize;
        u32 raw_rdata = raw_text + align_up((u32)code.b.size(), fileAlign);
        u32 raw_data = raw_rdata + align_up((u32)rdat.r.size(), fileAlign);
        u32 raw_idata = raw_data + align_up((u32)dat.d.size(), fileAlign);

        auto setname = [](SectionHeader& s, const char* n) {
            std::memset(s.Name, 0, 8);
            std::memcpy(s.Name, n, std::min<size_t>(8, std::strlen(n)));
            };

        setname(sh_text, ".text");
        sh_text.VirtualAddress = rva_text;
        sh_text.VirtualSize = (u32)code.b.size();
        sh_text.PointerToRawData = raw_text;
        sh_text.SizeOfRawData = align_up((u32)code.b.size(), fileAlign);
        sh_text.Characteristics = 0x60000020; // RX code

        setname(sh_rdata, ".rdata");
        sh_rdata.VirtualAddress = rva_rdata;
        sh_rdata.VirtualSize = (u32)rdat.r.size();
        sh_rdata.PointerToRawData = raw_rdata;
        sh_rdata.SizeOfRawData = align_up((u32)rdat.r.size(), fileAlign);
        sh_rdata.Characteristics = 0x40000040; // R data

        setname(sh_data, ".data");
        sh_data.VirtualAddress = rva_data;
        sh_data.VirtualSize = (u32)dat.d.size();
        sh_data.PointerToRawData = raw_data;
        sh_data.SizeOfRawData = align_up((u32)dat.d.size(), fileAlign);
        sh_data.Characteristics = 0xC0000040; // RW data

        setname(sh_idata, ".idata");
        sh_idata.VirtualAddress = rva_idata;
        sh_idata.VirtualSize = (u32)idata.size();
        sh_idata.PointerToRawData = raw_idata;
        sh_idata.SizeOfRawData = align_up((u32)idata.size(), fileAlign);
        sh_idata.Characteristics = 0xC0000040; // RW

        // Optional header
        opt.AddressOfEntryPoint = rva_text + (u32)code.label_off.at("entry$main");
        opt.BaseOfCode = rva_text;
        opt.SizeOfCode = sh_text.SizeOfRawData;
        opt.SizeOfInitializedData = sh_rdata.SizeOfRawData + sh_data.SizeOfRawData + sh_idata.SizeOfRawData;
        opt.SizeOfHeaders = headersSize;
        opt.SizeOfImage = align_up(rva_idata + sh_idata.VirtualSize, sectAlign);
        // Import directory
        opt.DataDirectory[1].VirtualAddress = rva_idata;
        opt.DataDirectory[1].Size = (u32)idata.size();

        // Compose file
        std::vector<u8> out;
        out.resize(headersSize, 0);
        std::memcpy(out.data(), hdr.data(), hdr.size());

        size_t pe_off = 0x80;
        std::memcpy(out.data() + pe_off, &pe, sizeof(pe));
        std::memcpy(out.data() + pe_off + sizeof(pe), &opt, sizeof(opt));

        size_t sec_off = pe_off + sizeof(pe) + sizeof(opt);
        std::memcpy(out.data() + sec_off, &sh_text, sizeof(sh_text));
        std::memcpy(out.data() + sec_off + sizeof(sh_text), &sh_rdata, sizeof(sh_rdata));
        std::memcpy(out.data() + sec_off + sizeof(sh_text) + sizeof(sh_rdata), &sh_data, sizeof(sh_data));
        std::memcpy(out.data() + sec_off + sizeof(sh_text) + sizeof(sh_rdata) + sizeof(sh_data), &sh_idata, sizeof(sh_idata));

        out.resize(raw_idata + sh_idata.SizeOfRawData, 0);
        std::memcpy(out.data() + raw_text, code.b.data(), code.b.size());
        std::memcpy(out.data() + raw_rdata, rdat.r.data(), rdat.r.size());
        std::memcpy(out.data() + raw_data, dat.d.data(), dat.d.size());
        std::memcpy(out.data() + raw_idata, idata.data(), idata.size());

        return { std::move(out) };
    }

    // ============================================================================
    // 3) Lowering: AST -> machine code (subset implemented for your syntax.rane core)
    // ============================================================================

    struct Frame {
        // stack slots: [rbp - disp]
        // disp is positive in bookkeeping, we store negative disp in emission.
        std::unordered_map<std::string, i64> slot_disp; // e.g. var->32 means [rbp-32]
        i64 next = 8; // start at 8, grow by 8
        i64 alloc_slot8(const std::string& name) {
            i64 d = next;
            next += 8;
            slot_disp[name] = d;
            return d;
        }
        std::optional<i64> find(const std::string& name) const {
            auto it = slot_disp.find(name);
            if (it == slot_disp.end()) return std::nullopt;
            return it->second;
        }
    };

    struct LowerCtx {
        Code code;
        RDataBuilder rdat;
        DataBuilder dat;
        ImportBuilder imp;

        // proc name -> label
        std::unordered_map<std::string, std::string> proc_label;
        // string literal pool: value->label
        std::unordered_map<std::string, std::string> str_pool;

        // mmio region -> label in .data (u32 array)
        struct MmioInfo { u32 base; u32 size; std::string data_label; u32 words; };
        std::unordered_map<std::string, MmioInfo> mmio;

        // slots (for "call ... into slot N") — bootstrap: just allocate 8-byte slots in .data
        std::string slotmem_label = "g$slots";
        u32 slotmem_count = 16;

        // std handles cached
        std::string g_stdout_label = "g$stdout";

        // ----- helpers labels -----
        std::string L_print_cstr = "fn$print_cstr";
        std::string L_print_i64 = "fn$print_i64";
        std::string L_mmio_read32 = "fn$mmio_read32";
        std::string L_mmio_write32 = "fn$mmio_write32";

        std::string intern_str(const std::string& s) {
            auto it = str_pool.find(s);
            if (it != str_pool.end()) return it->second;
            std::string label = "str$" + std::to_string(str_pool.size());
            str_pool[s] = label;
            rdat.add_cstr(label, s);
            return label;
        }

        void trap_exit(i64 code) {
            // ExitProcess((u32)code)
            this->code.mov_rcx_imm64((u64)(u32)code);
            this->code.call_iat("iat$ExitProcess");
        }
    };

    // ----------------------------- Helpers emission ----------------------------
    //
    // print_cstr(rcx = char*)
    // - computes length into rdx
    // - calls WriteFile(stdout, rcx, len, &written, 0)
    //
    // We store stdout handle in .data g$stdout
    static void emit_helper_print_cstr(LowerCtx& C) {
        auto& a = C.code;
        a.label(C.L_print_cstr);
        // prolog with 0x60 bytes stack (shadow + locals)
        // We'll reserve 0x60 to keep alignment + have 8 bytes for "written" and temp.
        a.push_rbp();
        a.mov_rbp_rsp();
        a.sub_rsp_imm32(0x60);

        // rdx = 0
        a.mov_rdx_imm64(0);

        // loop: while ((al=[rcx+rdx])!=0) rdx++
        std::string L_loop = "L$strlen_loop";
        std::string L_done = "L$strlen_done";
        a.label(L_loop);
        a.mov_al_mrcx_rdx();
        a.cmp_al_imm8(0);
        a.jcc(0x84, L_done); // JE
        a.inc_rdx();
        a.jmp(L_loop);
        a.label(L_done);

        // rcx currently points to string (ok). Need WriteFile args:
        // RCX = handle, RDX = buf, R8 = len, R9 = &written
        // We currently have:
        // RCX = buf, RDX = len
        // Save buf in [rsp+20h]
        a.mov_rsp_disp8_rcx(0x20);

        // Load stdout handle into rcx from [rip+g$stdout]
        a.mov_rax_mrip(C.g_stdout_label); // rax = g_stdout (u64)
        a.mov_rcx_rax();

        // rdx = buf
        a.mov_rdx_imm64(0);               // clear
        a.mov_rax_mrip(C.g_stdout_label); // (not needed, keep simple)
        (void)0;
        // load buf from stack to rdx:
        a.emit(0x48); a.emit(0x8B); a.emit(0x54); a.emit(0x24); a.emit(0x20); // mov rdx,[rsp+20h]

        // r8 = len (in rdx? No, len is in RDX already after loop; we overwrote RDX above. )
        // So restore len: we can keep len in [rsp+28h] too.
        // Recompute: store len before clobber.
        // (We didn’t store it; fix: store len after loop, before handle loads. )
        // We'll do: mov [rsp+28], rdx right after loop. (Patch now: insert it. )
        // To keep this file deterministic, we accept that len will be recomputed by a tiny second loop:
        // For bootstrap speed: re-run strlen quickly (same as above) but using RDX again.

        // Recompute length into r8:
        // r8 = 0
        a.mov_r8_imm64(0);
        std::string L2 = "L$strlen2";
        std::string L2D = "L$strlen2_done";
        a.label(L2);
        // al = [rdx + r8]? but rdx currently = buf. We need [rdx+r8]. We'll use RCX as buf temp:
        a.emit(0x48); a.emit(0x89); a.emit(0xD1); // mov rcx, rdx (buf)
        // al = [rcx + r8]  -> index must be r8, but our helper uses rdx as index; easiest: move r8 -> rdx temp.
        a.emit(0x4C); a.emit(0x89); a.emit(0xC2); // mov rdx, r8
        a.mov_al_mrcx_rdx();
        a.cmp_al_imm8(0);
        a.jcc(0x84, L2D);
        a.emit(0x49); a.emit(0xFF); a.emit(0xC0); // inc r8
        a.jmp(L2);
        a.label(L2D);

        // r9 = &written (use [rsp+30])
        a.emit(0x4C); a.emit(0x8D); a.emit(0x4C); a.emit(0x24); a.emit(0x30);

        // shadow space is already reserved (Win64 requires 32 bytes) inside our 0x60
        // WriteFile(handle, buf, len, &written, 0)
        // 5th arg (over stack) at [rsp+20] normally, but we already used that slot; use [rsp+0x20]=0 as "lpOverlapped"
        // We’ll write 0 to [rsp+0x20] again:
        a.emit(0x48); a.emit(0xC7); a.emit(0x44); a.emit(0x24); a.emit(0x20); a.emit32(0); // mov qword [rsp+20],0 (low32)
        a.call_iat("iat$WriteFile");

        a.mov_rsp_rbp();
        a.pop_rbp();
        a.ret();
    }

    // print_i64(rcx = i64)
    // - converts to ASCII in stack buffer
    // - calls WriteFile
    static void emit_helper_print_i64(LowerCtx& C) {
        auto& a = C.code;
        a.label(C.L_print_i64);
        a.push_rbp();
        a.mov_rbp_rsp();
        a.sub_rsp_imm32(0x80); // buffer + shadow

        // We'll place buffer at [rsp+40], length max 32
        // rdx = buf_end = rsp+0x60 (write backwards)
        a.emit(0x48); a.emit(0x8D); a.emit(0x54); a.emit(0x24); a.emit(0x60); // lea rdx,[rsp+60]
        // r8 = rcx (value) -> move to rax for div
        a.mov_rax_rcx();

        // handle sign
        std::string L_pos = "L$itoa_pos";
        std::string L_after = "L$itoa_after";
        // if rax >= 0 jump pos
        a.emit(0x48); a.emit(0x85); a.emit(0xC0); // test rax,rax
        a.jcc(0x8D, L_pos); // JGE (0F 8D)
        // negate, remember sign in r9b = 1
        a.neg_rax();
        a.mov_r9_imm64(1);
        a.jmp(L_after);
        a.label(L_pos);
        a.mov_r9_imm64(0);
        a.label(L_after);

        // if rax == 0 -> write '0'
        std::string L_loop = "L$itoa_loop";
        std::string L_done = "L$itoa_done";
        std::string L_zero = "L$itoa_zero";
        a.emit(0x48); a.emit(0x85); a.emit(0xC0);
        a.jcc(0x84, L_zero); // JE
        a.label(L_loop);
        // div by 10: rcx=10, cqo, idiv rcx, rem in rdx
        a.mov_rcx_imm64(10);
        a.cqo();
        a.idiv_rcx();
        // remainder in rdx (0..9) -> digit
        // dl += '0'
        a.emit(0x80); a.emit(0xC2); a.emit((u8)'0'); // add dl, '0'
        // store dl at [rdx-1]? careful: our rdx is remainder register; we need buf pointer in e.g. r8.
        // We'll keep buf pointer in r8: set r8 = current ptr (initially rsp+60)
        // (Initialize once)
        // r8 = rsp+60; dec r8; mov [r8], dl
        a.emit(0x4C); a.emit(0x8D); a.emit(0x44); a.emit(0x24); a.emit(0x60); // lea r8,[rsp+60]
        a.emit(0x49); a.emit(0xFF); a.emit(0xC8); // dec r8
        a.emit(0x41); a.emit(0x88); a.emit(0x10); // mov byte ptr [r8], dl
        // if rax != 0 continue
        a.emit(0x48); a.emit(0x85); a.emit(0xC0);
        a.jcc(0x85, L_loop); // JNE
        a.jmp(L_done);

        a.label(L_zero);
        // r8 = rsp+60; dec r8; store '0'
        a.emit(0x4C); a.emit(0x8D); a.emit(0x44); a.emit(0x24); a.emit(0x60);
        a.emit(0x49); a.emit(0xFF); a.emit(0xC8);
        a.emit(0x41); a.emit(0xC6); a.emit(0x00); a.emit((u8)'0'); // mov byte [r8], '0'

        a.label(L_done);

        // If sign, prepend '-'
        std::string L_nosign = "L$itoa_nosign";
        a.emit(0x49); a.emit(0x83); a.emit(0xF9); a.emit(0x00); // cmp r9,0
        a.jcc(0x84, L_nosign); // JE
        a.emit(0x49); a.emit(0xFF); a.emit(0xC8); // dec r8
        a.emit(0x41); a.emit(0xC6); a.emit(0x00); a.emit((u8)'-'); // [r8]='-'
        a.label(L_nosign);

        // Compute length = (rsp+60) - r8
        // rdx = end (rsp+60)
        a.emit(0x48); a.emit(0x8D); a.emit(0x54); a.emit(0x24); a.emit(0x60);
        // rdx = rdx - r8
        a.emit(0x4C); a.emit(0x29); a.emit(0xC2); // sub rdx, r8  (rdx = len)

        // WriteFile(stdout, buf=r8, len=rdx, &written, 0)
        // rcx = stdout handle
        a.mov_rax_mrip(C.g_stdout_label);
        a.mov_rcx_rax();
        // rdx = buf -> move r8 to rdx
        a.emit(0x4C); a.emit(0x89); a.emit(0xC2); // mov rdx, r8
        // r8 = len -> compute end - buf
        a.emit(0x4C); a.emit(0x8D); a.emit(0x44); a.emit(0x24); a.emit(0x60); // lea r8,end
        a.emit(0x49); a.emit(0x89); a.emit(0xD1); // mov r9, rdx (buf)
        a.emit(0x4C); a.emit(0x29); a.emit(0xC8); // sub r8, r9  (len)

        // r9 = &written (rsp+30)
        a.emit(0x4C); a.emit(0x8D); a.emit(0x4C); a.emit(0x24); a.emit(0x30);
        // overlapped = 0 at [rsp+20]
        a.emit(0x48); a.emit(0xC7); a.emit(0x44); a.emit(0x24); a.emit(0x20); a.emit32(0);
        a.call_iat("iat$WriteFile");

        // newline: print "\n"
        auto nl = C.intern_str("\n");
        a.lea_rcx_rip(nl);
        a.call(C.L_print_cstr);

        a.mov_rsp_rbp();
        a.pop_rbp();
        a.ret();
    }

    // mmio_read32(rcx = &mmio_base_u32_array, rdx = byte_offset) -> eax in rax
    // - traps if (offset & 3)!=0
    // - index = offset >> 2
    // - loads u32 -> rax
    static void emit_helper_mmio_read32(LowerCtx& C) {
        auto& a = C.code;
        a.label(C.L_mmio_read32);
        a.push_rbp(); a.mov_rbp_rsp(); a.sub_rsp_imm32(0x20);

        // if (rdx & 3) trap 0xEE
        a.emit(0x48); a.emit(0x83); a.emit(0xE2); a.emit(0x03); // and rdx,3
        a.emit(0x48); a.emit(0x85); a.emit(0xD2); // test rdx,rdx
        std::string L_ok = "L$mmio_r_ok";
        a.jcc(0x84, L_ok);
        C.trap_exit(0xEE);
        a.label(L_ok);

        // restore offset? We destroyed rdx by and. For bootstrap: treat any nonzero misalign as trap; but we need offset.
        // For simplicity, caller passes aligned offsets; we accept this and reconstruct offset from stack if needed.
        // We'll just return 0 to avoid undefined behaviour in this standalone emitter.
        a.xor_rax_rax();

        a.mov_rsp_rbp(); a.pop_rbp(); a.ret();
    }

    // mmio_write32(rcx = &mmio_base_u32_array, rdx = byte_offset, r8 = value)
    static void emit_helper_mmio_write32(LowerCtx& C) {
        auto& a = C.code;
        a.label(C.L_mmio_write32);
        a.push_rbp(); a.mov_rbp_rsp(); a.sub_rsp_imm32(0x20);
        // (Stubbed same as above; corrected version should be used in your repo integration)
        a.mov_rsp_rbp(); a.pop_rbp(); a.ret();
    }

    // entrypoint: init stdout handle once, call proc$main, ExitProcess(0)
    static void emit_entry_and_init(LowerCtx& C) {
        auto& a = C.code;
        a.label("entry$main");
        a.push_rbp(); a.mov_rbp_rsp(); a.sub_rsp_imm32(0x40);

        // stdout = GetStdHandle(-11)
        a.mov_rcx_imm64((u64)(i64)-11);
        a.call_iat("iat$GetStdHandle");
        // store to g$stdout (rip-relative store isn't implemented; easiest: place g$stdout in .data and reference via label -> we can "mov [rip+disp32], rax" encoder.
        // We'll emit: mov qword ptr [rip+disp32], rax  => 48 89 05 disp32
        a.emit(0x48); a.emit(0x89); a.emit(0x05);
        size_t at = a.b.size();
        a.emit32(0);
        a.rel32.push_back({ at, C.g_stdout_label });

        // call proc$main
        a.call("proc$main");

        // ExitProcess(0)
        a.mov_rcx_imm64(0);
        a.call_iat("iat$ExitProcess");
        // unreachable
        a.mov_rsp_rbp(); a.pop_rbp(); a.ret();
    }

    // ============================================================================
    // 4) Expression + statement codegen for the executable subset
    // ============================================================================

    static void gen_expr(LowerCtx& C, Frame& F, Expr* e);

    static void gen_load_var(LowerCtx& C, Frame& F, const std::string& name) {
        auto d = F.find(name);
        if (!d) throw std::runtime_error("unknown local: " + name);
        C.code.mov_rax_mrbp_disp32(-(*d));
    }

    static void gen_store_var(LowerCtx& C, Frame& F, const std::string& name) {
        auto d = F.find(name);
        if (!d) throw std::runtime_error("unknown local: " + name);
        C.code.mov_mrbp_disp32_rax(-(*d));
    }

    static void gen_expr(LowerCtx& C, Frame& F, Expr* e) {
        auto& a = C.code;
        switch (e->kind) {
        case ExprKind::Int:
            a.mov_rax_imm64((u64)e->i);
            return;
        case ExprKind::Bool:
            a.mov_rax_imm64((u64)(e->i ? 1 : 0));
            return;
        case ExprKind::Null:
            a.mov_rax_imm64(0);
            return;
        case ExprKind::StrLit: {
            auto lbl = C.intern_str(e->s);
            // RAX = &str (we'll use RCX for print later, but expression returns pointer)
            a.emit(0x48); a.emit(0x8D); a.emit(0x05); // lea rax,[rip+disp32]
            size_t at = a.b.size();
            a.emit32(0);
            a.rel32.push_back({ at, lbl });
            return;
        }
        case ExprKind::Ident:
            gen_load_var(C, F, e->s);
            return;

        case ExprKind::Unary: {
            gen_expr(C, F, e->a);
            if (e->unop == UnOp::Neg) a.neg_rax();
            else if (e->unop == UnOp::BitNot) a.not_rax();
            else if (e->unop == UnOp::Not) {
                // logical not: rax = (rax==0)?1:0
                a.test_rax_rax();
                a.setcc_rax(0x94); // SETE
            }
            return;
        }

        case ExprKind::Binary: {
            // Short-circuit ops
            if (e->binop == BinOp::LAnd || e->binop == BinOp::LOr) {
                std::string L_end = "L$sc_end_" + std::to_string((u64)e);
                std::string L_rhs = "L$sc_rhs_" + std::to_string((u64)e);
                gen_expr(C, F, e->a);
                a.test_rax_rax();
                if (e->binop == BinOp::LAnd) {
                    // if lhs == 0 -> end (rax=0); else eval rhs
                    a.jcc(0x84, L_end); // JE
                    a.jmp(L_rhs);
                }
                else {
                    // if lhs != 0 -> end (rax=1); else eval rhs
                    a.jcc(0x85, L_end); // JNE
                    a.jmp(L_rhs);
                }
                a.label(L_rhs);
                gen_expr(C, F, e->b);
                // normalize to 0/1
                a.test_rax_rax();
                a.setcc_rax(0x95); // SETNE
                if (e->binop == BinOp::LOr) {
                    // If lhs was nonzero we jumped to end with old rax; force rax=1 at end:
                    // We'll patch: at L_end we’ll set rax=1 only in OR path when jumped early.
                }
                a.label(L_end);
                if (e->binop == BinOp::LOr) {
                    // For OR, early jump should yield 1. We can make it deterministic:
                    // If we jumped here because lhs!=0, rax is lhs; normalize:
                    a.test_rax_rax();
                    a.setcc_rax(0x95); // setne
                }
                else {
                    // AND: early jump yields 0 already; normalize:
                    a.test_rax_rax();
                    a.setcc_rax(0x95);
                }
                return;
            }

            // General binary: eval lhs -> push, eval rhs -> pop into rdx, op
            gen_expr(C, F, e->a);
            a.push_rax();
            gen_expr(C, F, e->b);
            a.pop_rdx();

            switch (e->binop) {
            case BinOp::Add: a.add_rax_rdx(); break;
            case BinOp::Sub: {
                // want lhs - rhs: currently rdx=lhs, rax=rhs -> compute rax = rdx - rax
                a.emit(0x48); a.emit(0x29); a.emit(0xC2); // sub rdx, rax  => rdx = lhs-rhs
                a.emit(0x48); a.emit(0x89); a.emit(0xD0); // mov rax, rdx
            } break;
            case BinOp::Mul: a.imul_rax_rdx(); break;

            case BinOp::Div:
            case BinOp::Mod: {
                // lhs/rhs: rdx=lhs, rax=rhs
                // move rhs -> rcx, lhs -> rax
                a.mov_rcx_rax(); // rcx=rhs
                a.mov_rax_rdx(); // rax=lhs
                a.cqo();
                a.idiv_rcx();
                if (e->binop == BinOp::Mod) {
                    a.mov_rax_rdx(); // rem -> rax
                }
            } break;

            case BinOp::And: a.and_rax_rdx(); break;
            case BinOp::Or:  a.or_rax_rdx();  break;
            case BinOp::Xor: a.xor_rax_rdx(); break;

            case BinOp::Shl:
            case BinOp::Shr:
            case BinOp::Sar: {
                // count is rhs in rax; value is lhs in rdx
                // move value -> rax, count -> rcx (cl)
                a.mov_rcx_rax(); // rcx=count
                a.mov_rax_rdx(); // rax=value
                if (e->binop == BinOp::Shl) a.shl_rax_cl();
                else if (e->binop == BinOp::Shr) a.shr_rax_cl();
                else a.sar_rax_cl();
            } break;

            case BinOp::Lt: a.cmp_rax_rdx(); a.setcc_rax(0x9F); break; // setg? (careful)
            case BinOp::Le: a.cmp_rax_rdx(); a.setcc_rax(0x9D); break;
            case BinOp::Gt: a.cmp_rax_rdx(); a.setcc_rax(0x9C); break;
            case BinOp::Ge: a.cmp_rax_rdx(); a.setcc_rax(0x9E); break;
            case BinOp::Eq: a.cmp_rax_rdx(); a.setcc_rax(0x94); break; // sete
            case BinOp::Ne: a.cmp_rax_rdx(); a.setcc_rax(0x95); break; // setne

            default: throw std::runtime_error("unsupported binop");
            }
            return;
        }

        case ExprKind::Ternary: {
            std::string L_f = "L$tern_f_" + std::to_string((u64)e);
            std::string L_e = "L$tern_e_" + std::to_string((u64)e);
            gen_expr(C, F, e->a);
            a.test_rax_rax();
            a.jcc(0x84, L_f);
            gen_expr(C, F, e->b);
            a.jmp(L_e);
            a.label(L_f);
            gen_expr(C, F, e->c);
            a.label(L_e);
            return;
        }

        case ExprKind::Call: {
            // Supports: print(x) builtin; regular calls to procs by name in e->s
            // Convention: first 4 args in rcx,rdx,r8,r9; return in rax
            const std::string& callee = e->s;

            if (callee == "print") {
                if (e->args.size() != 1) throw std::runtime_error("print expects 1 arg");
                Expr* arg = e->args[0];
                if (arg->kind == ExprKind::StrLit) {
                    auto lbl = C.intern_str(arg->s);
                    a.lea_rcx_rip(lbl);
                    a.call(C.L_print_cstr);
                    a.mov_rax_imm64(0);
                    return;
                }
                else {
                    gen_expr(C, F, arg);
                    a.mov_rcx_rax();
                    a.call(C.L_print_i64);
                    a.mov_rax_imm64(0);
                    return;
                }
            }

            // Evaluate args
            // Save shadow: we keep stack aligned by caller prolog; we’ll push at most.
            // For bootstrap: evaluate each arg and move into reg.
            auto get = [&](size_t i)->Expr* { return e->args.at(i); };

            // Ensure 32-byte shadow space: sub rsp, 0x20; call; add rsp,0x20
            a.sub_rsp_imm32(0x20);

            if (e->args.size() > 0) { gen_expr(C, F, get(0)); a.mov_rcx_rax(); }
            if (e->args.size() > 1) { gen_expr(C, F, get(1)); a.mov_rdx_rax(); }
            if (e->args.size() > 2) { gen_expr(C, F, get(2)); a.emit(0x49); a.emit(0x89); a.emit(0xC0); } // mov r8, rax
            if (e->args.size() > 3) { gen_expr(C, F, get(3)); a.emit(0x49); a.emit(0x89); a.emit(0xC1); } // mov r9, rax
            if (e->args.size() > 4) {
                // Extra args not supported in this bootstrap file
                throw std::runtime_error("call with >4 args not supported in bootstrap emitter");
            }

            auto it = C.proc_label.find(callee);
            if (it == C.proc_label.end()) throw std::runtime_error("unknown proc: " + callee);
            a.call(it->second);

            a.add_rsp_imm32(0x20);
            return;
        }
        }
    }

    // Statement generator
    static void gen_stmt(LowerCtx& C, Frame& F, Stmt* s) {
        auto& a = C.code;
        switch (s->kind) {
        case StmtKind::Let: {
            if (!F.find(s->name)) F.alloc_slot8(s->name);
            gen_expr(C, F, s->expr);
            gen_store_var(C, F, s->name);
        } break;

        case StmtKind::Assign: {
            gen_expr(C, F, s->expr);
            gen_store_var(C, F, s->name);
        } break;

        case StmtKind::ExprStmt: {
            gen_expr(C, F, s->expr);
        } break;

        case StmtKind::Return: {
            gen_expr(C, F, s->expr);
            // epilog will ret
            a.mov_rsp_rbp();
            a.pop_rbp();
            a.ret();
        } break;

        case StmtKind::Label: {
            a.label("lbl$" + s->name);
        } break;

        case StmtKind::Goto2: {
            gen_expr(C, F, s->expr);
            a.test_rax_rax();
            a.jcc(0x85, "lbl$" + s->label_true);  // JNE
            a.jmp("lbl$" + s->label_false);
        } break;

        case StmtKind::Trap: {
            i64 code = 1;
            if (s->expr) { gen_expr(C, F, s->expr); a.mov_rcx_rax(); a.call_iat("iat$ExitProcess"); }
            else C.trap_exit(code);
        } break;

        case StmtKind::Halt: {
            C.trap_exit(0);
        } break;

        case StmtKind::Read32: {
            // read32 REG, off into x;
            // Bootstrap: compute index = off>>2, trap if misalign
            auto it = C.mmio.find(s->region);
            if (it == C.mmio.end()) throw std::runtime_error("unknown mmio region: " + s->region);
            // eval offset into rax
            gen_expr(C, F, s->expr2);
            // if (rax & 3) trap 0xEE
            a.emit(0x48); a.emit(0x83); a.emit(0xE0); a.emit(0x03); // and rax,3
            a.test_rax_rax();
            std::string L_ok = "L$rd_ok_" + s->name;
            a.jcc(0x84, L_ok);
            C.trap_exit(0xEE);
            a.label(L_ok);

            // Re-eval offset for index (keep simple)
            gen_expr(C, F, s->expr2);
            // rax >>= 2
            a.emit(0x48); a.emit(0xC1); a.emit(0xE8); a.emit(0x02); // shr rax,2
            // base ptr -> rcx = &mmio array
            a.emit(0x48); a.emit(0x8D); a.emit(0x0D); // lea rcx,[rip+disp32]  (address of .data)
            size_t at = a.b.size();
            a.emit32(0);
            a.rel32.push_back({ at, it->second.data_label });

            // load u32: mov eax, dword ptr [rcx + rax*4]
            a.emit(0x8B); a.emit(0x04); a.emit(0x81); // modrm=00 reg=eax r/m=100 sib, sib: scale=4 index=rax base=rcx
            // store into local var
            if (!F.find(s->name)) F.alloc_slot8(s->name);
            // zero-extend already in rax; store rax
            gen_store_var(C, F, s->name);
        } break;

        case StmtKind::Write32: {
            auto it = C.mmio.find(s->region);
            if (it == C.mmio.end()) throw std::runtime_error("unknown mmio region: " + s->region);

            // misalign check on offset expr2
            gen_expr(C, F, s->expr2);
            a.emit(0x48); a.emit(0x83); a.emit(0xE0); a.emit(0x03);
            a.test_rax_rax();
            std::string L_ok = "L$wr_ok_" + s->region;
            a.jcc(0x84, L_ok);
            C.trap_exit(0xEE);
            a.label(L_ok);

            // index = off>>2 in rdx
            gen_expr(C, F, s->expr2);
            a.emit(0x48); a.emit(0xC1); a.emit(0xE8); a.emit(0x02); // shr rax,2
            a.mov_rdx_rax(); // rdx=index

            // value in rax
            gen_expr(C, F, s->expr);
            // base ptr in rcx
            a.emit(0x48); a.emit(0x8D); a.emit(0x0D);
            size_t at = a.b.size();
            a.emit32(0);
            a.rel32.push_back({ at, it->second.data_label });

            // store dword ptr [rcx + rdx*4], eax
            a.emit(0x89); a.emit(0x04); a.emit(0x91); // sib scale=4 index=rdx base=rcx
        } break;

        case StmtKind::CallIntoSlot: {
            // call f(args) into slot N
            // Evaluate call as normal, then store RAX to g$slots[N]
            Expr call{};
            call.kind = ExprKind::Call;
            call.s = s->callee;
            call.args = s->call_args;
            gen_expr(C, F, &call);

            // store rax to g$slots + N*8
            // lea rcx, [rip+g$slots]
            a.emit(0x48); a.emit(0x8D); a.emit(0x0D);
            size_t at = a.b.size(); a.emit32(0);
            a.rel32.push_back({ at, C.slotmem_label });
            // mov [rcx + imm8], rax  (if within 0..127)
            u8 off8 = (u8)(s->slot_index * 8);
            a.emit(0x48); a.emit(0x89); a.emit(0x41); a.emit(off8); // mov [rcx+off8], rax
        } break;
        }
    }

    // Proc codegen
    static void gen_proc(LowerCtx& C, Proc* p) {
        auto& a = C.code;
        Frame F;

        // map params to slots and store incoming regs
        // Windows ABI: rcx, rdx, r8, r9
        for (auto& prm : p->params) F.alloc_slot8(prm);

        a.label("proc$" + p->name);

        // prolog: allocate stack for locals (we don’t know count yet; reserve a page-ish then keep it simple)
        // For correctness, we do a 0x200 frame. In your repo, compute exact frame after scanning lets.
        a.push_rbp(); a.mov_rbp_rsp(); a.sub_rsp_imm32(0x200);

        // store params to their slots
        auto store_param = [&](int idx, const std::string& name) {
            // move incoming reg to rax then store
            if (idx == 0) { a.mov_rax_rcx(); }
            else if (idx == 1) { a.mov_rax_rdx(); }
            else if (idx == 2) { a.emit(0x4C); a.emit(0x89); a.emit(0xC0); } // mov rax,r8? (not valid) keep simple: move r8->rax with: 4C 89 C0 is mov rax,r8 (REX.W + mov r/m64,r64)
            else if (idx == 3) { a.emit(0x4C); a.emit(0x89); a.emit(0xC8); } // mov rax,r9
            else throw std::runtime_error("params >4 not supported in bootstrap emitter");
            gen_store_var(C, F, name);
            };
        for (size_t i = 0; i < p->params.size(); i++) {
            store_param((int)i, p->params[i]);
        }

        // body
        for (auto* s : p->body) {
            gen_stmt(C, F, s);
        }

        // implicit return 0
        a.mov_rax_imm64(0);
        a.mov_rsp_rbp(); a.pop_rbp(); a.ret();
    }

    // ============================================================================
    // 5) Public entry: compile Program -> rane_out.exe
    // ============================================================================

    static void compile_to_exe(const Program& prog, const std::string& out_path) {
        LowerCtx C;

        // Allocate globals
        C.dat.add_u64(C.g_stdout_label, 0);
        C.dat.add_u64(C.slotmem_label, 0); // make label exist
        // Make slots block (16 slots)
        C.dat.label_off[C.slotmem_label] = (u32)C.dat.d.size();
        C.dat.d.resize(C.dat.d.size() + C.slotmem_count * 8, 0);

        // MMIO regions -> .data u32 arrays
        for (auto& r : prog.mmio) {
            if (r.size % 4 != 0) throw std::runtime_error("mmio size must be multiple of 4");
            u32 words = r.size / 4;
            std::string lbl = "mmio$" + r.name;
            C.dat.add_u32_array(lbl, words);
            C.mmio[r.name] = { r.base, r.size, lbl, words };
        }

        // Index procs
        for (auto* p : prog.procs) {
            C.proc_label[p->name] = "proc$" + p->name;
        }

        // Emit helpers + entry
        emit_helper_print_cstr(C);
        emit_helper_print_i64(C);
        // (mmio helpers are stubbed here; we inline read/write in statements above)
        emit_entry_and_init(C);

        // Emit procs (main + others)
        for (auto* p : prog.procs) {
            gen_proc(C, p);
        }

        // Ensure there's proc main
        if (!C.proc_label.count("main")) throw std::runtime_error("missing proc main()");

        // Build exe
        auto img = make_exe(C.code, C.rdat, C.dat, C.imp);
        write_file(out_path, img.file);
    }

} // namespace pe_x64

// ============================================================================
// 6) Notes for integrating into YOUR repo (what to replace, what to keep)
// ============================================================================
//
// Keep:
// - namespace pe_x64 (assembler, PE writer, lowering functions)
// Replace:
// - The minimal AST with your real AST
// - gen_expr/gen_stmt to switch on your AST node kinds
//
// Your `syntax.rane` includes more features than this bootstrap executable subset.
// You can still claim “full syntax support” by parsing + typechecking everything,
// and feature-gating non-lowered constructs with deterministic diagnostics.
// (That’s how you keep the syntax sheet exhaustive without silent ignores.)
//
// ============================================================================
// 7) If you want me to wire this to YOUR exact AST shapes
// ============================================================================
//
// Paste (or point to) your real structs/enums for:
// - rane_expr_t / rane_stmt_t / rane_proc_t / rane_program_t
// - how you represent literals, ops, call nodes, labels/goto, read32/write32
//
// Then I’ll respond with the adapted drop-in file that compiles your repo
// without the “bootstrap AST” section.
//
// ============================================================================

int main() {
    // This main() is only to show the emitter compiles standalone.
    // In your compiler, you call pe_x64::compile_to_exe(real_program_ast,"rane_out.exe").
    return 0;
}
