#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rane_loader.h"
#include "rane_tir.h"

// ---------------------------
// Internal helpers
// ---------------------------

#ifndef RANE_MIN
#define RANE_MIN(a,b) ((a)<(b)?(a):(b))
#endif

static uint64_t rane_align_up_u64(uint64_t v, uint64_t a) {
  if (a == 0) return v;
  uint64_t m = a - 1;
  return (v + m) & ~m;
}

static int rane_is_power_of_two_u64(uint64_t x) {
  return x && ((x & (x - 1)) == 0);
}

static int rane_ptr_in_range_u64(uint64_t p, uint64_t base, uint64_t end_excl) {
  return (p >= base) && (p < end_excl);
}

static void rane_safe_strcpy(char* dst, size_t dst_cap, const char* src) {
  if (!dst || dst_cap == 0) return;
  if (!src) { dst[0] = 0; return; }
  size_t len = strlen(src);
  if (len >= dst_cap) len = dst_cap - 1;
  memcpy(dst, src, len);
  dst[len] = 0;
}

static void rane_wide_to_ascii_best_effort(const wchar_t* w, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  out[0] = 0;
  if (!w) return;
  // Best-effort conversion; not locale-perfect, but stable for tooling names.
  WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)out_cap, NULL, NULL);
  out[out_cap - 1] = 0;
}

static const char* rane_err_to_string(rane_error_t e) {
  switch (e) {
    case RANE_OK: return "RANE_OK";
    case RANE_E_INVALID_ARG: return "RANE_E_INVALID_ARG";
    case RANE_E_NOT_INITIALIZED: return "RANE_E_NOT_INITIALIZED";
    case RANE_E_ALREADY_INITIALIZED: return "RANE_E_ALREADY_INITIALIZED";
    case RANE_E_OS_API_FAIL: return "RANE_E_OS_API_FAIL";
    case RANE_E_VERSION_MISMATCH: return "RANE_E_VERSION_MISMATCH";
    case RANE_E_RESERVE_BAND_FAIL: return "RANE_E_RESERVE_BAND_FAIL";
    case RANE_E_BAND_OVERLAP: return "RANE_E_BAND_OVERLAP";
    case RANE_E_BAND_OUT_OF_RANGE: return "RANE_E_BAND_OUT_OF_RANGE";
    case RANE_E_LAYOUT_INVARIANT_FAIL: return "RANE_E_LAYOUT_INVARIANT_FAIL";
    case RANE_E_AOT_LOAD_FAIL: return "RANE_E_AOT_LOAD_FAIL";
    case RANE_E_AOT_SLOT_OOB: return "RANE_E_AOT_SLOT_OOB";
    case RANE_E_AOT_OUTSIDE_SLOT: return "RANE_E_AOT_OUTSIDE_SLOT";
    case RANE_E_AOT_RELOCATED: return "RANE_E_AOT_RELOCATED";
    case RANE_E_AOT_BAD_PE: return "RANE_E_AOT_BAD_PE";
    case RANE_E_AOT_BAD_MACHINE: return "RANE_E_AOT_BAD_MACHINE";
    case RANE_E_AOT_SECTION_PERMS: return "RANE_E_AOT_SECTION_PERMS";
    case RANE_E_AOT_IMPORTS_INVALID: return "RANE_E_AOT_IMPORTS_INVALID";
    case RANE_E_JIT_RESERVE_FAIL: return "RANE_E_JIT_RESERVE_FAIL";
    case RANE_E_JIT_COMMIT_FAIL: return "RANE_E_JIT_COMMIT_FAIL";
    case RANE_E_JIT_OUTSIDE_BAND: return "RANE_E_JIT_OUTSIDE_BAND";
    case RANE_E_JIT_WX_VIOLATION: return "RANE_E_JIT_WX_VIOLATION";
    case RANE_E_JIT_SEAL_FAIL: return "RANE_E_JIT_SEAL_FAIL";
    case RANE_E_JIT_CFG_REG_FAIL: return "RANE_E_JIT_CFG_REG_FAIL";
    case RANE_E_JIT_REGISTRY_FAIL: return "RANE_E_JIT_REGISTRY_FAIL";
    case RANE_E_EXEC_OUTSIDE_ALLOWED: return "RANE_E_EXEC_OUTSIDE_ALLOWED";
    case RANE_E_RWX_FORBIDDEN: return "RANE_E_RWX_FORBIDDEN";
    case RANE_E_PROTECT_TRANSITION_DENIED: return "RANE_E_PROTECT_TRANSITION_DENIED";
    case RANE_E_INVALID_INDIRECT_TARGET: return "RANE_E_INVALID_INDIRECT_TARGET";
    case RANE_E_DIAG_PUBLISH_FAIL: return "RANE_E_DIAG_PUBLISH_FAIL";
    case RANE_E_DIAG_ALREADY_PUBLISHED: return "RANE_E_DIAG_ALREADY_PUBLISHED";
    default: return "RANE_E_UNKNOWN";
  }
}

// PAGE_* helpers
static int rane_page_is_exec(uint32_t protect) {
  switch (protect & 0xFFu) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
      return 1;
    default:
      return 0;
  }
}

static int rane_page_is_write(uint32_t protect) {
  switch (protect & 0xFFu) {
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
      return 1;
    default:
      return 0;
  }
}

