#pragma once

#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rane_common.h"
#include "rane_tir.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------
// Versioning / ABI
// ---------------------------

#define RANE_LOADER_ABI_VERSION          0x00010000u  // v1.0
#define RANE_DIAG_BLOCK_VERSION          0x00010000u  // v1.0
#define RANE_CRASH_RECORD_VERSION        0x00010000u  // v1.0

// ---------------------------
// Band IDs (stable)
// ---------------------------

typedef enum rane_band_id_e : uint32_t {
  RANE_BAND_CORE  = 1,
  RANE_BAND_AOT   = 2,
  RANE_BAND_JIT   = 3,
  RANE_BAND_META  = 4,
  RANE_BAND_HEAP  = 5,
  RANE_BAND_MMAP  = 6,
} rane_band_id_t;

// ---------------------------
// JIT tiers / regions
// ---------------------------

typedef enum rane_jit_tier_e : uint32_t {
  RANE_JIT_TIER1_BASELINE = 1,
  RANE_JIT_TIER2_HOT      = 2,
  RANE_JIT_TIER_STUBS     = 3,
} rane_jit_tier_t;

// ---------------------------
// Error codes (stable + parseable)
// ---------------------------

typedef enum {
  RANE_OK = 0,
  RANE_E_INVALID_ARG,
  RANE_E_NOT_INITIALIZED,
  RANE_E_ALREADY_INITIALIZED,
  RANE_E_OS_API_FAIL,
  RANE_E_VERSION_MISMATCH,
  RANE_E_RESERVE_BAND_FAIL,
  RANE_E_BAND_OVERLAP,
  RANE_E_BAND_OUT_OF_RANGE,
  RANE_E_LAYOUT_INVARIANT_FAIL,
  RANE_E_AOT_LOAD_FAIL,
  RANE_E_AOT_SLOT_OOB,
  RANE_E_AOT_OUTSIDE_SLOT,
  RANE_E_AOT_RELOCATED,
  RANE_E_AOT_BAD_PE,
  RANE_E_AOT_BAD_MACHINE,
  RANE_E_AOT_SECTION_PERMS,
  RANE_E_AOT_IMPORTS_INVALID,
  RANE_E_JIT_RESERVE_FAIL,
  RANE_E_JIT_COMMIT_FAIL,
  RANE_E_JIT_OUTSIDE_BAND,
  RANE_E_JIT_WX_VIOLATION,
  RANE_E_JIT_SEAL_FAIL,
  RANE_E_JIT_CFG_REG_FAIL,
  RANE_E_JIT_REGISTRY_FAIL,
  RANE_E_EXEC_OUTSIDE_ALLOWED,
  RANE_E_RWX_FORBIDDEN,
  RANE_E_PROTECT_TRANSITION_DENIED,
  RANE_E_INVALID_INDIRECT_TARGET,
  RANE_E_DIAG_PUBLISH_FAIL,
  RANE_E_DIAG_ALREADY_PUBLISHED,
  RANE_E_UNKNOWN = 0x7FFFFFFF
} rane_error_t;

// ---------------------------
// Policy knobs
// ---------------------------

typedef struct rane_policy_s {
  uint32_t abi_version;               // must be RANE_LOADER_ABI_VERSION
  uint32_t flags;                     // bitmask below

  // Flags:
  // 0x0001 FAIL_ON_AOT_RELOC      (recommended)
  // 0x0002 DENY_RWX_ALWAYS        (recommended)
  // 0x0004 ENFORCE_EXEC_BANDS     (recommended)
  // 0x0008 REQUIRE_CFG_IF_AVAIL   (recommended)
  // 0x0010 ENABLE_INDIRECT_CHECKS (optional; heavy, dev/debug mode)
  uint32_t reserved0;

  // Optional hard limits
  uint32_t max_aot_slots;             // e.g., 32
  uint32_t reserved1;

} rane_policy_t;

