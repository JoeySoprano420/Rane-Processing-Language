#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Advanced Threading Library for RANE

typedef struct rane_thread_pool_s rane_thread_pool_t;

rane_thread_pool_t* rane_thread_pool_create(size_t num_threads);
void rane_thread_pool_submit(rane_thread_pool_t* pool, void (*task)(void*), void* arg);
void rane_thread_pool_wait(rane_thread_pool_t* pool);
void rane_thread_pool_destroy(rane_thread_pool_t* pool);

// Futures
typedef struct rane_future_s rane_future_t;

rane_future_t* rane_async(void (*fn)(void*), void* arg);
void* rane_future_get(rane_future_t* f);

#ifdef __cplusplus
} // extern "C"
#endif