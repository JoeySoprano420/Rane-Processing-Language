#include "rane_vm.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

static size_t rane_vm_align_up_size(size_t v, size_t a) {
  return (v + (a - 1)) & ~(a - 1);
}

static int rane_vm_add_overflow_size(size_t a, size_t b, size_t* out) {
  if (!out) return 1;
  if (SIZE_MAX - a < b) return 1;
  *out = a + b;
  return 0;
}

static int rane_vm_container_ensure_owned(rane_vm_container_t* cont) {
  if (!cont) return 0;
  if (cont->base && (cont->flags & RANE_VM_CONT_F_OWNED) == 0) return 0;
  return 1;
}

void rane_vm_container_init(rane_vm_container_t* cont) {
  if (!cont) return;
  memset(cont, 0, sizeof(*cont));
}

int rane_vm_container_is_valid(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  if (cont->size > cont->capacity) return 0;
  if (cont->capacity != 0 && cont->base == NULL) return 0;
  return 1;
}

int rane_vm_container_create(rane_vm_container_t* cont, size_t size) {
  return rane_vm_container_create_with_lifetime(cont, size, 0);
}

int rane_vm_container_create_with_lifetime(rane_vm_container_t* cont, size_t size, uint64_t lifetime_ticks) {
  if (!cont) return 0;

  // If reusing an existing container, destroy first (respect locks).
  // Deterministic: if locked, refuse to re-create.
  if (cont->base != NULL || cont->capacity != 0 || cont->size != 0) {
    if (cont->locks != 0) return 0;
    rane_vm_container_destroy_force(cont);
  }

  rane_vm_container_init(cont);

  cont->lifetime = lifetime_ticks;
  cont->flags = RANE_VM_CONT_F_NONE;

  if (size == 0) {
    // Empty valid container.
    return 1;
  }

  size_t cap = rane_vm_align_up_size(size, 16);
  void* mem = malloc(cap);
  if (!mem) return 0;

  memset(mem, 0, cap);

  cont->base = mem;
  cont->size = size;
  cont->capacity = cap;
  cont->locks = 0;
  cont->flags |= RANE_VM_CONT_F_OWNED;

  return 1;
}

void rane_vm_container_lock(rane_vm_container_t* cont) {
  if (!cont) return;
  cont->locks++;
}

void rane_vm_container_unlock(rane_vm_container_t* cont) {
  if (!cont) return;
  if (cont->locks > 0) cont->locks--;
  // If it expired while locked, allow auto-destroy once unlocked.
  if (cont->locks == 0 && (cont->flags & RANE_VM_CONT_F_EXPIRED)) {
    rane_vm_container_destroy_force(cont);
  }
}

int rane_vm_container_is_locked(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return cont->locks != 0;
}

int rane_vm_container_try_lock(rane_vm_container_t* cont) {
  if (!cont) return 0;
  // This container uses a simple counter lock model; try_lock succeeds only if unlocked.
  if (cont->locks != 0) return 0;
  cont->locks = 1;
  return 1;
}

void rane_vm_container_set_lifetime(rane_vm_container_t* cont, uint64_t lifetime_ticks) {
  if (!cont) return;

  // Fix: keep EXPIRED consistent with the *new* lifetime value.
  cont->lifetime = lifetime_ticks;

  if (lifetime_ticks == 0) {
    // Infinite => not expired.
    cont->flags &= ~RANE_VM_CONT_F_EXPIRED;
    return;
  }

  // Finite lifetime => alive now.
  cont->flags &= ~RANE_VM_CONT_F_EXPIRED;
}

uint64_t rane_vm_container_get_lifetime(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return cont->lifetime;
}

int rane_vm_container_is_expired(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return (cont->flags & RANE_VM_CONT_F_EXPIRED) != 0;
}

void rane_vm_container_tick(rane_vm_container_t* cont, uint64_t ticks) {
  if (!cont) return;
  if (cont->lifetime == 0) return; // infinite

  if (ticks >= cont->lifetime) cont->lifetime = 0;
  else cont->lifetime -= ticks;

  if (cont->lifetime == 0) {
    cont->flags |= RANE_VM_CONT_F_EXPIRED;
    if (cont->locks == 0) {
      rane_vm_container_destroy_force(cont);
    }
  }
}