enum {
  RANE_POLICY_FAIL_ON_AOT_RELOC        = 0x00000001u,
  RANE_POLICY_DENY_RWX_ALWAYS          = 0x00000002u,
  RANE_POLICY_ENFORCE_EXEC_BANDS       = 0x00000004u,
  RANE_POLICY_REQUIRE_CFG_IF_AVAIL     = 0x00000008u,
  RANE_POLICY_ENABLE_INDIRECT_CHECKS   = 0x00000010u,
};

// ---------------------------
// Band layout spec (logical, pre-ASLR)
// The loader computes runtime bases after reservations succeed.
// ---------------------------

typedef struct rane_band_spec_s {
  rane_band_id_t id;
  uint32_t       reserved0;
  uint64_t       logical_base;     // pre-ASLR "city plan" base
  uint64_t       size;             // bytes
} rane_band_spec_t;

typedef struct rane_layout_spec_s {
  uint32_t abi_version;            // RANE_LOADER_ABI_VERSION
  uint32_t reserved0;

  // Slot grid for AOT band
  uint64_t aot_band_base;          // logical base
  uint64_t aot_slot_size;          // e.g., 0x10000000
  uint32_t aot_slots;              // e.g., 32
  uint32_t reserved1;

  // JIT band
  uint64_t jit_band_base;          // logical base
  uint64_t jit_band_size;          // bytes

  // Optional: tier partition offsets (relative to JIT_BASE)
  uint64_t jit_tier1_off;          // default 0
  uint64_t jit_tier2_off;          // default 0x40000000
  uint64_t jit_stubs_off;          // default 0x80000000

  // Remaining bands (logical)
  uint64_t core_band_base;
  uint64_t core_band_size;
  uint64_t meta_band_base;
  uint64_t meta_band_size;
  uint64_t heap_band_base;
  uint64_t heap_band_size;
  uint64_t mmap_band_base;
  uint64_t mmap_band_size;

} rane_layout_spec_t;

// ---------------------------
// Runtime-resolved band map (published for tools)
// ---------------------------

#pragma pack(push, 1)
typedef struct rane_band_runtime_s {
  uint32_t id;          // rane_band_id_t
  uint32_t reserved0;
  uint64_t base;        // runtime address
  uint64_t end;         // runtime address (exclusive)
} rane_band_runtime_t;
#pragma pack(pop)

// ---------------------------
// AOT module record (published for tools)
// ---------------------------

#define RANE_MAX_MODULE_NAME 64

#pragma pack(push, 1)
typedef struct rane_aot_module_record_s {
  uint32_t slot_index;
  uint32_t flags;                // future use
  uint64_t expected_slot_base;   // runtime address
  uint64_t expected_slot_end;    // runtime address
  uint64_t module_base;          // runtime base returned by loader
  uint64_t module_size;          // SizeOfImage
  char     name[RANE_MAX_MODULE_NAME]; // null-terminated if possible
} rane_aot_module_record_t;
#pragma pack(pop)

// ---------------------------
// JIT region / entry records (published for tools)
// ---------------------------

#pragma pack(push, 1)
typedef struct rane_jit_region_record_s {
  uint32_t tier;                 // rane_jit_tier_t
  uint32_t state;                // 1=RW_EMIT, 2=RX_SEALED
  uint64_t base;
  uint64_t end;                  // exclusive
  uint64_t committed_bytes;
} rane_jit_region_record_t;
#pragma pack(pop)

enum {
  RANE_JIT_STATE_RW_EMIT  = 1,
  RANE_JIT_STATE_RX_SEALED= 2,
};

// ---------------------------
// Diagnostics block (stable binary layout)
// Place this in CORE band and make it R/O after publishing.
// Tooling can parse by signature + version.
// ---------------------------

#define RANE_DIAG_SIGNATURE 0x47414944454E4152ull /* "RANE DIAG" with endian-friendly marker */

