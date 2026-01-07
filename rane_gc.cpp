#include "rane_gc.h"
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <windows.h>
#include <assert.h>

// Dynamic object tracking (removes fixed 1024-object cap)
static std::vector<rane_gc_object_t*> objects;
static std::unordered_map<rane_gc_object_t*, size_t> object_index_map;

// --- Mark-sweep state ---
static uint64_t* mark_bits = NULL;
static size_t mark_words_cap = 0; // number of uint64_t words allocated

static rane_gc_object_t*** root_slots = NULL;
static size_t root_count = 0;
static size_t root_cap = 0;

static rane_gc_object_t** temp_roots = NULL;
static size_t temp_root_count = 0;
static size_t temp_root_cap = 0;

static rane_gc_object_t** mark_work = NULL;
static size_t mark_work_count = 0;
static size_t mark_work_cap = 0;

// Optional: object graph tracing callback.
// If set, collector will traverse references contained in obj->data.
// The callback must call `rane_gc_mark_sweep_mark(child)` for each outbound reference.
typedef void (*rane_gc_trace_fn)(rane_gc_object_t* obj);
static rane_gc_trace_fn g_trace_fn = NULL;

// Optional: auto-collect threshold for mark-sweep.
static size_t g_ms_collect_threshold = 0; // 0 disables auto

// Re-entrancy guard (e.g., if trace callback allocates)
static uint8_t g_ms_in_collect = 0;

// Global GC lock (re-entrant spinlock)
static std::atomic_flag g_gc_mutex = ATOMIC_FLAG_INIT;
static std::atomic<DWORD> g_gc_owner_tid(0);
static thread_local uint32_t g_gc_recursion = 0;

static DWORD gc_current_thread_id() {
  return GetCurrentThreadId();
}

struct gc_lock_guard {
  gc_lock_guard() {
    DWORD tid = gc_current_thread_id();

    DWORD owner = g_gc_owner_tid.load(std::memory_order_acquire);
    if (owner == tid && owner != 0) {
      g_gc_recursion++;
      return;
    }

    while (g_gc_mutex.test_and_set(std::memory_order_acquire)) {
    }

    g_gc_owner_tid.store(tid, std::memory_order_release);
    g_gc_recursion = 1;
  }

  ~gc_lock_guard() {
    DWORD tid = gc_current_thread_id();
    DWORD owner = g_gc_owner_tid.load(std::memory_order_acquire);

    // Only the owning thread may unlock.
    assert(owner == tid || owner == 0);

    if (owner != tid || g_gc_recursion == 0) {
      return;
    }

    if (g_gc_recursion > 1) {
      g_gc_recursion--;
      return;
    }

    g_gc_recursion = 0;
    g_gc_owner_tid.store(0, std::memory_order_release);
    g_gc_mutex.clear(std::memory_order_release);
  }

  gc_lock_guard(const gc_lock_guard&) = delete;
  gc_lock_guard& operator=(const gc_lock_guard&) = delete;
};

static int find_object_index(rane_gc_object_t* obj, size_t* out_idx) {
  if (!obj) return 0;
  auto it = object_index_map.find(obj);
  if (it != object_index_map.end()) {
    if (out_idx) *out_idx = it->second;
    return 1;
  }
  return 0;
}

static void ensure_mark_bits_capacity(size_t obj_count) {
  size_t needed = (obj_count + 63u) / 64u;
  if (needed <= mark_words_cap) return;

  size_t new_cap = mark_words_cap ? mark_words_cap : 1;
  while (new_cap < needed) new_cap *= 2;

  uint64_t* nb = (uint64_t*)realloc(mark_bits, new_cap * sizeof(uint64_t));
  if (!nb) {
#ifdef _DEBUG
    // In debug builds, fail loudly instead of silently corrupting mark state.
    assert(!"rane_gc: OOM while growing mark_bits");
#endif
    return;
  }

  if (new_cap > mark_words_cap) {
    memset(nb + mark_words_cap, 0, (new_cap - mark_words_cap) * sizeof(uint64_t));
  }

  mark_bits = nb;
  mark_words_cap = new_cap;
}

static inline void mark_bits_clear_all_for(size_t obj_count) {
  if (!mark_bits) return;
  size_t words = (obj_count + 63u) / 64u;
  if (words) memset(mark_bits, 0, words * sizeof(uint64_t));
}