static int rane_page_is_rwx(uint32_t protect) {
  return rane_page_is_exec(protect) && rane_page_is_write(protect);
}

// ---------------------------
// Internal state
// ---------------------------

struct rane_loader_state_s {
  uint32_t abi_version;
  rane_layout_spec_t layout;
  rane_policy_t policy;

  // Reserved bands (runtime)
  rane_band_runtime_t bands[6]; // stable order: CORE,AOT,JIT,META,HEAP,MMAP
  uint32_t bands_ready;

  // AOT modules
  rane_aot_module_record_t aot_modules[64];
  HMODULE aot_hmods[64];
  uint32_t aot_count;

  // JIT region registry (simple bump allocator per tier)
  uint64_t jit_base;
  uint64_t jit_end;
  uint64_t jit_tier1_cur;
  uint64_t jit_tier2_cur;
  uint64_t jit_stubs_cur;

  rane_jit_region_record_t jit_regions[128];
  uint32_t jit_region_count;

  // Diagnostics
  const rane_diag_block_t* diag_block; // published R/O block pointer

  // Counters
  uint64_t exec_transitions;
  uint64_t exec_denials;
  uint64_t jit_seals;
  uint64_t violations;

  // Last crash record pointer (if written)
  const rane_crash_record_t* last_crash;
};

// Map stable band index -> id
static uint32_t rane_band_index_to_id(uint32_t idx) {
  switch (idx) {
    case 0: return RANE_BAND_CORE;
    case 1: return RANE_BAND_AOT;
    case 2: return RANE_BAND_JIT;
    case 3: return RANE_BAND_META;
    case 4: return RANE_BAND_HEAP;
    case 5: return RANE_BAND_MMAP;
    default: return 0;
  }
}

// Compute runtime "desired" bases. This shim uses the logical bases as desired
// addresses for VirtualAlloc(MEM_RESERVE). ASLR may prevent exact placement;
// in that case, you should decide policy:
// - strict: fail if reservation is not exactly at desired base
// - permissive: accept any reservation but keep structured "bands" anyway
//
// For your “structured under ASLR” concept, you typically want the OS to shift
// things, but Windows does not provide "one O that shifts all allocations".
// Therefore: we enforce structure by reservation at those addresses.
// If reservation fails, we fail fast (v1 behavior).
static rane_error_t rane_reserve_band_exact(uint64_t base, uint64_t size) {
  void* p = VirtualAlloc((void*)(uintptr_t)base, (SIZE_T)size, MEM_RESERVE, PAGE_NOACCESS);
  if (!p) return RANE_E_RESERVE_BAND_FAIL;
  if ((uint64_t)(uintptr_t)p != base) {
    // Released, because we require exact band anchoring in v1.
    VirtualFree(p, 0, MEM_RELEASE);
    return RANE_E_RESERVE_BAND_FAIL;
  }
  return RANE_OK;
}

static rane_error_t rane_compute_and_reserve_bands(rane_loader_state_t* st) {
  // Basic invariants
  if (!rane_is_power_of_two_u64(st->layout.aot_slot_size)) return RANE_E_LAYOUT_INVARIANT_FAIL;
  if (st->layout.aot_slots == 0 || st->layout.aot_slots > 64) return RANE_E_LAYOUT_INVARIANT_FAIL;

  // Reserve CORE
  rane_error_t e = rane_reserve_band_exact(st->layout.core_band_base, st->layout.core_band_size);
  if (e) return e;
  // Reserve AOT (entire band = slots * slot_size)
  uint64_t aot_size = (uint64_t)st->layout.aot_slots * st->layout.aot_slot_size;
  e = rane_reserve_band_exact(st->layout.aot_band_base, aot_size);
  if (e) return e;
  // Reserve JIT
  e = rane_reserve_band_exact(st->layout.jit_band_base, st->layout.jit_band_size);
  if (e) return e;
  // Reserve META/HEAP/MMAP
  e = rane_reserve_band_exact(st->layout.meta_band_base, st->layout.meta_band_size);
  if (e) return e;
  e = rane_reserve_band_exact(st->layout.heap_band_base, st->layout.heap_band_size);
  if (e) return e;
  e = rane_reserve_band_exact(st->layout.mmap_band_base, st->layout.mmap_band_size);
  if (e) return e;

  // Fill runtime band map (exact bases in v1)
  st->bands[0] = rane_band_runtime_t{ RANE_BAND_CORE, 0, st->layout.core_band_base,
                                       st->layout.core_band_base + st->layout.core_band_size };
  st->bands[1] = rane_band_runtime_t{ RANE_BAND_AOT,  0, st->layout.aot_band_base,
                                       st->layout.aot_band_base + aot_size };
  st->bands[2] = rane_band_runtime_t{ RANE_BAND_JIT,  0, st->layout.jit_band_base,
                                       st->layout.jit_band_base + st->layout.jit_band_size };
  st->bands[3] = rane_band_runtime_t{ RANE_BAND_META, 0, st->layout.meta_band_base,
                                       st->layout.meta_band_base + st->layout.meta_band_size };
  st->bands[4] = rane_band_runtime_t{ RANE_BAND_HEAP, 0, st->layout.heap_band_base,
                                       st->layout.heap_band_base + st->layout.heap_band_size };
  st->bands[5] = rane_band_runtime_t{ RANE_BAND_MMAP, 0, st->layout.mmap_band_base,
                                       st->layout.mmap_band_base + st->layout.mmap_band_size };

  // Overlap check
  for (int i = 0; i < 6; i++) {
    for (int j = i + 1; j < 6; j++) {
      uint64_t a0 = st->bands[i].base, a1 = st->bands[i].end;
      uint64_t b0 = st->bands[j].base, b1 = st->bands[j].end;
      int overlap = !(a1 <= b0 || b1 <= a0);
      if (overlap) return RANE_E_BAND_OVERLAP;
    }
  }

  // Initialize JIT cursors
  st->jit_base = st->bands[2].base;
  st->jit_end  = st->bands[2].end;
  st->jit_tier1_cur = st->jit_base + st->layout.jit_tier1_off;
  st->jit_tier2_cur = st->jit_base + st->layout.jit_tier2_off;
  st->jit_stubs_cur = st->jit_base + st->layout.jit_stubs_off;

  st->bands_ready = 1;
  return RANE_OK;
}

