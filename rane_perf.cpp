#include "rane_perf.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#endif

// -----------------------------------------------------------------------------
// rane_perf.cpp
//
// Upgrades:
// - NOMINMAX: no Windows min/max macro breakage.
// - Exact QPC->ns conversion (no float).
// - Persistent worker pool with epoch-based job handoff.
// - No exception dependency (safe when /EHsc-).
// - parallel_for uses the pool (caller participates).
// - parallel_sort uses pool for BOTH sort-partition and merge passes.
// - Removes global sort ctx data race; per-call job context is passed safely.
// -----------------------------------------------------------------------------

struct rane_profiler_s {
  LARGE_INTEGER t0;
  LARGE_INTEGER t1;
  uint8_t stopped;
};

static LONGLONG rane_perf_qpc_freq() {
  static std::atomic<LONGLONG> g_freq(0);
  LONGLONG f = g_freq.load(std::memory_order_relaxed);
  if (f > 0) return f;

  LARGE_INTEGER li = {};
  if (!QueryPerformanceFrequency(&li) || li.QuadPart <= 0) li.QuadPart = 1;
  g_freq.store(li.QuadPart, std::memory_order_relaxed);
  return li.QuadPart;
}

static uint64_t rane_perf_muldiv_u64(uint64_t a, uint64_t b, uint64_t d) {
  if (d == 0) return 0;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
  uint64_t hi = 0;
  uint64_t lo = _umul128(a, b, &hi);
  return _udiv128(hi, lo, d, NULL);
#else
  // Fallback: (a*b)/d ~= (a/d)*b + ((a%d)*b)/d with chunking
  uint64_t q = a / d;
  uint64_t r = a % d;
  uint64_t acc = q * b;

  while (r != 0) {
    uint64_t step = (b == 0) ? r : ((r > (UINT64_MAX / b)) ? (UINT64_MAX / b) : r);
    acc += (step * b) / d;
    r -= step;
  }
  return acc;
#endif
}

static uint64_t rane_perf_qpc_to_ns(LONGLONG dt) {
  if (dt <= 0) return 0;
  const uint64_t freq = (uint64_t)rane_perf_qpc_freq();
  if (freq == 0) return 0;
  return rane_perf_muldiv_u64((uint64_t)dt, 1000000000ull, freq);
}

rane_profiler_t* rane_profiler_start() {
  rane_profiler_t* p = (rane_profiler_t*)malloc(sizeof(rane_profiler_t));
  if (!p) return NULL;
  memset(p, 0, sizeof(*p));

  QueryPerformanceCounter(&p->t0);
  p->t1 = p->t0;
  p->stopped = 0;
  return p;
}

void rane_profiler_stop(rane_profiler_t* p) {
  if (!p) return;
  if (p->stopped) return;
  QueryPerformanceCounter(&p->t1);
  p->stopped = 1;
}

uint64_t rane_profiler_elapsed_ns(rane_profiler_t* p) {
  if (!p) return 0;

  LARGE_INTEGER now = {};
  if (p->stopped) now = p->t1;
  else QueryPerformanceCounter(&now);

  LONGLONG dt = now.QuadPart - p->t0.QuadPart;
  if (dt < 0) dt = 0;
  return rane_perf_qpc_to_ns(dt);
}

// -----------------------------------------------------------------------------
// Worker pool
// -----------------------------------------------------------------------------

static size_t rane_perf_hw_threads() {
  unsigned n = std::thread::hardware_concurrency();
  if (n == 0) n = 1;
  if (n > 64) n = 64;
  return (size_t)n;
}

static size_t rane_perf_pick_grain(size_t work_items, size_t threads) {
  if (threads == 0) threads = 1;

  // ~8 chunks per worker tends to amortize atomic ops well.
  size_t chunks = threads * 8;
  if (chunks == 0) chunks = 1;

  size_t g = work_items / chunks;
  if (g < 64) g = 64;
  if (g > 8192) g = 8192;
  return g;
}

typedef void (*rane_for_fn_t)(size_t i, void* ctx);

struct rane_pool_job {
  size_t start;
  size_t end;
  size_t grain;

  rane_for_fn_t fn;
  void* ctx;

  std::atomic_size_t next;
  std::atomic_uint active_workers;
  std::atomic_uint done; // 0/1
};

struct rane_worker_pool {
  std::mutex mu;
  std::condition_variable cv_work;
  std::condition_variable cv_done;

  std::vector<std::thread> threads;
  std::atomic<uint8_t> stopping;

