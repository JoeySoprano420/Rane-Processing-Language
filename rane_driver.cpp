#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "rane_driver.h"
#include "rane_parser.h"
#include "rane_typecheck.h"
#include "rane_tir.h"
#include "rane_ssa.h"
#include "rane_regalloc.h"
#include "rane_optimize.h"
#include "rane_aot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char* rane_read_entire_file(const char* path, size_t* out_len) {
  if (out_len) *out_len = 0;
  FILE* f = NULL;
  fopen_s(&f, path, "rb");
  if (!f) return NULL;

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) { fclose(f); return NULL; }

  char* buf = (char*)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return NULL; }

  size_t rd = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[rd] = 0;
  if (out_len) *out_len = rd;
  return buf;
}

static rane_error_t rane_write_entire_file(const char* path, const void* data, size_t len) {
  FILE* f = NULL;
  fopen_s(&f, path, "wb");
  if (!f) return RANE_E_OS_API_FAIL;
  fwrite(data, 1, len, f);
  fclose(f);
  return RANE_OK;
}

// Minimal PE32+ writer for .text + .rdata + .idata.
// Bootstrap: imports msvcrt.dll!printf for output.
#pragma pack(push, 1)
typedef struct {
  uint16_t e_magic;
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovn;

  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  int32_t  e_lfanew;
} dos_hdr_t;

typedef struct {
  uint32_t Signature;
  IMAGE_FILE_HEADER FileHeader;
  IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} nt_hdr64_t;

typedef struct {
  uint32_t OriginalFirstThunk;
  uint32_t TimeDateStamp;
  uint32_t ForwarderChain;
  uint32_t Name;
  uint32_t FirstThunk;
} import_desc_t;

typedef struct {
  uint16_t Hint;
  char Name[1];
} import_by_name_t;
#pragma pack(pop)

static uint32_t align_up_u32(uint32_t v, uint32_t a) {
  return (v + (a - 1)) & ~(a - 1);
}

static void buf_write(uint8_t* img, size_t cap, size_t off, const void* src, size_t len) {
  if (off + len > cap) return;
  memcpy(img + off, src, len);
}