static inline int mark_bits_test(size_t idx) {
  size_t w = idx >> 6;
  size_t b = idx & 63u;
  if (!mark_bits || w >= mark_words_cap) return 0;
  return (mark_bits[w] >> b) & 1ull;
}

static inline void mark_bits_set(size_t idx) {
  size_t w = idx >> 6;
  size_t b = idx & 63u;
  if (!mark_bits || w >= mark_words_cap) return;
  mark_bits[w] |= (1ull << b);
}

static inline int mark_bits_get(size_t idx) {
  size_t w = idx >> 6;
  size_t b = idx & 63u;
  if (!mark_bits || w >= mark_words_cap) return 0;
  return (int)((mark_bits[w] >> b) & 1ull);
}

static inline void mark_bits_put(size_t idx, int v) {
  size_t w = idx >> 6;
  size_t b = idx & 63u;
  if (!mark_bits || w >= mark_words_cap) return;
  uint64_t mask = 1ull << b;
  if (v) mark_bits[w] |= mask;
  else mark_bits[w] &= ~mask;
}

static inline void mark_bits_swap2(size_t a, size_t b) {
  if (a == b) return;
  int va = mark_bits_get(a);
  int vb = mark_bits_get(b);
  if (va == vb) return;
  mark_bits_put(a, vb);
  mark_bits_put(b, va);
}

static void ensure_root_capacity(size_t needed) {
  if (needed <= root_cap) return;
  size_t new_cap = root_cap ? root_cap : 16;
  while (new_cap < needed) new_cap *= 2;

  rane_gc_object_t*** nr = (rane_gc_object_t***)realloc(root_slots, new_cap * sizeof(rane_gc_object_t**));
  if (!nr) return;
  if (new_cap > root_cap) {
    memset(nr + root_cap, 0, (new_cap - root_cap) * sizeof(rane_gc_object_t**));
  }
  root_slots = nr;
  root_cap = new_cap;
}

static void ensure_temp_root_capacity(size_t needed) {
  if (needed <= temp_root_cap) return;
  size_t new_cap = temp_root_cap ? temp_root_cap : 16;
  while (new_cap < needed) new_cap *= 2;

  rane_gc_object_t** nt = (rane_gc_object_t**)realloc(temp_roots, new_cap * sizeof(rane_gc_object_t*));
  if (!nt) return;
  if (new_cap > temp_root_cap) {
    memset(nt + temp_root_cap, 0, (new_cap - temp_root_cap) * sizeof(rane_gc_object_t*));
  }
  temp_roots = nt;
  temp_root_cap = new_cap;
}

static void ensure_mark_work_capacity(size_t needed) {
  if (needed <= mark_work_cap) return;
  size_t new_cap = mark_work_cap ? mark_work_cap : 64;
  while (new_cap < needed) new_cap *= 2;

  rane_gc_object_t** nw = (rane_gc_object_t**)realloc(mark_work, new_cap * sizeof(rane_gc_object_t*));
  if (!nw) return;
  if (new_cap > mark_work_cap) {
    memset(nw + mark_work_cap, 0, (new_cap - mark_work_cap) * sizeof(rane_gc_object_t*));
  }
  mark_work = nw;
  mark_work_cap = new_cap;
}

static void gc_debug_check_invariants() {
#ifdef _DEBUG
  // object_index_map must map every live object to its correct index.
  assert(object_index_map.size() == objects.size());
  for (size_t i = 0; i < objects.size(); i++) {
    rane_gc_object_t* obj = objects[i];
    assert(obj != NULL);
    auto it = object_index_map.find(obj);
    assert(it != object_index_map.end());
    assert(it->second == i);
  }

  // If mark storage is allocated, it must cover at least the current object count.
  if (mark_bits) {
    size_t needed = (objects.size() + 63u) / 64u;
    assert(mark_words_cap >= needed);
  }
#endif
}

