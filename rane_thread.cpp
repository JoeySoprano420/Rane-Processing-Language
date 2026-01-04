#include "rane_thread.h"
#include <cstddef>

rane_thread_pool_t* rane_thread_pool_create(size_t num_threads) {
  return NULL;
}

void rane_thread_pool_submit(rane_thread_pool_t* pool, void (*task)(void*), void* arg) {
}

void rane_thread_pool_wait(rane_thread_pool_t* pool) {
}

void rane_thread_pool_destroy(rane_thread_pool_t* pool) {
}

rane_future_t* rane_async(void (*fn)(void*), void* arg) {
  return NULL;
}

void* rane_future_get(rane_future_t* f) {
  return NULL;
}
