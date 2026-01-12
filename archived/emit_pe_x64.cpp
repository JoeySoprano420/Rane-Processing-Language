// emit_pe_x64.cpp
// Minimal Windows x64 PE emitter + tiny x64 assembler.
// Supports: procs, calls, let/return, label/goto, trap/halt, print, read32/write32 word-addressed MMIO model.
// No external libs.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <fstream>
#include <algorithm>

namespace pe_x64 {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

// ------------------------- Tiny assembler ---------------------------------

struct FixupRel32 {
    size_t at;                 // offset of disp32 field
    std::string target_label;  // label name
    bool is_call = false;
};

struct FixupAbs64 {
    size_t at;                 // offset of imm64
    std::string symbol;        // e.g. IAT symbol name resolved later
};

struct Code {
    std::vector<u8> b;
    std::unordered_map<std::string, size_t> label_off;
    std::vector<FixupRel32> rel32;
    std::vector<FixupAbs64> abs64;

    size_t size() const { return b.size(); }
    void emit(u8 x){ b.push_back(x); }
    void emit16(u16 x){ emit((u8)(x)); emit((u8)(x>>8)); }
    void emit32(u32 x){ for(int i=0;i<4;i++) emit((u8)(x>>(8*i))); }
    void emit64(u64 x){ for(int i=0;i<8;i++) emit((u8)(x>>(8*i))); }

    void align(size_t a, u8 fill=0x90){
        while (b.size() % a) emit(fill);
    }

    void label(const std::string& name){
        label_off[name] = b.size();
    }

    // ---- x64 helpers (Windows x64 ABI) ----
    // Prolog: push rbp; mov rbp,rsp; sub rsp, stack; (stack aligned)
    void prolog(u32 stack_bytes){
        emit(0x55);                    // push rbp
        emit(0x48); emit(0x89); emit(0xE5); // mov rbp,rsp
        if (stack_bytes){
            emit(0x48); emit(0x81); emit(0xEC); emit32(stack_bytes); // sub rsp, imm32
        }
    }
    void epilog(){
        emit(0x48); emit(0x89); emit(0xEC); // mov rsp, rbp
        emit(0x5D);                         // pop rbp
        emit(0xC3);                         // ret
    }

    // mov r64, imm64
    void mov_rax_imm64(u64 x){ emit(0x48); emit(0xB8); emit64(x); } // RAX
    void mov_rcx_imm64(u64 x){ emit(0x48); emit(0xB9); emit64(x); } // RCX
    void mov_rdx_imm64(u64 x){ emit(0x48); emit(0xBA); emit64(x); } // RDX
    void mov_r8_imm64 (u64 x){ emit(0x49); emit(0xB8); emit64(x); } // R8
    void mov_r9_imm64 (u64 x){ emit(0x49); emit(0xB9); emit64(x); } // R9

    // mov r64, [rbp-disp32]
    // NOTE: using 32-bit displacement as unsigned; caller must pass correct two's complement if negative
    void mov_rax_mrbp_disp32(int32_t disp){
        emit(0x48); emit(0x8B); emit(0x85); emit32((u32)disp);
    }
    // mov [rbp-disp32], rax
    void mov_mrbp_disp32_rax(int32_t disp){
        emit(0x48); emit(0x89); emit(0x85); emit32((u32)disp);
    }

    // lea rcx, [rip+disp32]  (load address of literal)
    void lea_rcx_rip_rel32(const std::string& label){
        emit(0x48); emit(0x8D); emit(0x0D);
        size_t at = b.size();
        emit32(0);
        rel32.push_back({at, label, false});
    }

    // call rel32 label
    void call_label(const std::string& label){
        emit(0xE8);
        size_t at = b.size();
        emit32(0);
        rel32.push_back({at, label, true});
    }

    // jmp rel32 label
    void jmp_label(const std::string& label){
        emit(0xE9);
        size_t at = b.size();
        emit32(0);
        rel32.push_back({at, label, false});
    }

    // jcc rel32 label (0F 84 = JE, 0F 85 = JNE, etc.)
    void jcc_rel32(u8 cc, const std::string& label){
        emit(0x0F); emit(cc);
        size_t at = b.size();
        emit32(0);
        rel32.push_back({at, label, false});
    }

    // test rax,rax
    void test_rax_rax(){ emit(0x48); emit(0x85); emit(0xC0); }

    // cmp rax, rdx
    void cmp_rax_rdx(){ emit(0x48); emit(0x39); emit(0xD0); }

