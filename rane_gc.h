#pragma once

#include <cstddef>
#include <cstdint>

// Basic reference counting GC for RANE

typedef struct rane_gc_object_s {
  int ref_count;
  void* data;
  size_t size;
} rane_gc_object_t;

void rane_gc_init();
void rane_gc_shutdown();

rane_gc_object_t* rane_gc_alloc(size_t size);

// Convenience: allocate a GC object + payload and return payload pointer.
void* rane_gc_alloc_data(size_t size, rane_gc_object_t** out_obj);

void rane_gc_retain(rane_gc_object_t* obj);
void rane_gc_release(rane_gc_object_t* obj);
void rane_gc_collect();

// Mark-sweep GC
//
// This GC does not automatically scan the C/C++ stack; instead, users may register roots
// (a list of pointers to `rane_gc_object_t*`) that the collector will treat as
// starting points.
//
// For full object graph traversal, runtimes may provide a trace callback:
//   - the callback is invoked once per newly-marked object
//   - it must call `rane_gc_mark_sweep_mark(child)` for each outbound edge
void rane_gc_mark_sweep_init();
void rane_gc_mark_sweep_register_root(rane_gc_object_t** root_slot);
void rane_gc_mark_sweep_unregister_root(rane_gc_object_t** root_slot);

// Optional maintenance: remove NULL/invalid root slots from the registered root list.
void rane_gc_mark_sweep_compact_roots();

// New steps:
// - Unregister all registered root slots.
void rane_gc_mark_sweep_unregister_all_roots();

// - Clear all temporary roots at once.
void rane_gc_mark_sweep_clear_temp_roots();

void rane_gc_mark_sweep_collect();

// Temporary roots (stack-like): safer for short-lived references than registering root slots.
void rane_gc_mark_sweep_push_root(rane_gc_object_t* obj);
void rane_gc_mark_sweep_pop_root(rane_gc_object_t* obj);

// New: tracing and manual marking helpers
void rane_gc_mark_sweep_set_trace(void (*trace_fn)(rane_gc_object_t* obj));
void rane_gc_mark_sweep_mark(rane_gc_object_t* obj);

// New: optional allocation-count threshold to auto-trigger mark-sweep collection.
// 0 disables auto collection.
void rane_gc_mark_sweep_set_collect_threshold(size_t object_threshold);

// Introspection / stats
size_t rane_gc_object_count();
size_t rane_gc_object_capacity();
size_t rane_gc_root_count();
size_t rane_gc_root_capacity();

// Snapshot stats (does not allocate; safe to call under GC load).
typedef struct rane_gc_stats_s {
  size_t object_count;
  size_t object_capacity;
  size_t root_slot_count;
  size_t root_slot_capacity;
  size_t temp_root_count;
  size_t temp_root_capacity;
  size_t mark_words_capacity;
  size_t mark_work_count;
  size_t mark_work_capacity;
  size_t ms_collect_threshold;
  uint8_t ms_in_collect;
} rane_gc_stats_t;

void rane_gc_get_stats(rane_gc_stats_t* out_stats);

// Debug / validation
void rane_gc_run_selftest();

// ------------------------------------------------------------
// C++ convenience helpers (header-only)
// ------------------------------------------------------------

#ifdef __cplusplus

// Scoped temporary root: pushes `obj` as a mark-sweep temp root for the lifetime of the guard.
// This is useful for making allocations/calls safe without having to remember to pop.
struct rane_gc_scoped_root {
  rane_gc_object_t* obj;

  explicit rane_gc_scoped_root(rane_gc_object_t* o) : obj(o) {
    if (obj) rane_gc_mark_sweep_push_root(obj);
  }

  ~rane_gc_scoped_root() {
    if (obj) rane_gc_mark_sweep_pop_root(obj);
  }

  rane_gc_scoped_root(const rane_gc_scoped_root&) = delete;
  rane_gc_scoped_root& operator=(const rane_gc_scoped_root&) = delete;
};

#endif