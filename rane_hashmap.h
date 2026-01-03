#pragma once

#include <stdint.h>

// Simple hash map for RANE stdlib
// Key: uint64_t, Value: void*

typedef struct rane_hashmap_entry_s {
  uint64_t key;
  void* value;
  struct rane_hashmap_entry_s* next;
} rane_hashmap_entry_t;

typedef struct rane_hashmap_s {
  rane_hashmap_entry_t** buckets;
  size_t bucket_count;
  size_t size;
} rane_hashmap_t;

void rane_hashmap_init(rane_hashmap_t* hm, size_t initial_capacity);
void rane_hashmap_put(rane_hashmap_t* hm, uint64_t key, void* value);
void* rane_hashmap_get(rane_hashmap_t* hm, uint64_t key);
void rane_hashmap_remove(rane_hashmap_t* hm, uint64_t key);
void rane_hashmap_destroy(rane_hashmap_t* hm);