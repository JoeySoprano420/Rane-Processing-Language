#include "rane_thread.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

struct rane_task_item {
  void (*fn)(void*);
  void* arg;
};

struct rane_thread_pool_s {
  std::mutex mu;
  std::condition_variable cv_work;
  std::condition_variable cv_done;
  std::deque<rane_task_item> q;
  std::vector<std::thread> threads;
  std::atomic<uint8_t> stopping;
  size_t active; // guarded by mu
};

static void pool_worker(rane_thread_pool_t* p) {
  for (;;) {
    rane_task_item task = {};

    {
      std::unique_lock<std::mutex> lock(p->mu);
      p->cv_work.wait(lock, [&]() { return p->stopping.load(std::memory_order_relaxed) || !p->q.empty(); });

      if (p->stopping.load(std::memory_order_relaxed) && p->q.empty()) {
        return;
      }

      task = p->q.front();
      p->q.pop_front();
      p->active++;
    }

    if (task.fn) task.fn(task.arg);

    {
      std::lock_guard<std::mutex> lock(p->mu);
      if (p->active > 0) p->active--;
      if (p->q.empty() && p->active == 0) {
        p->cv_done.notify_all();
      }
    }
  }
}

rane_thread_pool_t* rane_thread_pool_create(size_t num_threads) {
  if (num_threads == 0) num_threads = 1;

  rane_thread_pool_t* p = new (std::nothrow) rane_thread_pool_t();
  if (!p) return NULL;

  p->stopping.store(0, std::memory_order_relaxed);
  p->active = 0;

  try {
    p->threads.reserve(num_threads);
    for (size_t i = 0; i < num_threads; i++) {
      p->threads.emplace_back([p]() { pool_worker(p); });
    }
  } catch (...) {
    rane_thread_pool_destroy(p);
    return NULL;
  }

  return p;
}

void rane_thread_pool_submit(rane_thread_pool_t* pool, void (*task)(void*), void* arg) {
  if (!pool || !task) return;

  {
    std::lock_guard<std::mutex> lock(pool->mu);
    if (pool->stopping.load(std::memory_order_relaxed)) return;
    pool->q.push_back({task, arg});
  }
  pool->cv_work.notify_one();
}

void rane_thread_pool_wait(rane_thread_pool_t* pool) {
  if (!pool) return;
  std::unique_lock<std::mutex> lock(pool->mu);
  pool->cv_done.wait(lock, [&]() { return pool->q.empty() && pool->active == 0; });
}

void rane_thread_pool_destroy(rane_thread_pool_t* pool) {
  if (!pool) return;

  {
    std::lock_guard<std::mutex> lock(pool->mu);
    pool->stopping.store(1, std::memory_order_relaxed);
  }
  pool->cv_work.notify_all();

  for (auto& t : pool->threads) {
    if (t.joinable()) t.join();
  }
  pool->threads.clear();

  delete pool;
}

struct rane_future_s {
  std::mutex mu;
  std::condition_variable cv;
  std::thread t;
  void* result;
  uint8_t done;
};

struct future_start_ctx {
  rane_future_t* f;
  void (*fn)(void*);
  void* arg;
};

static void future_runner(future_start_ctx* ctx) {
  rane_future_t* f = ctx->f;
  void (*fn)(void*) = ctx->fn;
  void* arg = ctx->arg;
  delete ctx;

  if (fn) fn(arg);

  {
    std::lock_guard<std::mutex> lock(f->mu);
    f->result = NULL;
    f->done = 1;
  }
  f->cv.notify_all();
}

rane_future_t* rane_async(void (*fn)(void*), void* arg) {
  if (!fn) return NULL;

  rane_future_t* f = new (std::nothrow) rane_future_t();
  if (!f) return NULL;

  f->result = NULL;
  f->done = 0;

  future_start_ctx* ctx = new (std::nothrow) future_start_ctx();
  if (!ctx) {
    delete f;
    return NULL;
  }
  ctx->f = f;
  ctx->fn = fn;
  ctx->arg = arg;

  try {
    f->t = std::thread([ctx]() { future_runner(ctx); });
  } catch (...) {
    delete ctx;
    delete f;
    return NULL;
  }

  return f;
}

void* rane_future_get(rane_future_t* f) {
  if (!f) return NULL;

  {
    std::unique_lock<std::mutex> lock(f->mu);
    f->cv.wait(lock, [&]() { return f->done != 0; });
  }

  if (f->t.joinable()) f->t.join();
  void* r = f->result;
  delete f;
  return r;
}