static rane_error_t rane_write_pe64_exe_with_printf(
  const char* out_path,
  const uint8_t* text,
  uint32_t text_size,
  const uint8_t* rdata,
  uint32_t rdata_size
) {
  const uint32_t file_align = 0x200;
  const uint32_t sect_align = 0x1000;

  const uint32_t text_rva = 0x1000;
  const uint32_t rdata_rva = 0x2000;
  const uint32_t idata_rva = 0x3000;

  uint32_t headers_size = align_up_u32((uint32_t)(sizeof(dos_hdr_t) + 0x40 + sizeof(nt_hdr64_t) + 3 * sizeof(IMAGE_SECTION_HEADER)), file_align);

  uint32_t text_raw_ptr = headers_size;
  uint32_t text_raw_size = align_up_u32(text_size, file_align);

  uint32_t rdata_raw_ptr = text_raw_ptr + text_raw_size;
  uint32_t rdata_raw_size = align_up_u32(rdata_size, file_align);

  // .idata layout (very small, fixed):
  // [import_desc printf][null desc]
  // [dll name]
  // [ILT (2 entries)]
  // [IAT (2 entries)]
  // [import_by_name "printf"]
  const char* dll = "msvcrt.dll";
  const char* fn = "printf";
  const uint32_t ilt_count = 2;
  const uint32_t thunk_size = 8; // 64-bit

  uint32_t idata_size = 0;
  uint32_t desc_off = 0;
  uint32_t dllname_off = desc_off + sizeof(import_desc_t) * 2;
  uint32_t ilt_off = align_up_u32(dllname_off + (uint32_t)strlen(dll) + 1, 8);
  uint32_t iat_off = ilt_off + ilt_count * thunk_size;
  uint32_t ibn_off = iat_off + ilt_count * thunk_size;
  ibn_off = align_up_u32(ibn_off, 2);
  uint32_t ibn_size = (uint32_t)(2 + strlen(fn) + 1);
  idata_size = align_up_u32(ibn_off + ibn_size, 8);

  uint32_t idata_raw_ptr = rdata_raw_ptr + rdata_raw_size;
  uint32_t idata_raw_size = align_up_u32(idata_size, file_align);

  uint32_t size_of_image = align_up_u32(idata_rva + align_up_u32(idata_size, sect_align), sect_align);

  uint8_t stub[0x40] = {0};
  const char* msg = "This program cannot be run in DOS mode.\r\n$";
  memcpy(stub, msg, strlen(msg));

  dos_hdr_t dos = {0};
  dos.e_magic = 0x5A4D;
  dos.e_cparhdr = (uint16_t)(sizeof(dos_hdr_t) / 16);
  dos.e_lfarlc = sizeof(dos_hdr_t);
  dos.e_lfanew = (int32_t)(sizeof(dos_hdr_t) + sizeof(stub));

  nt_hdr64_t nt = {0};
  nt.Signature = 0x00004550;
  nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
  nt.FileHeader.NumberOfSections = 3;
  nt.FileHeader.TimeDateStamp = (DWORD)time(NULL);
  nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
  nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;

  IMAGE_OPTIONAL_HEADER64* oh = &nt.OptionalHeader;
  oh->Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
  oh->AddressOfEntryPoint = text_rva;
  oh->BaseOfCode = text_rva;
  oh->ImageBase = 0x0000000140000000ULL;
  oh->SectionAlignment = sect_align;
  oh->FileAlignment = file_align;
  oh->MajorOperatingSystemVersion = 6;
  oh->MinorOperatingSystemVersion = 0;
  oh->MajorSubsystemVersion = 6;
  oh->MinorSubsystemVersion = 0;
  oh->Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
  oh->SizeOfImage = size_of_image;
  oh->SizeOfHeaders = headers_size;
  oh->SizeOfCode = align_up_u32(text_size, sect_align);
  oh->SizeOfInitializedData = align_up_u32(rdata_size, sect_align) + align_up_u32(idata_size, sect_align);
  oh->DllCharacteristics = IMAGE_DLLCHARACTERISTICS_NX_COMPAT | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
  oh->SizeOfStackReserve = 1ull << 20;
  oh->SizeOfStackCommit = 1ull << 12;
  oh->SizeOfHeapReserve = 1ull << 20;
  oh->SizeOfHeapCommit = 1ull << 12;
  oh->NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

  // Import directory
  oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = idata_rva;
  oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = idata_size;

  IMAGE_SECTION_HEADER sh_text = {0};
  memcpy(sh_text.Name, ".text", 5);
  sh_text.Misc.VirtualSize = text_size;
  sh_text.VirtualAddress = text_rva;
  sh_text.SizeOfRawData = text_raw_size;
  sh_text.PointerToRawData = text_raw_ptr;
  sh_text.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

  IMAGE_SECTION_HEADER sh_rdata = {0};
  memcpy(sh_rdata.Name, ".rdata", 6);
  sh_rdata.Misc.VirtualSize = rdata_size;
  sh_rdata.VirtualAddress = rdata_rva;
  sh_rdata.SizeOfRawData = rdata_raw_size;
  sh_rdata.PointerToRawData = rdata_raw_ptr;
  sh_rdata.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

  IMAGE_SECTION_HEADER sh_idata = {0};
  memcpy(sh_idata.Name, ".idata", 6);
  sh_idata.Misc.VirtualSize = idata_size;
  sh_idata.VirtualAddress = idata_rva;
  sh_idata.SizeOfRawData = idata_raw_size;
  sh_idata.PointerToRawData = idata_raw_ptr;
  sh_idata.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

  size_t file_size = (size_t)idata_raw_ptr + (size_t)idata_raw_size;
  uint8_t* img = (uint8_t*)calloc(1, file_size);
  if (!img) return RANE_E_OS_API_FAIL;

  size_t off = 0;
  buf_write(img, file_size, off, &dos, sizeof(dos)); off += sizeof(dos);
  buf_write(img, file_size, off, stub, sizeof(stub)); off += sizeof(stub);
  buf_write(img, file_size, off, &nt, sizeof(nt)); off += sizeof(nt);
  buf_write(img, file_size, off, &sh_text, sizeof(sh_text)); off += sizeof(sh_text);
  buf_write(img, file_size, off, &sh_rdata, sizeof(sh_rdata)); off += sizeof(sh_rdata);
  buf_write(img, file_size, off, &sh_idata, sizeof(sh_idata)); off += sizeof(sh_idata);

  // Sections
  buf_write(img, file_size, text_raw_ptr, text, text_size);
  buf_write(img, file_size, rdata_raw_ptr, rdata, rdata_size);

  // Build .idata
  uint8_t* idata = img + idata_raw_ptr;

  import_desc_t desc = {0};
  desc.OriginalFirstThunk = idata_rva + ilt_off;
  desc.Name = idata_rva + dllname_off;
  desc.FirstThunk = idata_rva + iat_off;

  buf_write(idata, idata_raw_size, desc_off, &desc, sizeof(desc));
  // null desc already zeroed
  buf_write(idata, idata_raw_size, dllname_off, dll, strlen(dll) + 1);

  // ILT / IAT entries: point to import-by-name
  uint64_t ibn_rva = (uint64_t)(idata_rva + ibn_off);
  buf_write(idata, idata_raw_size, ilt_off + 0, &ibn_rva, 8);
  uint64_t zero = 0;
  buf_write(idata, idata_raw_size, ilt_off + 8, &zero, 8);
  buf_write(idata, idata_raw_size, iat_off + 0, &ibn_rva, 8);
  buf_write(idata, idata_raw_size, iat_off + 8, &zero, 8);

  // import-by-name: [hint=0]["printf\0"]
  uint16_t hint = 0;
  buf_write(idata, idata_raw_size, ibn_off, &hint, 2);
  buf_write(idata, idata_raw_size, ibn_off + 2, fn, strlen(fn) + 1);

  rane_error_t e = rane_write_entire_file(out_path, img, file_size);
  free(img);
  return e;
}