  // Epoch-based publication to avoid missed wakeups/spurious reuse.
  uint64_t job_epoch;     // guarded by mu
  rane_pool_job* job;     // guarded by mu (lifetime: during run)

  rane_worker_pool() : stopping(0), job_epoch(0), job(NULL) {}
};

static rane_worker_pool* g_pool = NULL;

static void rane_pool_worker_main(rane_worker_pool* p) {
  uint64_t seen_epoch = 0;

  for (;;) {
    rane_pool_job* j = NULL;

    {
      std::unique_lock<std::mutex> lock(p->mu);
      p->cv_work.wait(lock, [&]() {
        return p->stopping.load(std::memory_order_relaxed) || p->job_epoch != seen_epoch;
      });

      if (p->stopping.load(std::memory_order_relaxed)) return;

      seen_epoch = p->job_epoch;
      j = p->job;
      if (!j) continue;

      j->active_workers.fetch_add(1, std::memory_order_relaxed);
    }

    for (;;) {
      size_t base = j->next.fetch_add(j->grain, std::memory_order_relaxed);
      if (base >= j->end) break;

      size_t lim = base + j->grain;
      if (lim > j->end) lim = j->end;

      for (size_t i = base; i < lim; i++) {
        j->fn(i, j->ctx);
      }
    }

    unsigned left = j->active_workers.fetch_sub(1, std::memory_order_relaxed) - 1;
    if (left == 0) {
      j->done.store(1, std::memory_order_relaxed);
      std::lock_guard<std::mutex> lock(p->mu);
      p->cv_done.notify_all();
    }
  }
}

static void rane_pool_shutdown() {
  rane_worker_pool* p = g_pool;
  if (!p) return;

  {
    std::lock_guard<std::mutex> lock(p->mu);
    p->stopping.store(1, std::memory_order_relaxed);
    p->job = NULL;
    p->job_epoch++;
  }
  p->cv_work.notify_all();

  for (auto& t : p->threads) {
    if (t.joinable()) t.join();
  }
  p->threads.clear();

  delete p;
  g_pool = NULL;
}

static rane_worker_pool* rane_pool_get() {
  if (g_pool) return g_pool;

  rane_worker_pool* p = new (std::nothrow) rane_worker_pool();
  if (!p) return NULL;

  size_t n = rane_perf_hw_threads();
  if (n < 1) n = 1;

  // Leave one "slot" for caller participation.
  size_t worker_threads = (n > 1) ? (n - 1) : 0;

  p->threads.reserve(worker_threads);
  for (size_t i = 0; i < worker_threads; i++) {
    // NOTE: std::thread can throw if exceptions are enabled; if exceptions are disabled,
    // this will terminate on failure. This project already relies on std::thread elsewhere.
    p->threads.emplace_back([p]() { rane_pool_worker_main(p); });
  }

  g_pool = p;
  atexit(rane_pool_shutdown);
  return g_pool;
}

static void rane_pool_run_ex(size_t start, size_t end, rane_for_fn_t fn, void* ctx) {
  if (!fn) return;
  if (end <= start) return;

  const size_t count = end - start;
  const size_t hw = rane_perf_hw_threads();

  if (hw <= 1 || count < 4096) {
    for (size_t i = start; i < end; i++) fn(i, ctx);
    return;
  }

  rane_worker_pool* p = rane_pool_get();
  if (!p || p->threads.empty()) {
    for (size_t i = start; i < end; i++) fn(i, ctx);
    return;
  }

  rane_pool_job job = {};
  job.start = start;
  job.end = end;
  job.grain = rane_perf_pick_grain(count, hw);
  job.fn = fn;
  job.ctx = ctx;
  job.next.store(start, std::memory_order_relaxed);
  job.active_workers.store(0, std::memory_order_relaxed);
  job.done.store(0, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lock(p->mu);
    p->job = &job;
    p->job_epoch++;
  }
  p->cv_work.notify_all();

  // Caller participates
  for (;;) {
    size_t base = job.next.fetch_add(job.grain, std::memory_order_relaxed);
    if (base >= end) break;

    size_t lim = base + job.grain;
    if (lim > end) lim = end;

    for (size_t i = base; i < lim; i++) fn(i, ctx);
  }

  {
    std::unique_lock<std::mutex> lock(p->mu);
    p->cv_done.wait(lock, [&]() { return job.done.load(std::memory_order_relaxed) != 0; });
    p->job = NULL;
  }
}