// ---------------------------
// AOT validation helpers
// ---------------------------

static int rane_validate_pe64_headers(void* base, uint64_t* out_size_of_image) {
  if (!base) return 0;
  IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
  IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)((uint8_t*)base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
  if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return 0;
  if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return 0;
  if (out_size_of_image) *out_size_of_image = nt->OptionalHeader.SizeOfImage;
  return 1;
}

// Validate no RWX sections; ensure typical .text is RX-ish
static int rane_validate_sections_perms(void* base) {
  IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
  IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)((uint8_t*)base + dos->e_lfanew);
  IMAGE_SECTION_HEADER* sec = (IMAGE_SECTION_HEADER*)((uint8_t*)&nt->OptionalHeader +
                                                      nt->FileHeader.SizeOfOptionalHeader);

  int saw_text = 0;
  for (uint16_t i = 0; i < nt->FileHeader.NumberOfSections; i++) {
    uint32_t ch = sec[i].Characteristics;

    int is_exec = (ch & IMAGE_SCN_MEM_EXECUTE) != 0;
    int is_write = (ch & IMAGE_SCN_MEM_WRITE) != 0;
    if (is_exec && is_write) return 0; // RWX section forbidden

    // Identify .text by name if present (not required, but good)
    char name[9] = {0};
    memcpy(name, sec[i].Name, 8);
    if (strncmp(name, ".text", 5) == 0) {
      saw_text = 1;
      // Require execute, forbid write
      if (!is_exec) return 0;
      if (is_write) return 0;
    }
  }
  // If no .text, still acceptable for weird modules, but most AOT will have it.
  return 1;
}

static int rane_module_is_relocated_best_effort(void* base) {
  // Best-effort heuristic:
  // If the loaded base != OptionalHeader.ImageBase, relocation probably occurred.
  // NOTE: With ASLR + DYNAMIC_BASE, Windows can relocate even if preferred base differs.
  // This is good enough for v1 to enforce "preferred base must match" policy.
  IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
  IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)((uint8_t*)base + dos->e_lfanew);
  uint64_t pref = nt->OptionalHeader.ImageBase;
  uint64_t loaded = (uint64_t)(uintptr_t)base;
  return loaded != pref;
}

// Compute slot runtime expected range for a slot index
static int rane_slot_range(const rane_loader_state_t* st, uint32_t slot, uint64_t* out_base, uint64_t* out_end) {
  if (slot >= st->layout.aot_slots) return 0;
  uint64_t base = st->layout.aot_band_base + (uint64_t)slot * st->layout.aot_slot_size;
  uint64_t end  = base + st->layout.aot_slot_size;
  if (out_base) *out_base = base;
  if (out_end)  *out_end = end;
  return 1;
}

// ---------------------------
// JIT helpers
// ---------------------------

static int rane_addr_in_band(const rane_loader_state_t* st, uint64_t addr, rane_band_id_t band) {
  for (int i = 0; i < 6; i++) {
    if (st->bands[i].id == band) return rane_ptr_in_range_u64(addr, st->bands[i].base, st->bands[i].end);
  }
  return 0;
}

// Reserve/commit inside JIT band only. In v1, we commit at a specific address
// using a bump pointer. If commit fails (fragmentation), fail fast.
static rane_error_t rane_jit_commit_at(uint64_t addr, uint64_t size, void** out_ptr) {
  void* p = VirtualAlloc((void*)(uintptr_t)addr, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE);
  if (!p) return RANE_E_JIT_COMMIT_FAIL;
  if ((uint64_t)(uintptr_t)p != addr) {
    // Unexpected placement; release commit and fail (v1 strictness)
    // Note: VirtualFree with MEM_DECOMMIT on committed pages.
    VirtualFree(p, (SIZE_T)size, MEM_DECOMMIT);
    return RANE_E_JIT_COMMIT_FAIL;
  }
  *out_ptr = p;
  return RANE_OK;
}

