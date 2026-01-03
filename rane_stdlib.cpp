#include "rane_stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

// I/O
void rane_print(const char* str) {
  printf("%s", str);
}

int rane_read_int() {
  int x;
  scanf("%d", &x);
  return x;
}

// Math
int rane_abs(int x) {
  return abs(x);
}

double rane_sqrt(double x) {
  return sqrt(x);
}

double rane_sin(double x) {
  return sin(x);
}

double rane_cos(double x) {
  return cos(x);
}

double rane_pow(double base, double exp) {
  return pow(base, exp);
}

// String
size_t rane_strlen(const char* s) {
  return strlen(s);
}

int rane_strcmp(const char* s1, const char* s2) {
  return strcmp(s1, s2);
}

char* rane_strcpy(char* dest, const char* src) {
  return strcpy(dest, src);
}

char* rane_strcat(char* dest, const char* src) {
  return strcat(dest, src);
}

// Memory
void* rane_memcpy(void* dest, const void* src, size_t n) {
  return memcpy(dest, src, n);
}

void* rane_memset(void* s, int c, size_t n) {
  return memset(s, c, n);
}

int rane_memcmp(const void* s1, const void* s2, size_t n) {
  return memcmp(s1, s2, n);
}

// Containers
void rane_vector_init(rane_vector_t* v) {
  v->data = NULL;
  v->size = 0;
}

void rane_vector_push(rane_vector_t* v, void* item) {
  // Stub
}

// Sorting
void rane_sort_int(int* arr, size_t n) {
  // Bubble sort for simplicity
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        int temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
}

void rane_sort_double(double* arr, size_t n) {
  // Similar
}

// Binary search
int rane_binary_search_int(const int* arr, size_t n, int key) {
  size_t low = 0, high = n;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (arr[mid] < key) low = mid + 1;
    else high = mid;
  }
  if (low < n && arr[low] == key) return (int)low;
  return -1;
}

// Dijkstra's shortest path
void rane_dijkstra(rane_graph_t* g, uint64_t start, uint64_t* dist, uint64_t* prev) {
  // Stub: initialize dist to INF, prev to -1, use heap for priority
  for (size_t i = 0; i < g->node_count; i++) {
    dist[i] = UINT64_MAX;
    prev[i] = UINT64_MAX;
  }
  dist[0] = 0; // assume start is 0
  // Use heap to process
  // For simplicity, stub
}

// Concurrency (stubs)
rane_thread_t* rane_thread_create(void (*fn)(void*), void* arg) {
  return NULL;
}

void rane_thread_join(rane_thread_t* t) {
}

void rane_mutex_init(rane_mutex_t* m) {
}

void rane_mutex_lock(rane_mutex_t* m) {
}

void rane_mutex_unlock(rane_mutex_t* m) {
}

void rane_channel_init(rane_channel_t* c) {
}

void rane_channel_send(rane_channel_t* c, void* data) {
}

void* rane_channel_recv(rane_channel_t* c) {
  return NULL;
}

// Barrier
rane_barrier_t* rane_barrier_create(size_t count) {
  return NULL;
}

void rane_barrier_wait(rane_barrier_t* b) {
}

void rane_barrier_destroy(rane_barrier_t* b) {
}