// -----------------------------------------------------------------------------
// Public parallel_for (API unchanged)
// -----------------------------------------------------------------------------

struct rane_pf_ctx {
  void (*fn)(size_t);
};

static void rane_pf_thunk(size_t i, void* ctx) {
  rane_pf_ctx* c = (rane_pf_ctx*)ctx;
  c->fn(i);
}

void rane_parallel_for(size_t start, size_t end, void (*fn)(size_t)) {
  if (!fn) return;
  rane_pf_ctx c = {};
  c.fn = fn;
  rane_pool_run_ex(start, end, rane_pf_thunk, &c);
}

// -----------------------------------------------------------------------------
// Parallel sort (pool-backed) with parallel merge passes
// -----------------------------------------------------------------------------

static void rane_sort_seq(int* arr, size_t n) {
  if (!arr || n <= 1) return;
  std::sort(arr, arr + n);
}

static void rane_merge_runs(int* arr, int* tmp, size_t a0, size_t a1, size_t b0, size_t b1) {
  size_t pa = a0;
  size_t pb = b0;
  size_t pt = a0;

  while (pa < a1 && pb < b1) {
    int va = arr[pa];
    int vb = arr[pb];
    if (va <= vb) { tmp[pt++] = va; pa++; }
    else { tmp[pt++] = vb; pb++; }
  }
  while (pa < a1) tmp[pt++] = arr[pa++];
  while (pb < b1) tmp[pt++] = arr[pb++];

  memcpy(arr + a0, tmp + a0, (b1 - a0) * sizeof(int));
}

struct rane_ps_sort_ctx {
  int* arr;
  const size_t* off;
  size_t parts;
};

static void rane_ps_sort_task(size_t i, void* ctx) {
  rane_ps_sort_ctx* c = (rane_ps_sort_ctx*)ctx;
  if (i >= c->parts) return;

  size_t b = c->off[i];
  size_t e = c->off[i + 1];
  if (e > b) std::sort(c->arr + b, c->arr + e);
}

struct rane_ps_merge_ctx {
  int* arr;
  int* tmp;
  const size_t* off;
  size_t parts;
  size_t width;
};

static void rane_ps_merge_task(size_t pair_index, void* ctx) {
  rane_ps_merge_ctx* c = (rane_ps_merge_ctx*)ctx;

  // Each task handles one merge pair: i = pair_index * (2*width)
  size_t i = pair_index * (2 * c->width);
  if (i >= c->parts) return;

  size_t left_i = i;
  size_t mid_i = (i + c->width < c->parts) ? (i + c->width) : c->parts;
  size_t right_i = (i + 2 * c->width < c->parts) ? (i + 2 * c->width) : c->parts;

  size_t a0 = c->off[left_i];
  size_t a1 = c->off[mid_i];
  size_t b0 = c->off[mid_i];
  size_t b1 = c->off[right_i];

  if (a1 == a0 || b1 == b0) return;
  rane_merge_runs(c->arr, c->tmp, a0, a1, b0, b1);
}

void rane_parallel_sort(int* arr, size_t n) {
  if (!arr || n <= 1) return;

  const size_t hw = rane_perf_hw_threads();
  if (hw <= 1 || n < (1u << 16)) {
    rane_sort_seq(arr, n);
    return;
  }

  size_t parts = hw;
  if (parts > n) parts = n;
  if (parts < 2) {
    rane_sort_seq(arr, n);
    return;
  }

  std::vector<size_t> off(parts + 1, 0);
  for (size_t i = 0; i <= parts; i++) off[i] = (n * i) / parts;

  // Sort partitions in parallel
  rane_ps_sort_ctx sctx = {};
  sctx.arr = arr;
  sctx.off = off.data();
  sctx.parts = parts;
  rane_pool_run_ex(0, parts, rane_ps_sort_task, &sctx);

  int* tmp = (int*)malloc(n * sizeof(int));
  if (!tmp) {
    rane_sort_seq(arr, n);
    return;
  }

  // Parallel merge passes: each pass merges disjoint ranges, safe to parallelize.
  for (size_t width = 1; width < parts; width *= 2) {
    size_t pairs = (parts + (2 * width) - 1) / (2 * width);

    rane_ps_merge_ctx mctx = {};
    mctx.arr = arr;
    mctx.tmp = tmp;
    mctx.off = off.data();
    mctx.parts = parts;
    mctx.width = width;

    rane_pool_run_ex(0, pairs, rane_ps_merge_task, &mctx);
  }

  free(tmp);
}
