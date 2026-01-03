#pragma once

#include <stdint.h>

// Heap (priority queue) for RANE stdlib
typedef struct rane_heap_s {
  void** data;
  size_t size;
  size_t capacity;
  int (*cmp)(const void*, const void*); // comparison function
} rane_heap_t;

void rane_heap_init(rane_heap_t* h, int (*cmp)(const void*, const void*));
void rane_heap_push(rane_heap_t* h, void* item);
void* rane_heap_pop(rane_heap_t* h);
void rane_heap_destroy(rane_heap_t* h);