typedef struct rane_string_patch_s {
  uint32_t code_imm_off; // offset of imm64 in code buffer
  uint64_t heap_ptr;     // original heap pointer from parser
} rane_string_patch_t;

static int collect_string_patches_from_tir(const rane_tir_module_t* mod, rane_string_patch_t* out, uint32_t cap, uint32_t* out_count) {
  if (out_count) *out_count = 0;
  if (!mod || !out || cap == 0) return 0;

  uint32_t n = 0;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    const rane_tir_function_t* f = &mod->functions[fi];
    for (uint32_t ii = 0; ii < f->inst_count; ii++) {
      const rane_tir_inst_t* inst = &f->insts[ii];
      if (inst->opcode != TIR_MOV) continue;
      if (inst->operand_count != 2) continue;
      if (inst->operands[0].kind != TIR_OPERAND_R) continue;
      if (inst->operands[1].kind != TIR_OPERAND_IMM) continue;
      if (inst->type != RANE_TYPE_P64) continue;

      // Heuristic: string literals are heap C-strings created by parser.
      uint64_t p = inst->operands[1].imm;
      const char* s = (const char*)(uintptr_t)p;
      if (!s) continue;

      // Accept printable / at least NUL-terminated.
      size_t len = strlen(s);
      if (len == 0) continue;

      // Find imm64 occurrences of this pointer in the generated code and patch them.
      // Codegen for MOV reg, imm64 is: REX.W + (B8+reg) + imm64, so imm starts at +2.
      // We'll scan the final code later; here we just remember the pointer.
      // We'll fill code_imm_off in a second scan of code bytes.
      if (n < cap) {
        out[n].code_imm_off = 0;
        out[n].heap_ptr = p;
        n++;
      }
    }
  }

  if (out_count) *out_count = n;
  return 1;
}