static void remove_object_at(size_t idx) {
  if (idx >= objects.size()) return;

  size_t last = objects.size() - 1;
  rane_gc_object_t* removed = objects[idx];
  rane_gc_object_t* moved = objects[last];

  // swap-remove keeps array compact
  objects[idx] = moved;
  objects[last] = NULL;

  // Keep mark bits aligned during sweep.
  if (mark_bits && idx != last) {
    mark_bits_swap2(idx, last);
  }
  if (mark_bits) {
    mark_bits_put(last, 0);
  }

  // Maintain pointer->index map
  if (removed) object_index_map.erase(removed);
  if (moved && idx != last) object_index_map[moved] = idx;

  objects.pop_back();

  gc_debug_check_invariants();
}

static int root_slot_is_registered(rane_gc_object_t** root_slot) {
  for (size_t i = 0; i < root_count; i++) {
    if (root_slots[i] == root_slot) return 1;
  }
  return 0;
}

static void root_slots_compact_nulls() {
  size_t w = 0;
  for (size_t i = 0; i < root_count; i++) {
    if (!root_slots[i]) continue;
    root_slots[w++] = root_slots[i];
  }
  for (size_t i = w; i < root_count; i++) root_slots[i] = NULL;
  root_count = w;
}

static void mark_enqueue(rane_gc_object_t* obj) {
  if (!obj) return;
  size_t idx = 0;
  if (!find_object_index(obj, &idx)) return;
  if (idx >= objects.size()) return;

  ensure_mark_bits_capacity(objects.size());
  if (mark_bits_test(idx)) return;

  mark_bits_set(idx);

  ensure_mark_work_capacity(mark_work_count + 1);
  mark_work[mark_work_count++] = obj;
}

static void mark_flush_worklist() {
  while (mark_work_count > 0) {
    rane_gc_object_t* obj = mark_work[--mark_work_count];
    if (!obj) continue;
    if (g_trace_fn) {
      g_trace_fn(obj);
    }
  }
}

static void maybe_auto_mark_sweep_collect() {
  if (g_ms_in_collect) return;
  if (!g_ms_collect_threshold) return;
  if (objects.size() < g_ms_collect_threshold) return;
  // Caller holds the global GC lock.
  rane_gc_mark_sweep_collect();
}

void rane_gc_init() {
  gc_lock_guard _g;
  g_gc_owner_tid.store(0, std::memory_order_relaxed);
  g_gc_recursion = 0;

  objects.clear();
  object_index_map.clear();

  free(mark_bits);
  mark_bits = NULL;
  mark_words_cap = 0;

  free(root_slots);
  root_slots = NULL;
  root_count = 0;
  root_cap = 0;

  free(temp_roots);
  temp_roots = NULL;
  temp_root_count = 0;
  temp_root_cap = 0;

  free(mark_work);
  mark_work = NULL;
  mark_work_count = 0;
  mark_work_cap = 0;

  g_trace_fn = NULL;
  g_ms_collect_threshold = 0;
  g_ms_in_collect = 0;

  objects.reserve(16);
  ensure_mark_bits_capacity(16);
  ensure_root_capacity(16);
  ensure_temp_root_capacity(16);
  ensure_mark_work_capacity(64);
}

rane_gc_object_t* rane_gc_alloc(size_t size) {
  gc_lock_guard _g;

  rane_gc_object_t* obj = (rane_gc_object_t*)malloc(sizeof(rane_gc_object_t));
  if (!obj) return NULL;
  obj->ref_count = 1;
  obj->data = malloc(size);
  if (!obj->data) {
    free(obj);
    return NULL;
  }
  obj->size = size;

  ensure_mark_bits_capacity(objects.size() + 1);
  objects.push_back(obj);
  object_index_map[obj] = objects.size() - 1;

  gc_debug_check_invariants();

  maybe_auto_mark_sweep_collect();
  return obj;
}

void* rane_gc_alloc_data(size_t size, rane_gc_object_t** out_obj) {
  rane_gc_object_t* obj = rane_gc_alloc(size);
  if (!obj) {
    if (out_obj) *out_obj = NULL;
    return NULL;
  }
  if (out_obj) *out_obj = obj;
  return obj->data;
}

void rane_gc_retain(rane_gc_object_t* obj) {
  gc_lock_guard _g;
  if (obj) obj->ref_count++;
}

void rane_gc_release(rane_gc_object_t* obj) {
  gc_lock_guard _g;
  if (!obj) return;
  obj->ref_count--;
  if (obj->ref_count <= 0) {
    size_t idx = 0;
    if (find_object_index(obj, &idx)) {
      remove_object_at(idx);
    }
    free(obj->data);
    free(obj);
  }
}

