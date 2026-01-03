#include "rane_hashmap.h"
#include <stdlib.h>
#include <string.h>

#define HASH_SEED 5381

static size_t hash_uint64(uint64_t key) {
  // Simple hash
  return (size_t)(key % HASH_SEED);
}

void rane_hashmap_init(rane_hashmap_t* hm, size_t initial_capacity) {
  hm->bucket_count = initial_capacity;
  hm->buckets = (rane_hashmap_entry_t**)calloc(initial_capacity, sizeof(rane_hashmap_entry_t*));
  hm->size = 0;
}

void rane_hashmap_put(rane_hashmap_t* hm, uint64_t key, void* value) {
  size_t idx = hash_uint64(key) % hm->bucket_count;
  rane_hashmap_entry_t* entry = hm->buckets[idx];
  while (entry) {
    if (entry->key == key) {
      entry->value = value;
      return;
    }
    entry = entry->next;
  }
  // New entry
  entry = (rane_hashmap_entry_t*)malloc(sizeof(rane_hashmap_entry_t));
  entry->key = key;
  entry->value = value;
  entry->next = hm->buckets[idx];
  hm->buckets[idx] = entry;
  hm->size++;
}

void* rane_hashmap_get(rane_hashmap_t* hm, uint64_t key) {
  size_t idx = hash_uint64(key) % hm->bucket_count;
  rane_hashmap_entry_t* entry = hm->buckets[idx];
  while (entry) {
    if (entry->key == key) return entry->value;
    entry = entry->next;
  }
  return NULL;
}

void rane_hashmap_remove(rane_hashmap_t* hm, uint64_t key) {
  size_t idx = hash_uint64(key) % hm->bucket_count;
  rane_hashmap_entry_t* entry = hm->buckets[idx];
  rane_hashmap_entry_t* prev = NULL;
  while (entry) {
    if (entry->key == key) {
      if (prev) prev->next = entry->next;
      else hm->buckets[idx] = entry->next;
      free(entry);
      hm->size--;
      return;
    }
    prev = entry;
    entry = entry->next;
  }
}

void rane_hashmap_destroy(rane_hashmap_t* hm) {
  for (size_t i = 0; i < hm->bucket_count; i++) {
    rane_hashmap_entry_t* entry = hm->buckets[i];
    while (entry) {
      rane_hashmap_entry_t* next = entry->next;
      free(entry);
      entry = next;
    }
  }
  free(hm->buckets);
}