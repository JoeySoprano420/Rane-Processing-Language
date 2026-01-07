#include "rane_heap.h"
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// rane_heap.cpp
//
// Bootstrap priority queue (min-heap) for opaque pointers.
//
// Design goals for this implementation:
// - Deterministic behavior (stable control flow; no hidden side effects).
// - Robustness: NULL checks, OOM handling, overflow-aware growth.
// - No new dependencies; C/C++ bootstrap style.
// - Keep existing public API intact (init/push/pop/destroy).
//
// Notes:
// - `cmp(a,b) < 0` means `a` has *higher priority* (comes out sooner) than `b`.
// - If `cmp` is NULL, the heap behaves as a deterministic bag (no ordering).
// - This file includes extra helpers as `static` so callers don't need header churn.
//   If you want them public, we can add declarations to `rane_heap.h`.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Small utilities
// -----------------------------------------------------------------------------

static void heap_swap(void** a, void** b) {
  void* t = *a;
  *a = *b;
  *b = t;
}

static size_t heap_parent(size_t i) { return (i - 1u) / 2u; }
static size_t heap_left(size_t i) { return (2u * i) + 1u; }
static size_t heap_right(size_t i) { return (2u * i) + 2u; }

static int heap_has_cmp(const rane_heap_t* h) { return h && h->cmp; }

static int heap_cmp(const rane_heap_t* h, const void* a, const void* b) {
  // If we don't have a comparator, treat all elements as equal.
  // This preserves determinism and avoids crashes.
  if (!heap_has_cmp(h)) return 0;
  return h->cmp(a, b);
}

// -----------------------------------------------------------------------------
// Capacity management
// -----------------------------------------------------------------------------

static int heap_reserve(rane_heap_t* h, size_t needed_capacity) {
  if (!h) return 0;
  if (needed_capacity <= h->capacity) return 1;

  size_t new_cap = (h->capacity != 0) ? h->capacity : 16u;
  while (new_cap < needed_capacity) {
    // overflow-safe doubling
    size_t next = new_cap * 2u;
    if (next < new_cap) return 0;
    new_cap = next;
  }

  void** nd = (void**)realloc(h->data, new_cap * sizeof(void*));
  if (!nd) return 0;

  h->data = nd;
  h->capacity = new_cap;
  return 1;
}

static void heap_shrink_to_fit(rane_heap_t* h) {
  if (!h) return;
  if (h->size == 0) {
    free(h->data);
    h->data = NULL;
    h->capacity = 0;
    return;
  }

  if (h->capacity == h->size) return;

  void** nd = (void**)realloc(h->data, h->size * sizeof(void*));
  if (!nd) return; // keep old buffer on OOM
  h->data = nd;
  h->capacity = h->size;
}

// -----------------------------------------------------------------------------
// Heapify operations
// -----------------------------------------------------------------------------

static void heapify_up(rane_heap_t* h, size_t idx) {
  if (!h || idx >= h->size) return;

  while (idx > 0) {
    size_t parent = heap_parent(idx);
    if (heap_cmp(h, h->data[idx], h->data[parent]) >= 0) break;
    heap_swap(&h->data[idx], &h->data[parent]);
    idx = parent;
  }
}

static void heapify_down(rane_heap_t* h, size_t idx) {
  if (!h || idx >= h->size) return;

  // Iterative form: avoids recursion depth and is easy to reason about.
  for (;;) {
    size_t left = heap_left(idx);
    size_t right = heap_right(idx);
    size_t smallest = idx;

    if (left < h->size && heap_cmp(h, h->data[left], h->data[smallest]) < 0) smallest = left;
    if (right < h->size && heap_cmp(h, h->data[right], h->data[smallest]) < 0) smallest = right;

    if (smallest == idx) break;

    heap_swap(&h->data[idx], &h->data[smallest]);
    idx = smallest;
  }
}

static void heap_build(rane_heap_t* h) {
  // Floyd bottom-up heap construction: O(n)
  if (!h || h->size <= 1) return;
  for (size_t i = (h->size / 2u); i-- > 0u;) {
    heapify_down(h, i);
  }
}

// -----------------------------------------------------------------------------
// Diagnostics / debug helpers
// -----------------------------------------------------------------------------