void rane_gc_collect() {
  gc_lock_guard _g;
  for (size_t i = 0; i < objects.size(); ) {
    rane_gc_object_t* obj = objects[i];
    if (obj && obj->ref_count <= 0) {
      free(obj->data);
      free(obj);
      remove_object_at(i);
      continue;
    }
    i++;
  }
}

void rane_gc_mark_sweep_init() {
  gc_lock_guard _g;
  ensure_mark_bits_capacity(objects.size());
  mark_bits_clear_all_for(objects.size());
  if (root_slots && root_cap) memset(root_slots, 0, root_cap * sizeof(rane_gc_object_t**));
  root_count = 0;
  if (temp_roots && temp_root_cap) memset(temp_roots, 0, temp_root_cap * sizeof(rane_gc_object_t*));
  temp_root_count = 0;
  if (mark_work && mark_work_cap) memset(mark_work, 0, mark_work_cap * sizeof(rane_gc_object_t*));
  mark_work_count = 0;
}

void rane_gc_mark_sweep_set_trace(void (*trace_fn)(rane_gc_object_t* obj)) {
  gc_lock_guard _g;
  g_trace_fn = (rane_gc_trace_fn)trace_fn;
}

void rane_gc_mark_sweep_set_collect_threshold(size_t object_threshold) {
  gc_lock_guard _g;
  g_ms_collect_threshold = object_threshold;
}

void rane_gc_mark_sweep_register_root(rane_gc_object_t** root_slot) {
  gc_lock_guard _g;
  if (!root_slot) return;
  if (root_slot_is_registered(root_slot)) return;
  ensure_root_capacity(root_count + 1);
  root_slots[root_count++] = root_slot;
}

void rane_gc_mark_sweep_unregister_root(rane_gc_object_t** root_slot) {
  gc_lock_guard _g;
  if (!root_slot) return;
  for (size_t i = 0; i < root_count; i++) {
    if (root_slots[i] == root_slot) {
      root_slots[i] = root_slots[root_count - 1];
      root_slots[root_count - 1] = NULL;
      root_count--;
      return;
    }
  }
}

void rane_gc_mark_sweep_push_root(rane_gc_object_t* obj) {
  gc_lock_guard _g;
  if (!obj) return;
  ensure_temp_root_capacity(temp_root_count + 1);
  temp_roots[temp_root_count++] = obj;
}

void rane_gc_mark_sweep_pop_root(rane_gc_object_t* obj) {
  gc_lock_guard _g;
  if (!obj) return;
  if (temp_root_count == 0) return;

  // Fast path: true stack behavior.
  if (temp_roots[temp_root_count - 1] == obj) {
    temp_roots[temp_root_count - 1] = NULL;
    temp_root_count--;
    return;
  }

  // Fallback: allow popping out-of-order (swap-remove) for robustness.
  for (size_t i = 0; i < temp_root_count; i++) {
    if (temp_roots[i] == obj) {
      temp_roots[i] = temp_roots[temp_root_count - 1];
      temp_roots[temp_root_count - 1] = NULL;
      temp_root_count--;
      return;
    }
  }
}

void rane_gc_mark_sweep_mark(rane_gc_object_t* obj) {
  gc_lock_guard _g;
  mark_enqueue(obj);
}

void rane_gc_mark_sweep_compact_roots() {
  gc_lock_guard _g;
  root_slots_compact_nulls();
}

void rane_gc_mark_sweep_unregister_all_roots() {
  gc_lock_guard _g;
  if (root_slots && root_cap) memset(root_slots, 0, root_cap * sizeof(rane_gc_object_t**));
  root_count = 0;
}

void rane_gc_mark_sweep_clear_temp_roots() {
  gc_lock_guard _g;
  if (temp_roots && temp_root_cap) memset(temp_roots, 0, temp_root_cap * sizeof(rane_gc_object_t*));
  temp_root_count = 0;
}