static uint32_t scan_code_for_mov_imm64(uint8_t* code, uint32_t code_size, uint64_t imm, uint32_t* out_imm_offs, uint32_t cap) {
  if (!code || !out_imm_offs || cap == 0) return 0;
  uint32_t found = 0;

  // Pattern: 48 B8+rd imm64
  for (uint32_t i = 0; i + 10 <= code_size; i++) {
    if (code[i] != 0x48) continue;
    uint8_t op = code[i + 1];
    if (op < 0xB8 || op > 0xBF) continue;

    uint64_t v = 0;
    memcpy(&v, code + i + 2, 8);
    if (v != imm) continue;

    uint32_t imm_off = i + 2;
    if (found < cap) out_imm_offs[found] = imm_off;
    found++;
  }

  return found;
}

rane_error_t rane_compile_file_to_exe(const rane_driver_options_t* opts) {
  if (!opts || !opts->input_path || !opts->output_path) return RANE_E_INVALID_ARG;

  size_t src_len = 0;
  char* src = rane_read_entire_file(opts->input_path, &src_len);
  if (!src) return RANE_E_OS_API_FAIL;

  rane_stmt_t* ast = NULL;
  rane_error_t err = rane_parse_source(src, &ast);
  if (err != RANE_OK) { free(src); return err; }

  err = rane_typecheck_ast(ast);
  if (err != RANE_OK) { free(src); return err; }

  rane_tir_module_t tir_mod = {};
  err = rane_lower_ast_to_tir(ast, &tir_mod);
  if (err != RANE_OK) { free(src); return err; }

  rane_build_ssa(&tir_mod);
  rane_allocate_registers(&tir_mod);

  // Optimize according to requested level
  err = rane_optimize_tir_with_level(&tir_mod, opts->opt_level);
  if (err != RANE_OK) { free(src); return err; }

  // AOT compile to raw code bytes
  void* code = NULL;
  size_t code_size = 0;
  rane_aot_result_t aot = {};
  err = rane_aot_compile_with_fixups(&tir_mod, &aot);
  if (err != RANE_OK) { free(src); return err; }
  code = aot.code;
  code_size = aot.code_size;

  // --- v0/v1 .rdata: format string + pooled user strings, patch MOV imm64s ---
  const uint64_t image_base = 0x0000000140000000ULL;
  const uint32_t rdata_rva = 0x2000;
  const uint64_t rdata_va = image_base + rdata_rva;

  const char* fmt = "%s";
  const size_t fmt_len = strlen(fmt) + 1;

  // Collect string pointers from the lowered TIR
  rane_string_patch_t str_patches[256];
  uint32_t str_patch_count = 0;
  collect_string_patches_from_tir(&tir_mod, str_patches, 256, &str_patch_count);

  // Build unique string list (by pointer identity)
  uint64_t uniq_ptrs[256];
  uint32_t uniq_count = 0;
  for (uint32_t i = 0; i < str_patch_count; i++) {
    uint64_t p = str_patches[i].heap_ptr;
    int seen = 0;
    for (uint32_t j = 0; j < uniq_count; j++) {
      if (uniq_ptrs[j] == p) { seen = 1; break; }
    }
    if (!seen && uniq_count < 256) uniq_ptrs[uniq_count++] = p;
  }

  // Layout: [fmt][pad8][str0][pad8]...[strN]
  uint32_t rdata_size = (uint32_t)fmt_len;
  rdata_size = align_up_u32(rdata_size, 8);

  uint32_t str_offs[256];
  memset(str_offs, 0, sizeof(str_offs));
  for (uint32_t i = 0; i < uniq_count; i++) {
    const char* s = (const char*)(uintptr_t)uniq_ptrs[i];
    uint32_t off = rdata_size;
    str_offs[i] = off;
    uint32_t sz = (uint32_t)(strlen(s) + 1);
    rdata_size += sz;
    rdata_size = align_up_u32(rdata_size, 8);
  }

  uint8_t* rdata = (uint8_t*)calloc(1, rdata_size);
  if (!rdata) { free(aot.call_fixups); free(code); free(src); return RANE_E_OS_API_FAIL; }

  memcpy(rdata, fmt, fmt_len);
  // strings
  for (uint32_t i = 0; i < uniq_count; i++) {
    const char* s = (const char*)(uintptr_t)uniq_ptrs[i];
    memcpy(rdata + str_offs[i], s, strlen(s) + 1);
  }

  // Patch all MOV imm64 uses of heap string pointers to their final .rdata VA
  uint8_t* c = (uint8_t*)code;
  for (uint32_t i = 0; i < uniq_count; i++) {
    uint64_t heap_p = uniq_ptrs[i];
    uint64_t va = rdata_va + (uint64_t)str_offs[i];

    uint32_t occ[256];
    uint32_t occ_count = scan_code_for_mov_imm64(c, (uint32_t)code_size, heap_p, occ, 256);
    uint32_t lim = occ_count;
    if (lim > 256) lim = 256;
    for (uint32_t k = 0; k < lim; k++) {
      uint32_t imm_off = occ[k];
      if (imm_off + 8 <= code_size) {
        memcpy(c + imm_off, &va, 8);
      }
    }
  }

  // Also patch print()'s format-string immediate (lowering uses imm=0 sentinel)
  {
    uint64_t zero64 = 0;
    uint64_t fmt_va = rdata_va + 0;
    uint32_t occ[256];
    uint32_t occ_count = scan_code_for_mov_imm64(c, (uint32_t)code_size, zero64, occ, 256);
    uint32_t lim = occ_count;
    if (lim > 256) lim = 256;
    for (uint32_t k = 0; k < lim; k++) {
      uint32_t imm_off = occ[k];
      if (imm_off + 8 <= code_size) {
        memcpy(c + imm_off, &fmt_va, 8);
      }
    }
  }

  // --- import patch address (msvcrt!printf IAT) based on our fixed PE layout ---
  // Must match `rane_write_pe64_exe_with_printf` layout.
  const uint32_t idata_rva = 0x3000;
  const char* dll = "msvcrt.dll";
  const uint32_t thunk_size = 8;
  const uint32_t ilt_count = 2;
  uint32_t desc_off = 0;
  uint32_t dllname_off = desc_off + (uint32_t)sizeof(import_desc_t) * 2;
  uint32_t ilt_off = align_up_u32(dllname_off + (uint32_t)strlen(dll) + 1, 8);
  uint32_t iat_off = ilt_off + ilt_count * thunk_size;
  const uint64_t iat_va = image_base + idata_rva + iat_off;

  // Patch calls using fixups (required)
  if (aot.call_fixup_count == 0 || !aot.call_fixups) {
    free(rdata);
    free(code);
    free(src);
    return RANE_E_INVALID_ARG;
  }

  for (uint32_t i = 0; i < aot.call_fixup_count; i++) {
    if (strcmp(aot.call_fixups[i].sym, "printf") != 0) continue;
    uint32_t off = aot.call_fixups[i].code_offset;
    if (off + 9 > code_size) continue;

    // Overwrite placeholder with: 48 8B 05 disp32 ; FF D0
    c[off + 0] = 0x48;
    c[off + 1] = 0x8B;
    c[off + 2] = 0x05;
    int32_t disp = (int32_t)(iat_va - (image_base + 0x1000 + (uint32_t)off + 7));
    memcpy(c + off + 3, &disp, 4);
    c[off + 7] = 0xFF;
    c[off + 8] = 0xD0;
  }

  // Emit exe with printf import
  err = rane_write_pe64_exe_with_printf(opts->output_path, (const uint8_t*)code, (uint32_t)code_size, rdata, rdata_size);

  free(aot.call_fixups);
  free(rdata);
  free(code);
  free(src);
  return err;
}
