#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <cstdint>

#include "rane_common.h"
#include "rane_graph.h"

// Standard Library for RANE
// Includes I/O, math, containers, sorting, concurrency, DSP

// I/O
void rane_print(const char* str);
int rane_read_int();

// Math
int rane_abs(int x);
double rane_sqrt(double x);
double rane_sin(double x);
double rane_cos(double x);
double rane_pow(double base, double exp);

// Containers
typedef struct rane_vector_s {
  void** data;
  size_t size;
  size_t capacity;
} rane_vector_t;

void rane_vector_init(rane_vector_t* v);
void rane_vector_free(rane_vector_t* v);
void rane_vector_clear(rane_vector_t* v);

void rane_vector_reserve(rane_vector_t* v, size_t capacity);
void rane_vector_push(rane_vector_t* v, void* item);
void* rane_vector_pop(rane_vector_t* v);

void* rane_vector_get(const rane_vector_t* v, size_t index);
void rane_vector_set(rane_vector_t* v, size_t index, void* item);

void rane_vector_insert(rane_vector_t* v, size_t index, void* item);
void* rane_vector_remove_at(rane_vector_t* v, size_t index);

// Sorting
void rane_sort_int(int* arr, size_t n);
void rane_sort_double(double* arr, size_t n);

// Searching
int rane_binary_search_int(const int* arr, size_t n, int key);

// String
size_t rane_strlen(const char* s);
int rane_strcmp(const char* s1, const char* s2);
char* rane_strcpy(char* dest, const char* src);
char* rane_strcat(char* dest, const char* src);

// Memory
void* rane_memcpy(void* dest, const void* src, size_t n);
void* rane_memset(void* s, int c, size_t n);
int rane_memcmp(const void* s1, const void* s2, size_t n);

// Concurrency
typedef struct rane_thread_s rane_thread_t;
typedef struct rane_mutex_s rane_mutex_t;
typedef struct rane_channel_s rane_channel_t;
typedef struct rane_barrier_s rane_barrier_t;

// New: rwlock / semaphore / once / atomics
typedef struct rane_rwlock_s rane_rwlock_t;
typedef struct rane_semaphore_s rane_semaphore_t;
typedef struct rane_once_s rane_once_t;

rane_thread_t* rane_thread_create(void (*fn)(void*), void* arg);
void rane_thread_join(rane_thread_t* t);

void rane_mutex_init(rane_mutex_t* m);
void rane_mutex_lock(rane_mutex_t* m);
void rane_mutex_unlock(rane_mutex_t* m);

// Channels
void rane_channel_init(rane_channel_t* c);
void rane_channel_init_bounded(rane_channel_t* c, size_t capacity);
void rane_channel_send(rane_channel_t* c, void* data);
void* rane_channel_recv(rane_channel_t* c);
void* rane_channel_recv_timeout_ms(rane_channel_t* c, uint32_t timeout_ms);

// Barrier
rane_barrier_t* rane_barrier_create(size_t count);
void rane_barrier_wait(rane_barrier_t* b);
void rane_barrier_destroy(rane_barrier_t* b);

// RWLock (SRWLOCK)
rane_rwlock_t* rane_rwlock_create(void);
void rane_rwlock_destroy(rane_rwlock_t* l);
void rane_rwlock_read_lock(rane_rwlock_t* l);
void rane_rwlock_read_unlock(rane_rwlock_t* l);
void rane_rwlock_write_lock(rane_rwlock_t* l);
void rane_rwlock_write_unlock(rane_rwlock_t* l);

// Semaphore
rane_semaphore_t* rane_semaphore_create(int32_t initial_count, int32_t max_count);
void rane_semaphore_destroy(rane_semaphore_t* s);
void rane_semaphore_release(rane_semaphore_t* s, int32_t release_count);
int rane_semaphore_wait_ms(rane_semaphore_t* s, uint32_t timeout_ms);

// Once init
void rane_once_init(rane_once_t* o);
void rane_once_do(rane_once_t* o, void (*fn)(void*), void* arg);

// Atomics
uint32_t rane_atomic_load_u32(volatile uint32_t* p);
void rane_atomic_store_u32(volatile uint32_t* p, uint32_t v);
uint32_t rane_atomic_add_u32(volatile uint32_t* p, uint32_t v);
uint32_t rane_atomic_cas_u32(volatile uint32_t* p, uint32_t expected, uint32_t desired);

uint64_t rane_atomic_load_u64(volatile uint64_t* p);
void rane_atomic_store_u64(volatile uint64_t* p, uint64_t v);
uint64_t rane_atomic_add_u64(volatile uint64_t* p, uint64_t v);
uint64_t rane_atomic_cas_u64(volatile uint64_t* p, uint64_t expected, uint64_t desired);

// Algorithms
void rane_dijkstra(rane_graph_t* g, uint64_t start, uint64_t* dist, uint64_t* prev);

// -----------------------------------------------------------------------------
// DSP / Audio utilities (pure functions; no WASAPI here yet)
// -----------------------------------------------------------------------------

// Windows (in-place multiply)
void rane_dsp_window_hann_f32(float* dst, const float* src, size_t n);
void rane_dsp_window_hamming_f32(float* dst, const float* src, size_t n);
void rane_dsp_window_blackman_f32(float* dst, const float* src, size_t n);

// Measurement
float rane_dsp_rms_f32(const float* x, size_t n);
float rane_dsp_peak_f32(const float* x, size_t n);

// Scaling / mixing
void rane_dsp_normalize_f32(float* x, size_t n, float peak_target);
void rane_dsp_mix_f32(float* dst, const float* src, size_t n, float gain);

// Biquad
typedef struct rane_biquad_f32_s {
  float b0, b1, b2;
  float a1, a2;
  float z1, z2;
} rane_biquad_f32_t;

void rane_dsp_biquad_reset_f32(rane_biquad_f32_t* b);
void rane_dsp_biquad_init_lpf_f32(rane_biquad_f32_t* b, float sample_rate_hz, float cutoff_hz, float q);
void rane_dsp_biquad_init_hpf_f32(rane_biquad_f32_t* b, float sample_rate_hz, float cutoff_hz, float q);
void rane_dsp_biquad_process_inplace_f32(rane_biquad_f32_t* b, float* x, size_t n);

// FFT (complex, in-place), radix-2. `inverse!=0` performs IFFT (normalized by 1/n).
void rane_dsp_fft_cplx_f32(float* re, float* im, size_t n, int inverse);