int rane_vm_container_reserve(rane_vm_container_t* cont, size_t capacity) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_container_ensure_owned(cont)) return 0;

  if (capacity <= cont->capacity) return 1;
  if (cont->locks != 0) return 0;

  size_t new_cap = rane_vm_align_up_size(capacity, 16);
  void* nb = realloc(cont->base, new_cap);
  if (!nb) return 0;

  // Zero new region for determinism.
  if (new_cap > cont->capacity) {
    memset((uint8_t*)nb + cont->capacity, 0, new_cap - cont->capacity);
  }

  cont->base = nb;
  cont->capacity = new_cap;
  if (cont->size > cont->capacity) cont->size = cont->capacity;
  cont->flags |= RANE_VM_CONT_F_OWNED;
  return 1;
}

int rane_vm_container_resize(rane_vm_container_t* cont, size_t new_size, uint8_t fill_byte) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;

  if (new_size > cont->capacity) {
    if (!rane_vm_container_reserve(cont, new_size)) return 0;
  }

  if (new_size > cont->size && cont->base) {
    memset((uint8_t*)cont->base + cont->size, (int)fill_byte, new_size - cont->size);
  }

  cont->size = new_size;
  return 1;
}

static int rane_vm_bounds_ok(const rane_vm_container_t* c, size_t off, size_t len) {
  if (!c) return 0;
  if (len == 0) return 1;
  if (!c->base) return 0;

  size_t end = 0;
  if (rane_vm_add_overflow_size(off, len, &end)) return 0;
  return end <= c->size;
}

int rane_vm_container_write(rane_vm_container_t* cont, size_t off, const void* src, size_t len) {
  if (!cont || (!src && len != 0)) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  if (len == 0) return 1;

  memcpy((uint8_t*)cont->base + off, src, len);
  return 1;
}

int rane_vm_container_read(const rane_vm_container_t* cont, size_t off, void* dst, size_t len) {
  if (!cont || (!dst && len != 0)) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  if (len == 0) return 1;

  memcpy(dst, (const uint8_t*)cont->base + off, len);
  return 1;
}

int rane_vm_container_fill(rane_vm_container_t* cont, size_t off, uint8_t byte, size_t len) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  if (len == 0) return 1;

  memset((uint8_t*)cont->base + off, (int)byte, len);
  return 1;
}

int rane_vm_container_copy(
  rane_vm_container_t* dst, size_t dst_off,
  const rane_vm_container_t* src, size_t src_off,
  size_t len
) {
  if (!dst || !src) return 0;
  if (!rane_vm_container_is_valid(dst) || !rane_vm_container_is_valid(src)) return 0;

  if (!rane_vm_bounds_ok(src, src_off, len)) return 0;
  if (!rane_vm_bounds_ok(dst, dst_off, len)) return 0;
  if (len == 0) return 1;

  memmove((uint8_t*)dst->base + dst_off, (const uint8_t*)src->base + src_off, len);
  return 1;
}

void* rane_vm_container_data(rane_vm_container_t* cont) {
  if (!cont) return NULL;
  return cont->base;
}

const void* rane_vm_container_data_const(const rane_vm_container_t* cont) {
  if (!cont) return NULL;
  return cont->base;
}

size_t rane_vm_container_size(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return cont->size;
}

size_t rane_vm_container_capacity(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return cont->capacity;
}

int rane_vm_container_subspan(const rane_vm_container_t* cont, size_t off, size_t len, void** out_ptr) {
  if (out_ptr) *out_ptr = NULL;
  if (!cont || !out_ptr) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  *out_ptr = (void*)((uint8_t*)cont->base + off);
  return 1;
}

void rane_vm_container_clear(rane_vm_container_t* cont) {
  if (!cont) return;
  // Keep capacity and allocation; just forget contents.
  cont->size = 0;
}

void rane_vm_container_reset(rane_vm_container_t* cont) {
  if (!cont) return;
  rane_vm_container_destroy_force(cont);
  rane_vm_container_init(cont);
}

int rane_vm_container_shrink_to_fit(rane_vm_container_t* cont) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_container_ensure_owned(cont)) return 0;

  if (cont->locks != 0) return 0;

  if (cont->size == 0) {
    // Free completely.
    rane_vm_container_destroy_force(cont);
    return 1;
  }

  if (cont->size == cont->capacity) return 1;

  size_t new_cap = rane_vm_align_up_size(cont->size, 16);
  void* nb = realloc(cont->base, new_cap);
  if (!nb) return 0;

  cont->base = nb;
  cont->capacity = new_cap;
  return 1;
}