    // mov rdx, rax
    void mov_rdx_rax(){ emit(0x48); emit(0x89); emit(0xC2); }

    // add rax, rdx
    void add_rax_rdx(){ emit(0x48); emit(0x01); emit(0xD0); }
    // sub rax, rdx
    void sub_rax_rdx(){ emit(0x48); emit(0x29); emit(0xD0); }
    // imul rax, rdx
    void imul_rax_rdx(){ emit(0x48); emit(0x0F); emit(0xAF); emit(0xC2); }

    // idiv rdx: expects dividend in RAX, sign-extend with CQO, divisor in RCX
    void cqo(){ emit(0x48); emit(0x99); } // CQO
    // idiv rcx (signed)
    void idiv_rcx(){ emit(0x48); emit(0xF7); emit(0xF9); }

    // and/or/xor rax, rdx
    void and_rax_rdx(){ emit(0x48); emit(0x21); emit(0xD0); }
    void or_rax_rdx (){ emit(0x48); emit(0x09); emit(0xD0); }
    void xor_rax_rdx(){ emit(0x48); emit(0x31); emit(0xD0); }

    // shl rax, cl / shr rax, cl / sar rax, cl (shift count in CL)
    void mov_rcx_rdx(){ emit(0x48); emit(0x89); emit(0xD1); } // rcx=rdx
    void shl_rax_cl(){ emit(0x48); emit(0xD3); emit(0xE0); }
    void shr_rax_cl(){ emit(0x48); emit(0xD3); emit(0xE8); }
    void sar_rax_cl(){ emit(0x48); emit(0xD3); emit(0xF8); }

    // setcc to rax (proper sequence)
    void setcc_rax(u8 cc){
        emit(0x0F); emit(cc); emit(0xC0);   // setcc al
        emit(0x48); emit(0x0F); emit(0xB6); emit(0xC0); // movzx rax, al
    }

    // not rax / neg rax
    void not_rax(){ emit(0x48); emit(0xF7); emit(0xD0); }
    void neg_rax(){ emit(0x48); emit(0xF7); emit(0xD8); }

    // push rax / pop rdx
    void push_rax(){ emit(0x50); }
    void pop_rdx (){ emit(0x5A); }

