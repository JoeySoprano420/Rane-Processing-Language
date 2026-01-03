#pragma once

#include <stdint.h>

// Simple binary search tree for RANE stdlib
// Key: uint64_t, Value: void*

typedef struct rane_bst_node_s {
  uint64_t key;
  void* value;
  struct rane_bst_node_s* left;
  struct rane_bst_node_s* right;
} rane_bst_node_t;

typedef struct rane_bst_s {
  rane_bst_node_t* root;
  size_t size;
} rane_bst_t;

void rane_bst_init(rane_bst_t* bst);
void rane_bst_insert(rane_bst_t* bst, uint64_t key, void* value);
void* rane_bst_find(rane_bst_t* bst, uint64_t key);
void rane_bst_remove(rane_bst_t* bst, uint64_t key);
void rane_bst_destroy(rane_bst_t* bst);