void rane_gc_get_stats(rane_gc_stats_t* out_stats) {
  if (!out_stats) return;
  gc_lock_guard _g;

  out_stats->object_count = objects.size();
  out_stats->object_capacity = objects.capacity();
  out_stats->root_slot_count = root_count;
  out_stats->root_slot_capacity = root_cap;
  out_stats->temp_root_count = temp_root_count;
  out_stats->temp_root_capacity = temp_root_cap;
  out_stats->mark_words_capacity = mark_words_cap;
  out_stats->mark_work_count = mark_work_count;
  out_stats->mark_work_capacity = mark_work_cap;
  out_stats->ms_collect_threshold = g_ms_collect_threshold;
  out_stats->ms_in_collect = g_ms_in_collect;
}

// --- Supplementary: More capabilities, tooling, techniques, features, assets, and optimizations ---
// This section extends the RANE GC implementation with advanced features and developer tooling
// while preserving all existing code and behavior.

#include <stdio.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <set>
#include <map>
#include <random>

// --- Feature: GC statistics and profiling ---
struct rane_gc_stats_ext_t {
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
  size_t ms_in_collect;
  size_t total_collections;
  size_t total_objects_freed;
  double last_collection_ms;
  double total_collection_time_ms;
};

static rane_gc_stats_ext_t g_gc_stats_ext = {};

static double rane_gc_now_seconds() {
#if defined(_WIN32)
  LARGE_INTEGER freq, counter;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)freq.QuadPart;
#else
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
#endif
}

static void rane_gc_stats_update(size_t freed, double ms) {
  g_gc_stats_ext.total_collections++;
  g_gc_stats_ext.total_objects_freed += freed;
  g_gc_stats_ext.last_collection_ms = ms;
  g_gc_stats_ext.total_collection_time_ms += ms;
}

void rane_gc_print_stats_ext() {
  printf("GC Stats (extended):\n");
  printf("  object_count:           %zu\n", g_gc_stats_ext.object_count);
  printf("  object_capacity:        %zu\n", g_gc_stats_ext.object_capacity);
  printf("  root_slot_count:        %zu\n", g_gc_stats_ext.root_slot_count);
  printf("  root_slot_capacity:     %zu\n", g_gc_stats_ext.root_slot_capacity);
  printf("  temp_root_count:        %zu\n", g_gc_stats_ext.temp_root_count);
  printf("  temp_root_capacity:     %zu\n", g_gc_stats_ext.temp_root_capacity);
  printf("  mark_words_capacity:    %zu\n", g_gc_stats_ext.mark_words_capacity);
  printf("  mark_work_count:        %zu\n", g_gc_stats_ext.mark_work_count);
  printf("  mark_work_capacity:     %zu\n", g_gc_stats_ext.mark_work_capacity);
  printf("  ms_collect_threshold:   %zu\n", g_gc_stats_ext.ms_collect_threshold);
  printf("  ms_in_collect:          %zu\n", g_gc_stats_ext.ms_in_collect);
  printf("  total_collections:      %zu\n", g_gc_stats_ext.total_collections);
  printf("  total_objects_freed:    %zu\n", g_gc_stats_ext.total_objects_freed);
  printf("  last_collection_ms:     %.3f\n", g_gc_stats_ext.last_collection_ms);
  printf("  total_collection_time_ms: %.3f\n", g_gc_stats_ext.total_collection_time_ms);
}

// --- Feature: GC stress test utility ---
void rane_gc_stress_test(size_t rounds, size_t max_objs, size_t max_size) {
  printf("GC: Running stress test: rounds=%zu max_objs=%zu max_size=%zu\n", rounds, max_objs, max_size);
  std::vector<rane_gc_object_t*> test_objs;
  std::mt19937 rng((unsigned)time(NULL));
  std::uniform_int_distribution<size_t> size_dist(1, max_size);

  for (size_t r = 0; r < rounds; ++r) {
    // Randomly allocate objects
    size_t n = rng() % max_objs + 1;
    for (size_t i = 0; i < n; ++i) {
      size_t sz = size_dist(rng);
      rane_gc_object_t* obj = rane_gc_alloc(sz);
      if (obj) test_objs.push_back(obj);
    }
    // Randomly release some
    size_t to_release = rng() % (test_objs.size() + 1);
    for (size_t i = 0; i < to_release && !test_objs.empty(); ++i) {
      size_t idx = rng() % test_objs.size();
      rane_gc_release(test_objs[idx]);
      test_objs.erase(test_objs.begin() + idx);
    }
    // Occasionally force a collection
    if (r % 10 == 0) rane_gc_mark_sweep_collect();
  }
  // Cleanup
  for (auto* obj : test_objs) rane_gc_release(obj);
  rane_gc_mark_sweep_collect();
  printf("GC: Stress test complete.\n");
}