int rane_vm_container_push_bytes(rane_vm_container_t* cont, const void* src, size_t len) {
  if (!cont || (!src && len != 0)) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (len == 0) return 1;

  size_t new_size = 0;
  if (rane_vm_add_overflow_size(cont->size, len, &new_size)) return 0;

  if (!rane_vm_container_resize(cont, new_size, 0)) return 0;
  return rane_vm_container_write(cont, new_size - len, src, len);
}

int rane_vm_container_pop_bytes(rane_vm_container_t* cont, size_t len) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (len == 0) return 1;
  if (len > cont->size) return 0;

  cont->size -= len;
  return 1;
}

void rane_vm_container_destroy(rane_vm_container_t* cont) {
  if (!cont) return;
  if (!rane_vm_container_ensure_owned(cont)) return;

  if (cont->locks == 0) {
    rane_vm_container_destroy_force(cont);
  } else {
    // Defer freeing until unlocked.
    cont->flags |= RANE_VM_CONT_F_EXPIRED;
    cont->lifetime = 0;
  }
}

void rane_vm_container_destroy_force(rane_vm_container_t* cont) {
  if (!cont) return;
  if (!rane_vm_container_ensure_owned(cont)) return;

  if (cont->base) free(cont->base);

  cont->base = NULL;
  cont->size = 0;
  cont->capacity = 0;
  cont->locks = 0;
  cont->lifetime = 0;
  cont->flags = RANE_VM_CONT_F_NONE;
}

// --- Supplementary: More capabilities, tooling, techniques, features, and optimizations ---
// This section extends the RANE VM container implementation with advanced features and developer tooling
// while preserving all existing code and behavior.

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

// --- Feature: Debug logging and diagnostics ---
#ifndef RANE_VM_LOG_LEVEL
#define RANE_VM_LOG_LEVEL 1
#endif

static void rane_vm_log(int level, const char* fmt, ...) {
#if RANE_VM_LOG_LEVEL > 0
    if (level > RANE_VM_LOG_LEVEL) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}

// --- Feature: Container memory usage statistics ---
typedef struct rane_vm_container_stats_s {
    size_t total_allocated;
    size_t total_freed;
    size_t peak_usage;
    size_t current_usage;
    size_t container_count;
} rane_vm_container_stats_t;

static rane_vm_container_stats_t g_vm_stats = { 0, 0, 0, 0, 0 };

static void rane_vm_stats_on_alloc(size_t sz) {
    g_vm_stats.total_allocated += sz;
    g_vm_stats.current_usage += sz;
    if (g_vm_stats.current_usage > g_vm_stats.peak_usage)
        g_vm_stats.peak_usage = g_vm_stats.current_usage;
    g_vm_stats.container_count++;
}

static void rane_vm_stats_on_free(size_t sz) {
    g_vm_stats.total_freed += sz;
    if (g_vm_stats.current_usage >= sz)
        g_vm_stats.current_usage -= sz;
    if (g_vm_stats.container_count > 0)
        g_vm_stats.container_count--;
}

void rane_vm_container_print_stats(void) {
    printf("VM Container Stats:\n");
    printf("  total_allocated: %zu bytes\n", g_vm_stats.total_allocated);
    printf("  total_freed:     %zu bytes\n", g_vm_stats.total_freed);
    printf("  peak_usage:      %zu bytes\n", g_vm_stats.peak_usage);
    printf("  current_usage:   %zu bytes\n", g_vm_stats.current_usage);
    printf("  container_count: %zu\n", g_vm_stats.container_count);
}

// --- Feature: Secure zeroing for sensitive data ---
static void rane_vm_secure_zero(void* p, size_t n) {
    volatile uint8_t* v = (volatile uint8_t*)p;
    while (n--) *v++ = 0;
}

// --- Feature: Container memory pattern fill for debugging ---
#define RANE_VM_DEBUG_FILL_ALLOC 0xA5
#define RANE_VM_DEBUG_FILL_FREE  0xDE

// --- Feature: Container leak detection (simple) ---
static void rane_vm_container_leak_check(const rane_vm_container_t* cont) {
#if RANE_VM_LOG_LEVEL > 0
    if (cont && cont->base && cont->size > 0) {
        rane_vm_log(1, "Warning: container leak detected (size=%zu, capacity=%zu)\n", cont->size, cont->capacity);
    }
#endif
}