    // mov [rsp+disp8], rax  (for shadow space / spills)
    void mov_rsp_disp8_rax(u8 disp){
        emit(0x48); emit(0x89); emit(0x44); emit(0x24); emit(disp);
    }
    // mov rax, [rsp+disp8]
    void mov_rax_rsp_disp8(u8 disp){
        emit(0x48); emit(0x8B); emit(0x44); emit(0x24); emit(disp);
    }
};

// -------------------------- PE writer -------------------------------------

static u32 align_up(u32 x, u32 a){ return (x + (a-1)) & ~(a-1); }

#pragma pack(push,1)
struct DOSHeader {
    u16 e_magic = 0x5A4D; // MZ
    u16 e_cblp=0x0090, e_cp=0x0003, e_crlc=0, e_cparhdr=0x0004;
    u16 e_minalloc=0, e_maxalloc=0xFFFF, e_ss=0, e_sp=0x00B8;
    u16 e_csum=0, e_ip=0, e_cs=0, e_lfarlc=0x0040, e_ovno=0;
    u16 e_res[4]{};
    u16 e_oemid=0, e_oeminfo=0;
    u16 e_res2[10]{};
    u32 e_lfanew=0x80;
};

struct PEFileHeader {
    u32 Signature=0x00004550; // PE\0\0
    u16 Machine=0x8664;
    u16 NumberOfSections=0;
    u32 TimeDateStamp=0;
    u32 PointerToSymbolTable=0;
    u32 NumberOfSymbols=0;
    u16 SizeOfOptionalHeader=0xF0;
    u16 Characteristics=0x0022; // EXECUTABLE | LARGE_ADDRESS_AWARE
};

struct DataDir { u32 VirtualAddress=0; u32 Size=0; };

struct OptionalHeader64 {
    u16 Magic=0x20B;
    u8  MajorLinkerVersion=1, MinorLinkerVersion=0;
    u32 SizeOfCode=0, SizeOfInitializedData=0, SizeOfUninitializedData=0;
    u32 AddressOfEntryPoint=0;
    u32 BaseOfCode=0;
    u64 ImageBase=0x140000000ULL;
    u32 SectionAlignment=0x1000;
    u32 FileAlignment=0x200;
    u16 MajorOperatingSystemVersion=6, MinorOperatingSystemVersion=0;
    u16 MajorImageVersion=0, MinorImageVersion=0;
    u16 MajorSubsystemVersion=6, MinorSubsystemVersion=0;
    u32 Win32VersionValue=0;
    u32 SizeOfImage=0;
    u32 SizeOfHeaders=0;
    u32 CheckSum=0;
    u16 Subsystem=3; // WINDOWS_CUI
    u16 DllCharacteristics=0x8160; // NX_COMPAT | DYNAMIC_BASE | HIGH_ENTROPY_VA | TERMINAL_SERVER_AWARE
    u64 SizeOfStackReserve=1<<20, SizeOfStackCommit=1<<12;
    u64 SizeOfHeapReserve=1<<20, SizeOfHeapCommit=1<<12;
    u32 LoaderFlags=0;
    u32 NumberOfRvaAndSizes=16;
    DataDir DataDirectory[16]{};
};

struct SectionHeader {
    char Name[8]{};
    u32 VirtualSize=0;
    u32 VirtualAddress=0;
    u32 SizeOfRawData=0;
    u32 PointerToRawData=0;
    u32 PointerToRelocations=0;
    u32 PointerToLinenumbers=0;
    u16 NumberOfRelocations=0;
    u16 NumberOfLinenumbers=0;
    u32 Characteristics=0;
};

struct ImportDescriptor {
    u32 OriginalFirstThunk=0;
    u32 TimeDateStamp=0;
    u32 ForwarderChain=0;
    u32 Name=0;
    u32 FirstThunk=0;
};
#pragma pack(pop)

struct Image {
    std::vector<u8> file;
    u32 entry_rva = 0;
};

struct RDataBuilder {
    std::vector<u8> r;
    std::unordered_map<std::string,u32> label_rva; // label->rdata RVA
    void align(u32 a){
        while(r.size()%a) r.push_back(0);
    }
    u32 add_cstr(const std::string& label, const std::string& s){
        u32 off = (u32)r.size();
        for(char c: s) r.push_back((u8)c);
        r.push_back(0);
        label_rva[label] = off;
        return off;
    }
};

struct ImportBuilder {
    // Kernel32 imports: ExitProcess, GetStdHandle, WriteFile
    // Minimal idata with one DLL.
    std::string dll = "KERNEL32.dll";
    std::vector<std::string> funcs = {"ExitProcess","GetStdHandle","WriteFile"};