#pragma pack(push, 1)
typedef struct rane_diag_block_s {
  uint64_t signature;           // RANE_DIAG_SIGNATURE
  uint32_t version;             // RANE_DIAG_BLOCK_VERSION
  uint32_t abi_version;         // RANE_LOADER_ABI_VERSION

  uint32_t policy_flags;        // rane_policy_t.flags snapshot
  uint32_t band_count;          // number of valid entries in bands[]
  uint32_t aot_module_count;    // number of valid entries in aot_modules[]
  uint32_t jit_region_count;    // number of valid entries in jit_regions[]

  // Monotonic counters (optional but useful)
  uint64_t exec_transitions;    // count of VirtualProtect transitions observed/allowed
  uint64_t exec_denials;        // count denied
  uint64_t jit_seals;           // count of RW->RX seals
  uint64_t violations;          // count of policy violations

  // Runtime-resolved band map
  // Keep fixed max for stable parsing; counts tell how many are used.
  rane_band_runtime_t bands[6];             // CORE,AOT,JIT,META,HEAP,MMAP

  // Fixed-size tables (for stable offsets); counts indicate active prefix.
  rane_aot_module_record_t aot_modules[64]; // support more than 32 to allow future growth
  rane_jit_region_record_t jit_regions[128];// track sealed regions/pages

  // Optional pointer to last crash record (if you keep one in-process)
  uint64_t last_crash_record_ptr;           // 0 if none
} rane_diag_block_t;
#pragma pack(pop)

// ---------------------------
// Crash record (stable for tooling)
// Written on violation before fastfail/terminate.
// ---------------------------

#define RANE_CRASH_SIGNATURE 0x48534152434E4152ull /* "RANECRASH" marker */

#pragma pack(push, 1)
typedef struct rane_crash_record_s {
  uint64_t signature;          // RANE_CRASH_SIGNATURE
  uint32_t version;            // RANE_CRASH_RECORD_VERSION
  uint32_t abi_version;        // RANE_LOADER_ABI_VERSION

  uint32_t error;              // rane_error_t
  uint32_t violation_site;     // enum below
  uint64_t thread_id;          // GetCurrentThreadId()
  uint64_t process_id;         // GetCurrentProcessId()

  // What was being attempted
  uint64_t addr;
  uint64_t size;
  uint32_t old_protect;        // previous protection (if known)
  uint32_t new_protect;        // requested protection (if applicable)

  // Classification hints (optional but very useful)
  uint32_t addr_band;          // rane_band_id_t (0 if unknown)
  uint32_t reserved0;
  uint32_t aot_slot;           // if addr in AOT slot, else 0xFFFFFFFF
  uint32_t reserved1;

  // Snapshot of resolved bands (small, stable)
  rane_band_runtime_t bands[6];

  // Small text payload (null-terminated if possible)
  char message[256];
} rane_crash_record_t;
#pragma pack(pop)

typedef enum rane_violation_site_e : uint32_t {
  RANE_VSITE_UNKNOWN            = 0,
  RANE_VSITE_BAND_RESERVE       = 1,
  RANE_VSITE_AOT_LOAD           = 2,
  RANE_VSITE_AOT_VALIDATE       = 3,
  RANE_VSITE_VIRTUALPROTECT     = 4,
  RANE_VSITE_JIT_ALLOC          = 5,
  RANE_VSITE_JIT_SEAL           = 6,
  RANE_VSITE_INDIRECT_TARGET    = 7,
  RANE_VSITE_DIAG_PUBLISH       = 8,
} rane_violation_site_t;

// ---------------------------
// Public API
// ---------------------------

typedef struct rane_loader_state_s rane_loader_state_t;

// rane_loader_init()
// - Reserves all bands
// - Initializes allocators (JIT/HEAP/META) to be band-confined
// - Enables policy enforcement hooks (optional: the caller can do it)
rane_error_t
rane_loader_init(
  const rane_layout_spec_t* layout,
  const rane_policy_t* policy,
  rane_loader_state_t** out_state
);