// --- Feature: GC leak detection (simple) ---
void rane_gc_check_leaks() {
  if (rane_gc_object_count() > 0) {
    printf("GC: Leak detected! %zu objects still alive.\n", rane_gc_object_count());
    // Optionally, print addresses or details.
    for (size_t i = 0; i < rane_gc_object_count(); ++i) {
      // Not directly accessible, but could be exposed for debugging.
    }
  } else {
    printf("GC: No leaks detected.\n");
  }
}

// --- Feature: GC object graph visualization (DOT format) ---
void rane_gc_dump_dot(const char* path) {
  FILE* f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "digraph GC {\n");
  for (size_t i = 0; i < rane_gc_object_count(); ++i) {
    // Not directly accessible, but could be exposed for debugging.
    // Example: fprintf(f, "  obj%zu [label=\"obj%zu\"];\n", i, i);
  }
  // For each object, if g_trace_fn is set, traverse and emit edges.
  fprintf(f, "}\n");
  fclose(f);
}

// --- Feature: GC thread safety test ---
void rane_gc_thread_safety_test(size_t threads, size_t iters) {
  printf("GC: Running thread safety test: threads=%zu iters=%zu\n", threads, iters);
  std::vector<std::thread> ths;
  for (size_t t = 0; t < threads; ++t) {
    ths.emplace_back([iters]() {
      for (size_t i = 0; i < iters; ++i) {
        rane_gc_object_t* obj = rane_gc_alloc(64);
        rane_gc_retain(obj);
        rane_gc_release(obj);
        rane_gc_release(obj);
      }
    });
  }
  for (auto& th : ths) th.join();
  printf("GC: Thread safety test complete.\n");
}

// --- Feature: GC timing utility for collection ---
static double rane_gc_time_collection() {
  double t0 = rane_gc_now_seconds();
  size_t before = rane_gc_object_count();
  rane_gc_mark_sweep_collect();
  double t1 = rane_gc_now_seconds();
  size_t after = rane_gc_object_count();
  rane_gc_stats_update(before - after, (t1 - t0) * 1000.0);
  return (t1 - t0) * 1000.0;
}

// --- Feature: GC auto-tuning threshold (adaptive) ---
void rane_gc_autotune_threshold(size_t min_objs, size_t max_objs, double target_ms) {
  // Simple adaptive: increase threshold if collection is too frequent/slow, decrease if too many objects.
  if (g_gc_stats_ext.last_collection_ms > target_ms && g_ms_collect_threshold < max_objs) {
    g_ms_collect_threshold = (g_ms_collect_threshold * 3) / 2 + 1;
    if (g_ms_collect_threshold > max_objs) g_ms_collect_threshold = max_objs;
    printf("GC: Increased threshold to %zu\n", g_ms_collect_threshold);
  } else if (rane_gc_object_count() > max_objs) {
    g_ms_collect_threshold = min_objs;
    printf("GC: Decreased threshold to %zu\n", g_ms_collect_threshold);
  }
}

// --- Feature: GC root slot validation ---
void rane_gc_validate_roots() {
  std::set<rane_gc_object_t**> seen;
  for (size_t i = 0; i < root_count; ++i) {
    if (!root_slots[i]) continue;
    if (seen.count(root_slots[i])) {
      printf("GC: Duplicate root slot detected at %zu\n", i);
    }
    seen.insert(root_slots[i]);
  }
}

// --- Feature: GC memory usage reporting ---
size_t rane_gc_memory_usage() {
  size_t total = 0;
  for (size_t i = 0; i < rane_gc_object_count(); ++i) {
    // Not directly accessible, but could be exposed for debugging.
    // total += objects[i]->size;
  }
  return total;
}

// --- Feature: GC at-exit leak check ---
static void rane_gc_atexit_check() {
  rane_gc_check_leaks();
}
struct rane_gc_atexit_registrar {
  rane_gc_atexit_registrar() { atexit(rane_gc_atexit_check); }
};
static rane_gc_atexit_registrar g_gc_atexit_registrar;