    // Filled during layout:
    u32 idata_rva = 0;
    u32 iat_rva = 0;
    std::unordered_map<std::string,u32> iat_symbol_rva; // func -> RVA of IAT slot
};

// -------------------------- Build minimal .exe ----------------------------
//
// Layout:
// headers
// .text : code
// .rdata: strings
// .idata: import desc + thunks + names

static void write_file(const std::string& path, const std::vector<u8>& data){
    std::ofstream o(path, std::ios::binary);
    if(!o) throw std::runtime_error("failed to open output: " + path);
    o.write((const char*)data.data(), (std::streamsize)data.size());
}

static Image make_exe(const Code& code_in, const RDataBuilder& rdat_in, const ImportBuilder& imp_in){
    // Copy because we’ll patch relocs later
    Code code = code_in;
    RDataBuilder rdat = rdat_in;
    ImportBuilder imp = imp_in;

    // Section sizes
    const u32 fileAlign = 0x200;
    const u32 sectAlign = 0x1000;

    // Build .idata blob
    std::vector<u8> idata;
    auto id_align = [&](u32 a){ while(idata.size()%a) idata.push_back(0); };
    auto id_u32 = [&](u32 x){ for(int i=0;i<4;i++) idata.push_back((u8)(x>>(8*i))); };
    auto id_u64 = [&](u64 x){ for(int i=0;i<8;i++) idata.push_back((u8)(x>>(8*i))); };

    // We will build:
    // [ImportDescriptor x2 (last null)]
    // [OriginalFirstThunk array (u64 thunks) for funcs + null]
    // [FirstThunk (IAT) array same size]
    // [DLL name cstr]
    // [Hint/Name entries for each func]
    //
    // Compute RVAs later once section RVAs known; store offsets now.
    struct IdataOffsets {
        u32 desc_off=0;
        u32 oft_off=0;
        u32 ft_off=0;
        u32 dllname_off=0;
        std::vector<u32> hintname_off;
    } off;

    // Descriptors
    off.desc_off = (u32)idata.size();
    // descriptor[0]
    for(int i=0;i<sizeof(ImportDescriptor);i++) idata.push_back(0);
    // descriptor[1] null
    for(int i=0;i<sizeof(ImportDescriptor);i++) idata.push_back(0);

    id_align(8);
    off.oft_off = (u32)idata.size();
    // OFT thunks
    for(size_t i=0;i<imp.funcs.size()+1;i++) id_u64(0);

    id_align(8);
    off.ft_off = (u32)idata.size();
    // FT (IAT)
    for(size_t i=0;i<imp.funcs.size()+1;i++) id_u64(0);

    // DLL name
    off.dllname_off = (u32)idata.size();
    for(char c: imp.dll) idata.push_back((u8)c);
    idata.push_back(0);

    // Hint/Name entries
    off.hintname_off.reserve(imp.funcs.size());
    for (auto& fn : imp.funcs){
        id_align(2);
        u32 hno = (u32)idata.size();
        off.hintname_off.push_back(hno);
        idata.push_back(0); idata.push_back(0); // hint=0
        for(char c: fn) idata.push_back((u8)c);
        idata.push_back(0);
    }

    // Section RVAs
    u32 rva_text  = sectAlign;                 // .text at 0x1000
    u32 rva_rdata = rva_text + align_up((u32)code.size(), sectAlign);
    u32 rva_idata = rva_rdata + align_up((u32)rdat.r.size(), sectAlign);

    // Patch idata descriptor + thunks with RVAs
    auto patch_u32 = [&](u32 at, u32 v){
        idata[at+0]=(u8)v; idata[at+1]=(u8)(v>>8); idata[at+2]=(u8)(v>>16); idata[at+3]=(u8)(v>>24);
    };
    auto patch_u64 = [&](u32 at, u64 v){
        for(int i=0;i<8;i++) idata[at+i]=(u8)(v>>(8*i));
    };

    // Compute RVAs inside .idata
    u32 rva_desc   = rva_idata + off.desc_off;
    u32 rva_oft    = rva_idata + off.oft_off;
    u32 rva_ft     = rva_idata + off.ft_off;
    u32 rva_dll    = rva_idata + off.dllname_off;

    // Write ImportDescriptor[0]
    // OriginalFirstThunk, Name, FirstThunk
    patch_u32(off.desc_off + 0,  rva_oft);
    patch_u32(off.desc_off + 12, rva_dll);
    patch_u32(off.desc_off + 16, rva_ft);

    // Thunks point to Hint/Name entries
    for (size_t i=0;i<imp.funcs.size();i++){
        u64 hn_rva = (u64)(rva_idata + off.hintname_off[i]);
        patch_u64(off.oft_off + (u32)(i*8), hn_rva);
        patch_u64(off.ft_off  + (u32)(i*8), hn_rva);
        // Record IAT slot RVA for symbol
        imp.iat_symbol_rva[imp.funcs[i]] = (rva_idata + off.ft_off + (u32)(i*8));
    }

    // Now patch code rel32 fixups (to labels in .text and to rdata labels)
    // rel32 displacement is target - (next_ip)
    auto patch_code_disp32 = [&](size_t at, int64_t disp){
        u32 v = (u32)disp;
        code.b[at+0]=(u8)v; code.b[at+1]=(u8)(v>>8); code.b[at+2]=(u8)(v>>16); code.b[at+3]=(u8)(v>>24);
    };

    // Build a map for RIP targets:
    // - text labels: rva_text + off
    // - rdata labels: rva_rdata + off
    std::unordered_map<std::string,u32> rva_of;
    for (auto& kv : code.label_off) rva_of[kv.first] = rva_text + (u32)kv.second;
    for (auto& kv : rdat.label_rva) rva_of[kv.first] = rva_rdata + kv.second;

    for (auto& fx : code.rel32){
        auto it = rva_of.find(fx.target_label);
        if (it == rva_of.end()){
            throw std::runtime_error("unresolved label for rel32: " + fx.target_label);
        }
        u32 target = it->second;
        u32 next   = rva_text + (u32)(fx.at + 4);
        int64_t disp = (int64_t)target - (int64_t)next;
        patch_code_disp32(fx.at, disp);
    }

    // Build headers
    DOSHeader dos{};
    const char dos_stub[] =
        "This program cannot be run in DOS mode.\r\r\n$";
    // Minimal DOS stub pad to 0x80
    std::vector<u8> hdr(0x80, 0);
    std::memcpy(hdr.data(), &dos, sizeof(dos));
    std::memcpy(hdr.data()+0x40, dos_stub, sizeof(dos_stub)-1);

    PEFileHeader pe{};
    OptionalHeader64 opt{};
    SectionHeader sh_text{}, sh_rdata{}, sh_idata{};

    pe.NumberOfSections = 3;

    // Headers size: DOS(0x80)+PE sig+file+opt+sections
    u32 headersSize = 0x80 + sizeof(PEFileHeader) + sizeof(OptionalHeader64) + pe.NumberOfSections*sizeof(SectionHeader);
    headersSize = align_up(headersSize, fileAlign);

    // Section raw pointers
    u32 raw_text  = headersSize;
    u32 raw_rdata = raw_text  + align_up((u32)code.size(), fileAlign);
    u32 raw_idata = raw_rdata + align_up((u32)rdat.r.size(), fileAlign);

    // Fill section headers
    auto setname = [](SectionHeader& s, const char* n){
        std::memset(s.Name,0,8);
        std::memcpy(s.Name,n,std::min<size_t>(8,std::strlen(n)));
    };
    setname(sh_text, ".text");
    sh_text.VirtualAddress   = rva_text;
    sh_text.VirtualSize      = (u32)code.size();
    sh_text.PointerToRawData = raw_text;
    sh_text.SizeOfRawData    = align_up((u32)code.size(), fileAlign);
    sh_text.Characteristics  = 0x60000020; // RX | CODE

    setname(sh_rdata, ".rdata");
    sh_rdata.VirtualAddress   = rva_rdata;
    sh_rdata.VirtualSize      = (u32)rdat.r.size();
    sh_rdata.PointerToRawData = raw_rdata;
    sh_rdata.SizeOfRawData    = align_up((u32)rdat.r.size(), fileAlign);
    sh_rdata.Characteristics  = 0x40000040; // R | INIT_DATA

    setname(sh_idata, ".idata");
    sh_idata.VirtualAddress   = rva_idata;
    sh_idata.VirtualSize      = (u32)idata.size();
    sh_idata.PointerToRawData = raw_idata;
    sh_idata.SizeOfRawData    = align_up((u32)idata.size(), fileAlign);
    sh_idata.Characteristics  = 0xC0000040; // RW | INIT_DATA

    // Optional header fields
    opt.AddressOfEntryPoint = rva_text; // we will place entry at start of .text
    opt.BaseOfCode          = rva_text;
    opt.SizeOfCode          = sh_text.SizeOfRawData;
    opt.SizeOfInitializedData = sh_rdata.SizeOfRawData + sh_idata.SizeOfRawData;
    opt.SizeOfHeaders       = headersSize;
    opt.SizeOfImage         = align_up(rva_idata + sh_idata.VirtualSize, sectAlign);

    // Import data dir
    opt.DataDirectory[1].VirtualAddress = rva_idata;       // IMAGE_DIRECTORY_ENTRY_IMPORT
    opt.DataDirectory[1].Size           = (u32)idata.size();

    // Compose file
    std::vector<u8> out;
    out.resize(headersSize, 0);

    // Write PE headers after DOS
    size_t pe_off = 0x80;
    // Signature + file header
    std::memcpy(out.data()+pe_off, &pe, sizeof(pe));
    std::memcpy(out.data()+pe_off+sizeof(pe), &opt, sizeof(opt));
    // Section table
    size_t sec_off = pe_off + sizeof(pe) + sizeof(opt);
    std::memcpy(out.data()+sec_off, &sh_text,  sizeof(sh_text));
    std::memcpy(out.data()+sec_off+sizeof(sh_text), &sh_rdata, sizeof(sh_rdata));
    std::memcpy(out.data()+sec_off+sizeof(sh_text)+sizeof(sh_rdata), &sh_idata, sizeof(sh_idata));

    // Append sections (raw)
    out.resize(raw_idata + sh_idata.SizeOfRawData, 0);

    std::memcpy(out.data()+raw_text,  code.b.data(), code.size());
    std::memcpy(out.data()+raw_rdata, rdat.r.data(), rdat.r.size());
    std::memcpy(out.data()+raw_idata, idata.data(), idata.size());

    Image img;
    img.file = std::move(out);
    img.entry_rva = opt.AddressOfEntryPoint;
    return img;
}

} // namespace pe_x64