static rane_error_t rane_jit_register_region(rane_loader_state_t* st, rane_jit_tier_t tier, uint64_t base, uint64_t size, uint32_t state) {
  if (st->jit_region_count >= (uint32_t)(sizeof(st->jit_regions) / sizeof(st->jit_regions[0])))
    return RANE_E_JIT_REGISTRY_FAIL;

  rane_jit_region_record_t* r = &st->jit_regions[st->jit_region_count++];
  r->tier = (uint32_t)tier;
  r->state = state;
  r->base = base;
  r->end = base + size;
  r->committed_bytes = size;
  return RANE_OK;
}

static rane_error_t rane_jit_choose_cursor(rane_loader_state_t* st, rane_jit_tier_t tier, uint64_t* io_cursor) {
  uint64_t c = 0;
  if (tier == RANE_JIT_TIER1_BASELINE) c = st->jit_tier1_cur;
  else if (tier == RANE_JIT_TIER2_HOT) c = st->jit_tier2_cur;
  else if (tier == RANE_JIT_TIER_STUBS) c = st->jit_stubs_cur;
  else return RANE_E_INVALID_ARG;

  *io_cursor = c;
  return RANE_OK;
}

static void rane_jit_set_cursor(rane_loader_state_t* st, rane_jit_tier_t tier, uint64_t cursor) {
  if (tier == RANE_JIT_TIER1_BASELINE) st->jit_tier1_cur = cursor;
  else if (tier == RANE_JIT_TIER2_HOT) st->jit_tier2_cur = cursor;
  else if (tier == RANE_JIT_TIER_STUBS) st->jit_stubs_cur = cursor;
}

// ---------------------------
// Crash record writer (core band)
// ---------------------------