static int heap_is_valid(const rane_heap_t* h) {
  if (!h) return 0;
  if (h->size > h->capacity) return 0;
  if (h->size == 0) return 1;
  if (!h->data) return 0;

  // Validate min-heap property
  for (size_t i = 0; i < h->size; i++) {
    size_t l = heap_left(i);
    size_t r = heap_right(i);
    if (l < h->size && heap_cmp(h, h->data[l], h->data[i]) < 0) return 0;
    if (r < h->size && heap_cmp(h, h->data[r], h->data[i]) < 0) return 0;
  }
  return 1;
}

// -----------------------------------------------------------------------------
// Extra operations (TU-local for now; can be made public later)
// -----------------------------------------------------------------------------

static size_t heap_size(const rane_heap_t* h) { return h ? h->size : 0u; }

static void* heap_peek(const rane_heap_t* h) {
  if (!h || h->size == 0) return NULL;
  return h->data[0];
}

static void heap_clear(rane_heap_t* h) {
  if (!h) return;
  h->size = 0;
}

static void* heap_replace_top(rane_heap_t* h, void* item) {
  // Replace the root item and restore heap order.
  // Returns old root. If empty, behaves like push and returns NULL.
  if (!h) return NULL;
  if (h->size == 0) {
    rane_heap_push(h, item);
    return NULL;
  }

  void* old = h->data[0];
  h->data[0] = item;
  heapify_down(h, 0);
  return old;
}

static void* heap_remove_at(rane_heap_t* h, size_t idx) {
  // Remove element at arbitrary index.
  if (!h || idx >= h->size) return NULL;

  void* removed = h->data[idx];
  h->size--;

  if (idx != h->size) {
    h->data[idx] = h->data[h->size];

    // The new value might need to go up or down.
    if (idx > 0 && heap_cmp(h, h->data[idx], h->data[heap_parent(idx)]) < 0) {
      heapify_up(h, idx);
    } else {
      heapify_down(h, idx);
    }
  }

  return removed;
}

static int heap_contains_ptr(const rane_heap_t* h, const void* item) {
  // Linear scan (debug/utility). No comparator assumptions.
  if (!h || !h->data) return 0;
  for (size_t i = 0; i < h->size; i++) {
    if (h->data[i] == item) return 1;
  }
  return 0;
}

static int heap_try_pop_if(rane_heap_t* h, int (*pred)(const void*)) {
  // Pop if the top satisfies a predicate; returns 1 if popped.
  if (!h || h->size == 0 || !pred) return 0;
  void* t = heap_peek(h);
  if (!pred(t)) return 0;
  (void)rane_heap_pop(h);
  return 1;
}

// Bulk push: evaluates capacity once, then heapifies via Floyd build.
static int heap_push_many(rane_heap_t* h, void** items, size_t count) {
  if (!h) return 0;
  if (!items || count == 0) return 1;

  size_t old_size = h->size;
  if (!heap_reserve(h, old_size + count)) return 0;

  memcpy(&h->data[old_size], items, count * sizeof(void*));
  h->size = old_size + count;

  heap_build(h);
  return 1;
}

// -----------------------------------------------------------------------------
// Public API (existing signatures preserved)
// -----------------------------------------------------------------------------

void rane_heap_init(rane_heap_t* h, int (*cmp)(const void*, const void*)) {
  if (!h) return;
  h->data = NULL;
  h->size = 0;
  h->capacity = 0;
  h->cmp = cmp;
}

void rane_heap_push(rane_heap_t* h, void* item) {
  if (!h) return;

  if (!heap_reserve(h, h->size + 1u)) {
    // OOM: preserve determinism by leaving heap unchanged.
    return;
  }

  h->data[h->size] = item;
  h->size++;
  heapify_up(h, h->size - 1u);
}

void* rane_heap_pop(rane_heap_t* h) {
  if (!h || h->size == 0) return NULL;

  void* item = h->data[0];
  h->size--;

  if (h->size != 0) {
    h->data[0] = h->data[h->size];
    heapify_down(h, 0);
  }

  return item;
}

void rane_heap_destroy(rane_heap_t* h) {
  if (!h) return;
  free(h->data);
  h->data = NULL;
  h->size = 0;
  h->capacity = 0;
  h->cmp = NULL;
}
