#include "rane_bst.h"
#include <stdlib.h>

void rane_bst_init(rane_bst_t* bst) {
  bst->root = NULL;
  bst->size = 0;
}

static rane_bst_node_t* create_node(uint64_t key, void* value) {
  rane_bst_node_t* node = (rane_bst_node_t*)malloc(sizeof(rane_bst_node_t));
  node->key = key;
  node->value = value;
  node->left = node->right = NULL;
  return node;
}

void rane_bst_insert(rane_bst_t* bst, uint64_t key, void* value) {
  rane_bst_node_t** node = &bst->root;
  while (*node) {
    if (key < (*node)->key) node = &(*node)->left;
    else if (key > (*node)->key) node = &(*node)->right;
    else {
      (*node)->value = value; // Update
      return;
    }
  }
  *node = create_node(key, value);
  bst->size++;
}

void* rane_bst_find(rane_bst_t* bst, uint64_t key) {
  rane_bst_node_t* node = bst->root;
  while (node) {
    if (key < node->key) node = node->left;
    else if (key > node->key) node = node->right;
    else return node->value;
  }
  return NULL;
}

void rane_bst_remove(rane_bst_t* bst, uint64_t key) {
  // Simplified: not handling all cases
  rane_bst_node_t** node = &bst->root;
  while (*node) {
    if (key < (*node)->key) node = &(*node)->left;
    else if (key > (*node)->key) node = &(*node)->right;
    else {
      // Remove
      free(*node);
      *node = NULL; // Simplified
      bst->size--;
      return;
    }
  }
}

static void destroy_node(rane_bst_node_t* node) {
  if (!node) return;
  destroy_node(node->left);
  destroy_node(node->right);
  free(node);
}

void rane_bst_destroy(rane_bst_t* bst) {
  destroy_node(bst->root);
  bst->root = NULL;
  bst->size = 0;
}