static void* rane_try_alloc_core_rw(uint64_t size) {
  // For v1 simplicity: just VirtualAlloc anywhere RW (OS decides) and keep record.
  // If you want "must be inside CORE band", you can implement a CORE allocator.
  return VirtualAlloc(NULL, (SIZE_T)size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

rane_error_t
rane_write_crash_record(
  rane_loader_state_t* st,
  rane_error_t err,
  rane_violation_site_t site,
  const char* message_ascii,
  const void* addr,
  uint64_t size,
  uint32_t old_protect,
  uint32_t new_protect,
  const rane_crash_record_t** out_record
) {
  if (!st) return RANE_E_NOT_INITIALIZED;

  rane_crash_record_t* rec = (rane_crash_record_t*)rane_try_alloc_core_rw(sizeof(rane_crash_record_t));
  if (!rec) return RANE_E_OS_API_FAIL;

  memset(rec, 0, sizeof(*rec));
  rec->signature = RANE_CRASH_SIGNATURE;
  rec->version = RANE_CRASH_RECORD_VERSION;
  rec->abi_version = st->abi_version;
  rec->error = (uint32_t)err;
  rec->violation_site = (uint32_t)site;
  rec->thread_id = (uint64_t)GetCurrentThreadId();
  rec->process_id = (uint64_t)GetCurrentProcessId();

  rec->addr = (uint64_t)(uintptr_t)addr;
  rec->size = size;
  rec->old_protect = old_protect;
  rec->new_protect = new_protect;

  uint32_t band = 0, slot = 0xFFFFFFFFu;
  rane_classify_address(st, addr, &band, &slot);
  rec->addr_band = band;
  rec->aot_slot = slot;

  for (int i = 0; i < 6; i++) rec->bands[i] = st->bands[i];

  rane_safe_strcpy(rec->message, sizeof(rec->message), message_ascii ? message_ascii : "");
  st->last_crash = rec;

  if (out_record) *out_record = rec;

  // If diagnostics exists, update pointer (best-effort; diag may be read-only)
  // We don't write into diag if it's read-only. Your tooling can find crash record by scanning too.

  return RANE_OK;
}

// ---------------------------
// Address classifier (consumes runtime state; tooling version consumes diag block)
// ---------------------------

void
rane_classify_address(
  rane_loader_state_t* st,
  const void* addr,
  uint32_t* out_band_id,
  uint32_t* out_aot_slot
) {
  if (out_band_id) *out_band_id = 0;
  if (out_aot_slot) *out_aot_slot = 0xFFFFFFFFu;
  if (!st || !addr) return;

  uint64_t a = (uint64_t)(uintptr_t)addr;

  // Bands
  for (int i = 0; i < 6; i++) {
    if (rane_ptr_in_range_u64(a, st->bands[i].base, st->bands[i].end)) {
      if (out_band_id) *out_band_id = st->bands[i].id;
      break;
    }
  }

  // AOT slot
  uint64_t aot_base = st->bands[1].base;
  uint64_t aot_end  = st->bands[1].end;
  if (rane_ptr_in_range_u64(a, aot_base, aot_end)) {
    uint64_t off = a - aot_base;
    uint32_t slot = (uint32_t)(off / st->layout.aot_slot_size);
    if (slot < st->layout.aot_slots && out_aot_slot) *out_aot_slot = slot;
  }
}

// ---------------------------
// rane_loader_init
// ---------------------------

rane_error_t
rane_loader_init(
  const rane_layout_spec_t* layout,
  const rane_policy_t* policy,
  rane_loader_state_t** out_state
) {
  if (!layout || !policy || !out_state) return RANE_E_INVALID_ARG;
  if (layout->abi_version != RANE_LOADER_ABI_VERSION) return RANE_E_VERSION_MISMATCH;
  if (policy->abi_version != RANE_LOADER_ABI_VERSION) return RANE_E_VERSION_MISMATCH;

  rane_loader_state_t* st = (rane_loader_state_t*)calloc(1, sizeof(rane_loader_state_t));
  if (!st) return RANE_E_OS_API_FAIL;

  st->abi_version = RANE_LOADER_ABI_VERSION;
  st->layout = *layout;
  st->policy = *policy;

  // Defensive defaults if caller leaves tier offsets at 0
  if (st->layout.jit_tier1_off == 0) st->layout.jit_tier1_off = 0x0000000000000000ull;
  if (st->layout.jit_tier2_off == 0) st->layout.jit_tier2_off = 0x0000000040000000ull;
  if (st->layout.jit_stubs_off == 0) st->layout.jit_stubs_off = 0x0000000080000000ull;

  // Basic layout invariants
  if (!rane_is_power_of_two_u64(st->layout.aot_slot_size)) { free(st); return RANE_E_LAYOUT_INVARIANT_FAIL; }
  if (st->layout.aot_slots == 0 || st->layout.aot_slots > 64) { free(st); return RANE_E_LAYOUT_INVARIANT_FAIL; }
  if (st->layout.jit_band_size == 0) { free(st); return RANE_E_LAYOUT_INVARIANT_FAIL; }

  // Reserve bands exactly (v1 strict)
  rane_error_t e = rane_compute_and_reserve_bands(st);
  if (e != RANE_OK) { free(st); return e; }

  // You can initialize custom allocators here; v1 uses bump pointers + VirtualAlloc commits.
  // Enforcement hooks are not installed automatically; you call policy functions from your wrappers.

  *out_state = st;
  return RANE_OK;
}

// ---------------------------
// rane_load_aot_module(slot, path)
// ---------------------------

rane_error_t
rane_load_aot_module(
  rane_loader_state_t* st,
  uint32_t slot_index,
  const wchar_t* path_utf16,
  const char* module_name_ascii
) {
  if (!st || !st->bands_ready) return RANE_E_NOT_INITIALIZED;
  if (!path_utf16) return RANE_E_INVALID_ARG;
  if (slot_index >= st->layout.aot_slots) return RANE_E_AOT_SLOT_OOB;
  if (st->aot_count >= 64) return RANE_E_OS_API_FAIL;

  uint64_t slot_base=0, slot_end=0;
  if (!rane_slot_range(st, slot_index, &slot_base, &slot_end)) return RANE_E_AOT_SLOT_OOB;

  // Load module. We use LoadLibraryExW. Windows decides base address.
  // Your strict policy then validates it is inside the slot and (optionally) not relocated.
  HMODULE h = LoadLibraryExW(path_utf16, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (!h) return RANE_E_AOT_LOAD_FAIL;

  void* base = (void*)h;

  // Validate PE64
  uint64_t size_of_image = 0;
  if (!rane_validate_pe64_headers(base, &size_of_image)) {
    FreeLibrary(h);
    return RANE_E_AOT_BAD_PE;
  }

  // Validate machine
  // (already checked by validate_pe64_headers)
  // Validate section perms (no RWX; .text exec no-write)
  if (!rane_validate_sections_perms(base)) {
    FreeLibrary(h);
    return RANE_E_AOT_SECTION_PERMS;
  }

  uint64_t loaded_base = (uint64_t)(uintptr_t)base;

  // Must be inside slot
  if (!rane_ptr_in_range_u64(loaded_base, slot_base, slot_end)) {
    FreeLibrary(h);
    return RANE_E_AOT_OUTSIDE_SLOT;
  }

  // Fail on relocation if policy says so (best-effort detection)
  if (st->policy.flags & RANE_POLICY_FAIL_ON_AOT_RELOC) {
    if (rane_module_is_relocated_best_effort(base)) {
      FreeLibrary(h);
      return RANE_E_AOT_RELOCATED;
    }
  }

  // Record
  rane_aot_module_record_t* rec = &st->aot_modules[st->aot_count];
  memset(rec, 0, sizeof(*rec));
  rec->slot_index = slot_index;
  rec->expected_slot_base = slot_base;
  rec->expected_slot_end  = slot_end;
  rec->module_base = loaded_base;
  rec->module_size = size_of_image;

  if (module_name_ascii && module_name_ascii[0]) {
    rane_safe_strcpy(rec->name, sizeof(rec->name), module_name_ascii);
  } else {
    // Use filename as name best-effort
    char tmp[128];
    rane_wide_to_ascii_best_effort(path_utf16, tmp, sizeof(tmp));
    rane_safe_strcpy(rec->name, sizeof(rec->name), tmp);
  }

  st->aot_hmods[st->aot_count] = h;
  st->aot_count++;

  return RANE_OK;
}

// ---------------------------
// rane_policy_on_virtualprotect()
// Called from your wrapper BEFORE doing VirtualProtect.
// If returns OK => you may apply. Else deny and crash record.
// ---------------------------

rane_error_t
rane_policy_on_virtualprotect(
  rane_loader_state_t* st,
  const rane_vprotect_ctx_t* ctx
) {
  if (!st || !st->bands_ready) return RANE_E_NOT_INITIALIZED;
  if (!ctx || !ctx->address || ctx->size == 0) return RANE_E_INVALID_ARG;

  st->exec_transitions++;

  uint64_t addr = (uint64_t)(uintptr_t)ctx->address;
  uint64_t size = (uint64_t)ctx->size;
  uint32_t newp = ctx->new_protect;

  // Deny RWX always if flagged
  if ((st->policy.flags & RANE_POLICY_DENY_RWX_ALWAYS) && rane_page_is_rwx(newp)) {
    st->exec_denials++;
    st->violations++;
    rane_write_crash_record(st, RANE_E_RWX_FORBIDDEN, RANE_VSITE_VIRTUALPROTECT,
                            "Denied PAGE_EXECUTE_READWRITE / RWX transition",
                            ctx->address, size, ctx->inout_old_protect ? *ctx->inout_old_protect : 0, newp,
                            NULL);
    return RANE_E_RWX_FORBIDDEN;
  }

  // If making executable, enforce exec-band policy
  if ((st->policy.flags & RANE_POLICY_ENFORCE_EXEC_BANDS) && rane_page_is_exec(newp)) {
    int in_jit = rane_addr_in_band(st, addr, RANE_BAND_JIT);
    int in_aot = rane_addr_in_band(st, addr, RANE_BAND_AOT);
    int in_core= rane_addr_in_band(st, addr, RANE_BAND_CORE);

    // Allow executable pages:
    // - AOT/core PE images (.text) OR
    // - JIT band *only if* called from JIT sealing pipeline
    //
    // We cannot reliably detect “PE image .text” at runtime without deeper VAD/loader inspection,
    // so v1 rules are:
    // - exec outside JIT is allowed only if address is inside CORE or AOT bands
    // - exec inside JIT is allowed only when is_from_jit_pipeline = 1
    //
    // If you want “only PE .text allowed”, add a MEMORY_BASIC_INFORMATION scan plus module enumeration.
    if (in_jit) {
      if (!ctx->is_from_jit_pipeline) {
        st->exec_denials++;
        st->violations++;
        rane_write_crash_record(st, RANE_E_PROTECT_TRANSITION_DENIED, RANE_VSITE_VIRTUALPROTECT,
                                "Denied: making memory executable inside JIT band outside JIT pipeline",
                                ctx->address, size, ctx->inout_old_protect ? *ctx->inout_old_protect : 0, newp,
                                NULL);
        return RANE_E_PROTECT_TRANSITION_DENIED;
      }
      // additionally enforce W^X: during pipeline, executable pages should not be writable
      if (rane_page_is_write(newp)) {
        st->exec_denials++;
        st->violations++;
        rane_write_crash_record(st, RANE_E_JIT_WX_VIOLATION, RANE_VSITE_VIRTUALPROTECT,
                                "Denied: JIT seal attempted with executable+write (W^X violation)",
                                ctx->address, size, ctx->inout_old_protect ? *ctx->inout_old_protect : 0, newp,
                                NULL);
        return RANE_E_JIT_WX_VIOLATION;
      }
      return RANE_OK;
    }

    // Not in JIT band: require CORE or AOT band (v1 approximation to "PE image code only")
    if (!(in_aot || in_core)) {
      st->exec_denials++;
      st->violations++;
      rane_write_crash_record(st, RANE_E_EXEC_OUTSIDE_ALLOWED, RANE_VSITE_VIRTUALPROTECT,
                              "Denied: executable memory outside CORE/AOT/JIT bands",
                              ctx->address, size, ctx->inout_old_protect ? *ctx->inout_old_protect : 0, newp,
                              NULL);
      return RANE_E_EXEC_OUTSIDE_ALLOWED;
    }

    // Still deny RWX already handled above
    return RANE_OK;
  }

  // Non-executable transitions: allowed (you can add additional band confinement here if desired)
  return RANE_OK;
}

// ---------------------------
// rane_jit_alloc_emit_seal()
// ---------------------------

rane_error_t
rane_jit_alloc_emit_seal(
  rane_loader_state_t* st,
  const rane_jit_emit_args_t* args,
  rane_jit_emitter_fn emitter,
  void* user_ctx,
  void** out_entrypoint
) {
  if (!st || !st->bands_ready) return RANE_E_NOT_INITIALIZED;
  if (!args || !emitter || !out_entrypoint) return RANE_E_INVALID_ARG;

  uint64_t size = args->size;
  if (size == 0) return RANE_E_INVALID_ARG;

  // Page align commit size
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  uint64_t page = (uint64_t)si.dwPageSize;
  uint64_t commit_size = rane_align_up_u64(size, page);

  // Pick tier cursor
  uint64_t cursor = 0;
  rane_error_t e = rane_jit_choose_cursor(st, args->tier, &cursor);
  if (e) return e;

  // Ensure within band and tier area doesn't exceed band end
  uint64_t region_base = rane_align_up_u64(cursor, page);
  uint64_t region_end  = region_base + commit_size;

  if (!rane_ptr_in_range_u64(region_base, st->jit_base, st->jit_end) ||
      !rane_ptr_in_range_u64(region_end - 1, st->jit_base, st->jit_end)) {
    return RANE_E_JIT_OUTSIDE_BAND;
  }

  // Commit RW memory at fixed address
  void* rw_ptr = NULL;
  e = rane_jit_commit_at(region_base, commit_size, &rw_ptr);
  if (e) return e;

  // Register region as RW_EMIT
  e = rane_jit_register_region(st, args->tier, region_base, commit_size, RANE_JIT_STATE_RW_EMIT);
  if (e) {
    VirtualFree(rw_ptr, (SIZE_T)commit_size, MEM_DECOMMIT);
    return e;
  }

  // Emit code
  uint64_t entry_off = 0;
  e = emitter(rw_ptr, commit_size, &entry_off, user_ctx);
  if (e != RANE_OK) {
    // Decommit memory, remove region from registry (simple rollback: just decrement count)
    st->jit_region_count--;
    VirtualFree(rw_ptr, (SIZE_T)commit_size, MEM_DECOMMIT);
    return e;
  }

  if (entry_off >= commit_size) {
    st->jit_region_count--;
    VirtualFree(rw_ptr, (SIZE_T)commit_size, MEM_DECOMMIT);
    return RANE_E_INVALID_ARG;
  }

  void* entry = (uint8_t*)rw_ptr + entry_off;

  // Flush icache before changing to RX
  FlushInstructionCache(GetCurrentProcess(), rw_ptr, (SIZE_T)commit_size);

  // Enforce policy on VirtualProtect transition via your policy hook (recommended)
  // Since this function is part of shim, we call policy directly.
  DWORD oldp = 0;
  rane_vprotect_ctx_t vp = {0};
  vp.process_handle = GetCurrentProcess();
  vp.address = rw_ptr;
  vp.size = (size_t)commit_size;
  vp.new_protect = PAGE_EXECUTE_READ;
  vp.inout_old_protect = (uint32_t*)&oldp;
  vp.is_from_jit_pipeline = 1;

  e = rane_policy_on_virtualprotect(st, &vp);
  if (e != RANE_OK) {
    st->jit_region_count--;
    VirtualFree(rw_ptr, (SIZE_T)commit_size, MEM_DECOMMIT);
    return e;
  }

  // Apply protection
  if (!VirtualProtect(rw_ptr, (SIZE_T)commit_size, PAGE_EXECUTE_READ, &oldp)) {
    st->jit_region_count--;
    VirtualFree(rw_ptr, (SIZE_T)commit_size, MEM_DECOMMIT);
    st->violations++;
    rane_write_crash_record(st, RANE_E_JIT_SEAL_FAIL, RANE_VSITE_JIT_SEAL,
                            "VirtualProtect failed sealing JIT region to RX",
                            rw_ptr, commit_size, oldp, PAGE_EXECUTE_READ, NULL);
    return RANE_E_JIT_SEAL_FAIL;
  }

  // Update region record state to RX_SEALED
  if (st->jit_region_count > 0) {
    rane_jit_region_record_t* rr = &st->jit_regions[st->jit_region_count - 1];
    rr->state = RANE_JIT_STATE_RX_SEALED;
  }

  st->jit_seals++;

  // CFG registration (optional):
  // For Windows CFG, registering dynamic call targets is non-trivial.
  // v1: we provide a hook point; you can integrate later with SetProcessValidCallTargets.
  // If policy requires CFG and you implement it, call it here; otherwise ignore.
  // if ((st->policy.flags & RANE_POLICY_REQUIRE_CFG_IF_AVAIL) && cfg_is_available()) { ... }

  // Advance tier cursor
  uint64_t next = region_end;
  rane_jit_set_cursor(st, args->tier, next);

  *out_entrypoint = entry;
  return RANE_OK;
}

// ---------------------------
// rane_diag_publish_block()
// ---------------------------

static void rane_diag_fill_from_state(rane_loader_state_t* st, rane_diag_block_t* b) {
  memset(b, 0, sizeof(*b));
  b->signature = RANE_DIAG_SIGNATURE;
  b->version = RANE_DIAG_BLOCK_VERSION;
  b->abi_version = st->abi_version;

  b->policy_flags = st->policy.flags;
  b->band_count = 6;
  b->aot_module_count = st->aot_count;
  b->jit_region_count = st->jit_region_count;

  b->exec_transitions = st->exec_transitions;
  b->exec_denials = st->exec_denials;
  b->jit_seals = st->jit_seals;
  b->violations = st->violations;

  for (int i = 0; i < 6; i++) b->bands[i] = st->bands[i];

  uint32_t am = RANE_MIN(st->aot_count, (uint32_t)(sizeof(b->aot_modules)/sizeof(b->aot_modules[0])));
  for (uint32_t i = 0; i < am; i++) b->aot_modules[i] = st->aot_modules[i];

  uint32_t jr = RANE_MIN(st->jit_region_count, (uint32_t)(sizeof(b->jit_regions)/sizeof(b->jit_regions[0])));
  for (uint32_t i = 0; i < jr; i++) b->jit_regions[i] = st->jit_regions[i];

  b->last_crash_record_ptr = (uint64_t)(uintptr_t)st->last_crash;
}

rane_error_t
rane_diag_publish_block(
  rane_loader_state_t* st,
  const rane_diag_publish_args_t* args,
  const rane_diag_block_t** out_block
) {
  if (!st || !st->bands_ready) return RANE_E_NOT_INITIALIZED;
  if (!out_block) return RANE_E_DIAG_ALREADY_PUBLISHED;

  uint64_t need = sizeof(rane_diag_block_t);
  if (args && args->reserved_bytes) need = rane_align_up_u64(args->reserved_bytes, 16);

  // v1: allocate anywhere, then optionally make RO.
  // If you want it inside CORE band, implement a CORE allocator that commits from CORE band.
  void* mem = NULL;
  if (args && args->preferred_address) {
    mem = VirtualAlloc(args->preferred_address, (SIZE_T)need, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  } else {
    mem = VirtualAlloc(NULL, (SIZE_T)need, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  }
  if (!mem) return RANE_E_DIAG_PUBLISH_FAIL;

  rane_diag_block_t* b = (rane_diag_block_t*)mem;
  rane_diag_fill_from_state(st, b);

  // Make it read-only (recommended)
  DWORD oldp = 0;
  if (!VirtualProtect(mem, (SIZE_T)need, PAGE_READONLY, &oldp)) {
    // Still usable; but policy might want this to be fatal.
    // v1 returns success but records violation.
    st->violations++;
  }

  st->diag_block = (const rane_diag_block_t*)mem;
  *out_block = st->diag_block;
  return RANE_OK;
}

// ---------------------------
// Tooling support: classify address from diag block
// ---------------------------

static const char* band_name(uint32_t id) {
  switch (id) {
    case RANE_BAND_CORE: return "CORE";
    case RANE_BAND_AOT:  return "AOT";
    case RANE_BAND_JIT:  return "JIT";
    case RANE_BAND_META: return "META";
    case RANE_BAND_HEAP: return "HEAP";
    case RANE_BAND_MMAP: return "MMAP";
    default: return "UNKNOWN";
  }
}

static int ptr_in(uint64_t p, uint64_t a, uint64_t b) { return p >= a && p < b; }

void rane_ptr_classify_from_diag(const rane_diag_block_t* d, uint64_t addr) {
  if (!d || d->signature != RANE_DIAG_SIGNATURE) {
    printf("No valid RANE diag block.\n");
    return;
  }

  // 1) Identify band by range check
  uint32_t band = 0;
  for (uint32_t i = 0; i < d->band_count && i < 6; i++) {
    if (ptr_in(addr, d->bands[i].base, d->bands[i].end)) {
      band = d->bands[i].id;
      break;
    }
  }

  printf("Address 0x%016" PRIX64 "\n", addr);
  printf("  Band: %s (%u)\n", band_name(band), band);

  // 2) If AOT: compute slot + module record match
  if (band == RANE_BAND_AOT) {
    uint64_t aot_base = 0, aot_end = 0;
    for (uint32_t i = 0; i < d->band_count && i < 6; i++) {
      if (d->bands[i].id == RANE_BAND_AOT) {
        aot_base = d->bands[i].base;
        aot_end  = d->bands[i].end;
        break;
      }
    }

    // Derive slot size from module records (or infer by scanning expected_slot_base gaps).
    // Here we simply search module records for a matching slot range.
    const rane_aot_module_record_t* best = NULL;
    for (uint32_t i = 0; i < d->aot_module_count && i < 64; i++) {
      const rane_aot_module_record_t* m = &d->aot_modules[i];
      if (ptr_in(addr, m->expected_slot_base, m->expected_slot_end)) {
        best = m;
        break;
      }
    }

    if (best) {
      printf("  AOT Slot: %u\n", best->slot_index);
      printf("  Module: %s\n", best->name);
      printf("  ModuleBase: 0x%016" PRIX64 "  SizeOfImage: 0x%016" PRIX64 "\n",
             best->module_base, best->module_size);
      printf("  SlotRange:  0x%016" PRIX64 " - 0x%016" PRIX64 "\n",
             best->expected_slot_base, best->expected_slot_end);
      printf("  OffsetInModule: +0x%016" PRIX64 "\n", addr - best->module_base);
    } else {
      // Still can compute slot index if you know slot size; if not, show raw band offset.
      printf("  AOT: No module record matched. Band offset: +0x%016" PRIX64 "\n", addr - aot_base);
    }
  }

  // 3) If JIT: find region record
  if (band == RANE_BAND_JIT) {
    const rane_jit_region_record_t* rbest = NULL;
    for (uint32_t i = 0; i < d->jit_region_count && i < 128; i++) {
      const rane_jit_region_record_t* r = &d->jit_regions[i];
      if (ptr_in(addr, r->base, r->end)) { rbest = r; break; }
    }
    if (rbest) {
      printf("  JIT Region: tier=%u state=%u\n", rbest->tier, rbest->state);
      printf("  Region: 0x%016" PRIX64 " - 0x%016" PRIX64 "\n", rbest->base, rbest->end);
      printf("  OffsetInRegion: +0x%016" PRIX64 "\n", addr - rbest->base);
    } else {
      printf("  JIT: No region record matched (diag may be stale).\n");
    }
  }

  // 4) Show last crash record pointer if available
  if (d->last_crash_record_ptr) {
    printf("  LastCrashRecordPtr: 0x%016" PRIX64 "\n", d->last_crash_record_ptr);
  }
}

rane_error_t rane_jit_hot_path(const rane_tir_function_t* func, void** out_code) {
  // Stub: compile func to JIT
  *out_code = NULL; // placeholder
  return RANE_OK;
}