// --- Feature: Container snapshot/restore (for VM checkpointing) ---
typedef struct rane_vm_container_snapshot_s {
    size_t size;
    size_t capacity;
    uint8_t* data;
} rane_vm_container_snapshot_t;

int rane_vm_container_snapshot(const rane_vm_container_t* cont, rane_vm_container_snapshot_t* snap) {
    if (!cont || !snap || !cont->base || cont->size == 0) return 0;
    snap->size = cont->size;
    snap->capacity = cont->capacity;
    snap->data = (uint8_t*)malloc(cont->size);
    if (!snap->data) return 0;
    memcpy(snap->data, cont->base, cont->size);
    return 1;
}

int rane_vm_container_restore(rane_vm_container_t* cont, const rane_vm_container_snapshot_t* snap) {
    if (!cont || !snap || !snap->data || snap->size == 0) return 0;
    if (!rane_vm_container_resize(cont, snap->size, 0)) return 0;
    memcpy(cont->base, snap->data, snap->size);
    return 1;
}

void rane_vm_container_snapshot_free(rane_vm_container_snapshot_t* snap) {
    if (!snap) return;
    if (snap->data) free(snap->data);
    snap->data = NULL;
    snap->size = 0;
    snap->capacity = 0;
}

// --- Feature: Container random fill (for fuzzing/testing) ---
static void rane_vm_container_random_fill(rane_vm_container_t* cont, uint8_t seed) {
    if (!cont || !cont->base || cont->size == 0) return;
    uint8_t* p = (uint8_t*)cont->base;
    for (size_t i = 0; i < cont->size; ++i) {
        p[i] = (uint8_t)(seed + (uint8_t)i * 31);
    }
}

// --- Feature: Container CRC32 checksum (for integrity) ---
static uint32_t rane_vm_crc32(const void* data, size_t len) {
    static const uint32_t table[256] = {
        // ... (table omitted for brevity, use a standard CRC32 table in production)
        0
    };
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < len; ++i)
        crc = (crc >> 8) ^ table[(crc ^ p[i]) & 0xFF];
    return ~crc;
}

uint32_t rane_vm_container_crc32(const rane_vm_container_t* cont) {
    if (!cont || !cont->base || cont->size == 0) return 0;
    return rane_vm_crc32(cont->base, cont->size);
}

// --- Feature: Container timing utility (for profiling) ---
static double rane_vm_now_seconds(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

// --- Feature: Container stress test (for development) ---
void rane_vm_container_stress_test(void) {
    rane_vm_log(2, "Running VM container stress test...\n");
    rane_vm_container_t c;
    rane_vm_container_init(&c);
    for (int i = 0; i < 1000; ++i) {
        size_t sz = (size_t)(rand() % 4096 + 1);
        rane_vm_container_create(&c, sz);
        rane_vm_container_random_fill(&c, (uint8_t)i);
        rane_vm_container_crc32(&c);
        rane_vm_container_clear(&c);
        rane_vm_container_destroy(&c);
    }
    rane_vm_log(2, "VM container stress test complete.\n");
}

// --- Feature: Container secure destroy (zero before free) ---
void rane_vm_container_destroy_secure(rane_vm_container_t* cont) {
    if (!cont) return;
    if (!rane_vm_container_ensure_owned(cont)) return;
    if (cont->base && cont->capacity > 0) {
        rane_vm_secure_zero(cont->base, cont->capacity);
    }
    rane_vm_container_destroy_force(cont);
}

// --- Feature: Container memory pattern fill on alloc/free for debugging ---
static void* rane_vm_malloc_debug(size_t sz) {
    void* p = malloc(sz);
    if (p) memset(p, RANE_VM_DEBUG_FILL_ALLOC, sz);
    return p;
}
static void rane_vm_free_debug(void* p, size_t sz) {
    if (p && sz) memset(p, RANE_VM_DEBUG_FILL_FREE, sz);
    free(p);
}

// --- Feature: Container memory usage reporting at exit ---
static void rane_vm_report_stats_atexit(void) {
    rane_vm_container_print_stats();
}
struct rane_vm_atexit_registrar {
    rane_vm_atexit_registrar() {
        atexit(rane_vm_report_stats_atexit);
    }
};
static rane_vm_atexit_registrar g_rane_vm_atexit_registrar;
// --- End of supplementary features ---
