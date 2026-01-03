#include "rane_heap.h"
#include <stdlib.h>
#include <string.h>

void rane_heap_init(rane_heap_t* h, int (*cmp)(const void*, const void*)) {
  h->data = NULL;
  h->size = 0;
  h->capacity = 0;
  h->cmp = cmp;
}

static void heapify_up(rane_heap_t* h, size_t idx) {
  while (idx > 0) {
    size_t parent = (idx - 1) / 2;
    if (h->cmp(h->data[idx], h->data[parent]) >= 0) break;
    void* temp = h->data[idx];
    h->data[idx] = h->data[parent];
    h->data[parent] = temp;
    idx = parent;
  }
}

static void heapify_down(rane_heap_t* h, size_t idx) {
  size_t left = 2 * idx + 1;
  size_t right = 2 * idx + 2;
  size_t smallest = idx;
  if (left < h->size && h->cmp(h->data[left], h->data[smallest]) < 0) smallest = left;
  if (right < h->size && h->cmp(h->data[right], h->data[smallest]) < 0) smallest = right;
  if (smallest != idx) {
    void* temp = h->data[idx];
    h->data[idx] = h->data[smallest];
    h->data[smallest] = temp;
    heapify_down(h, smallest);
  }
}

void rane_heap_push(rane_heap_t* h, void* item) {
  if (h->size >= h->capacity) {
    h->capacity = h->capacity ? h->capacity * 2 : 16;
    h->data = (void**)realloc(h->data, h->capacity * sizeof(void*));
  }
  h->data[h->size++] = item;
  heapify_up(h, h->size - 1);
}

void* rane_heap_pop(rane_heap_t* h) {
  if (h->size == 0) return NULL;
  void* item = h->data[0];
  h->data[0] = h->data[--h->size];
  heapify_down(h, 0);
  return item;
}

void rane_heap_destroy(rane_heap_t* h) {
  free(h->data);
}