// rane_load_aot_module(slot, path)
// - Loads a PE image and validates placement inside the computed slot range
// - Records module into AOT slot table + diagnostics
// - If policy FAIL_ON_AOT_RELOC, must fail if module_base != expected_slot_base
rane_error_t
rane_load_aot_module(
  rane_loader_state_t* st,
  uint32_t slot_index,
  const wchar_t* path_utf16,          // Windows-native
  const char* module_name_ascii       // optional friendly name; can be NULL
);

// rane_jit_alloc_emit_seal()
// One-shot helper that:
// - Commits RW memory inside JIT band
// - Calls user emitter callback to fill machine code
// - Seals RX (W^X) + FlushInstructionCache
// - Registers code region (CFG + shim registry)
// Returns entrypoint pointer (inside allocated region)
// Caller can store entrypoint into an indirection cell after validation.
typedef struct rane_jit_emit_args_s {
  rane_jit_tier_t tier;
  uint32_t flags;                    // future use (e.g., "stubs", "no-cfg")
  uint64_t size;                     // bytes requested (will be page-aligned internally)
  uint64_t align;                    // entry alignment (e.g., 16); 0 => default
  uint64_t user_tag;                 // tooling ID (method hash, etc.)
} rane_jit_emit_args_t;

typedef rane_error_t (*rane_jit_emitter_fn)(
  void* rw_ptr,                      // writable pointer for emission
  uint64_t capacity,                 // bytes available
  uint64_t* out_entry_offset,         // entrypoint offset within region
  void* user_ctx
);

rane_error_t
rane_jit_alloc_emit_seal(
  rane_loader_state_t* st,
  const rane_jit_emit_args_t* args,
  rane_jit_emitter_fn emitter,
  void* user_ctx,
  void** out_entrypoint              // final RX entrypoint
);

// JIT for hot paths
rane_error_t rane_jit_hot_path(const rane_tir_function_t* func, void** out_code);

// rane_policy_on_virtualprotect()
// Hook point for enforcing W^X + exec-band policy.
// Call this from your VirtualProtect/NtProtect wrapper BEFORE applying it.
// If it returns RANE_OK, the transition is allowed.
// If not OK, deny and write crash record.
typedef struct rane_vprotect_ctx_s {
  void*  process_handle;             // HANDLE
  void*  address;                    // base address
  size_t size;                       // bytes
  uint32_t new_protect;              // PAGE_*
  uint32_t* inout_old_protect;       // optional; may be NULL
  uint32_t is_from_jit_pipeline;     // 1 if shim is sealing JIT, else 0
} rane_vprotect_ctx_t;

rane_error_t
rane_policy_on_virtualprotect(
  rane_loader_state_t* st,
  const rane_vprotect_ctx_t* ctx
);

// rane_diag_publish_block()
// Publishes a stable diagnostic block:
// - allocates in CORE band (or uses caller-supplied address)
// - populates band map + module table + jit registry snapshot
// - marks memory read-only after publish (recommended)
// Tooling can locate by signature scan or exported pointer.
typedef struct rane_diag_publish_args_s {
  uint32_t flags;                    // future use
  void*    preferred_address;        // optional; if NULL, shim chooses
  uint64_t reserved_bytes;           // optional; 0 => sizeof(rane_diag_block_t)
} rane_diag_publish_args_t;

rane_error_t
rane_diag_publish_block(
  rane_loader_state_t* st,
  const rane_diag_publish_args_t* args,
  const rane_diag_block_t** out_block // pointer to published block (R/O)
);
// ---------------------------
// Crash record helpers
// ---------------------------

// Writes a crash record (in CORE band if possible) and returns pointer.
// Implementations typically:
// - populate record
// - store pointer in diag block (last_crash_record_ptr)
// - then fastfail/terminate if "fatal"
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
);

// Optional: classify an address into band/slot for tooling + crash records.
void
rane_classify_address(
  rane_loader_state_t* st,
  const void* addr,
  uint32_t* out_band_id,     // rane_band_id_t or 0
  uint32_t* out_aot_slot     // slot index or 0xFFFFFFFF
);

#ifdef __cplusplus
} // extern "C"
#endif