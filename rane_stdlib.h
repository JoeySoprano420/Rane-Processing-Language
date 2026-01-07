#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <cstdint>

#include "rane_common.h"
#include "rane_graph.h"

// Standard Library for RANE
// Includes I/O, math, containers, sorting, concurrency

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

rane_thread_t* rane_thread_create(void (*fn)(void*), void* arg);
void rane_thread_join(rane_thread_t* t);

void rane_mutex_init(rane_mutex_t* m);
void rane_mutex_lock(rane_mutex_t* m);
void rane_mutex_unlock(rane_mutex_t* m);

void rane_channel_init(rane_channel_t* c);
void rane_channel_send(rane_channel_t* c, void* data);
void* rane_channel_recv(rane_channel_t* c);

rane_barrier_t* rane_barrier_create(size_t count);
void rane_barrier_wait(rane_barrier_t* b);
void rane_barrier_destroy(rane_barrier_t* b);

// Algorithms
// Dijkstra's shortest path
void rane_dijkstra(rane_graph_t* g, uint64_t start, uint64_t* dist, uint64_t* prev);