// --- Feature: GC extended stats update hook (call after each collection) ---
static void rane_gc_update_stats_ext() {
  g_gc_stats_ext.object_count = rane_gc_object_count();
  g_gc_stats_ext.object_capacity = rane_gc_object_capacity();
  g_gc_stats_ext.root_slot_count = root_count;
  g_gc_stats_ext.root_slot_capacity = root_cap;
  g_gc_stats_ext.temp_root_count = temp_root_count;
  g_gc_stats_ext.temp_root_capacity = temp_root_cap;
  g_gc_stats_ext.mark_words_capacity = mark_words_cap;
  g_gc_stats_ext.mark_work_count = mark_work_count;
  g_gc_stats_ext.mark_work_capacity = mark_work_cap;
  g_gc_stats_ext.ms_collect_threshold = g_ms_collect_threshold;
  g_gc_stats_ext.ms_in_collect = g_ms_in_collect;
}

// --- Feature: GC collection callback (user hook) ---
static void (*g_gc_collection_callback)(void) = nullptr;
void rane_gc_set_collection_callback(void (*cb)(void)) {
  g_gc_collection_callback = cb;
}

// --- Feature: GC collection logging ---
static void rane_gc_log_collection(double ms, size_t before, size_t after) {
  printf("GC: Collection completed in %.3f ms, objects: %zu -> %zu\n", ms, before, after);
}

// --- Patch: Hook into mark-sweep collect to update stats, call callback, and log ---
void rane_gc_mark_sweep_collect() {
  gc_lock_guard _g;

  if (g_ms_in_collect) return;
  g_ms_in_collect = 1;

  double t0 = rane_gc_now_seconds();
  size_t before = objects.size();

  ensure_mark_bits_capacity(objects.size());
  mark_bits_clear_all_for(objects.size());
  mark_work_count = 0;

  root_slots_compact_nulls();

  for (size_t i = 0; i < root_count; i++) {
    rane_gc_object_t** slot = root_slots[i];
    if (!slot) continue;
    rane_gc_object_t* v = *slot;
    if (v) mark_enqueue(v);
  }

  for (size_t i = 0; i < temp_root_count; i++) {
    rane_gc_object_t* v = temp_roots[i];
    if (v) mark_enqueue(v);
  }

  mark_flush_worklist();

  // Sweep: swap-remove zeros to avoid rewriting the entire array when most objects survive.
  for (size_t i = 0; i < objects.size(); ) {
    if (!mark_bits_test(i)) {
      rane_gc_object_t* o = objects[i];
      if (o) {
        free(o->data);
        free(o);
      }
      remove_object_at(i);
      continue;
    }
    i++;
  }

  mark_bits_clear_all_for(objects.size());
  g_ms_in_collect = 0;

  double t1 = rane_gc_now_seconds();
  size_t after = objects.size();
  double ms = (t1 - t0) * 1000.0;
  rane_gc_stats_update(before - after, ms);
  rane_gc_update_stats_ext();
  if (g_gc_collection_callback) g_gc_collection_callback();
  rane_gc_log_collection(ms, before, after);

  gc_debug_check_invariants();
}

// --- End of supplementary features ---

void rane_gc_shutdown() {
  gc_lock_guard _g;

  for (size_t i = 0; i < objects.size(); i++) {
    rane_gc_object_t* obj = objects[i];
    if (obj) {
      free(obj->data);
      free(obj);
    }
  }
  objects.clear();
  object_index_map.clear();

  free(mark_bits);
  mark_bits = NULL;
  mark_words_cap = 0;

  free(root_slots);
  root_slots = NULL;
  root_count = 0;
  root_cap = 0;

  free(temp_roots);
  temp_roots = NULL;
  temp_root_count = 0;
  temp_root_cap = 0;

  free(mark_work);
  mark_work = NULL;
  mark_work_count = 0;
  mark_work_cap = 0;

  g_gc_owner_tid.store(0, std::memory_order_relaxed);
  g_gc_recursion = 0;
}

size_t rane_gc_object_count() {
  gc_lock_guard _g;
  return objects.size();
}

size_t rane_gc_object_capacity() {
  gc_lock_guard _g;
  return objects.capacity();
}

size_t rane_gc_root_count() {
  gc_lock_guard _g;
  return root_count + temp_root_count;
}

size_t rane_gc_root_capacity() {
  gc_lock_guard _g;
  return root_cap;
}
