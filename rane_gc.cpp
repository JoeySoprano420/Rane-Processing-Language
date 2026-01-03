#include "rane_gc.h"
#include <stdlib.h>
#include <string.h>

#define MAX_OBJECTS 1024
static rane_gc_object_t* objects[MAX_OBJECTS];
static size_t object_count = 0;

void rane_gc_init() {
  memset(objects, 0, sizeof(objects));
  object_count = 0;
}

rane_gc_object_t* rane_gc_alloc(size_t size) {
  if (object_count >= MAX_OBJECTS) return NULL;
  rane_gc_object_t* obj = (rane_gc_object_t*)malloc(sizeof(rane_gc_object_t));
  obj->ref_count = 1;
  obj->data = malloc(size);
  obj->size = size;
  objects[object_count++] = obj;
  return obj;
}

void rane_gc_retain(rane_gc_object_t* obj) {
  if (obj) obj->ref_count++;
}

void rane_gc_release(rane_gc_object_t* obj) {
  if (!obj) return;
  obj->ref_count--;
  if (obj->ref_count <= 0) {
    free(obj->data);
    free(obj);
    // Remove from objects array (simplified)
  }
}

void rane_gc_collect() {
  // Simple sweep
  for (size_t i = 0; i < object_count; i++) {
    if (objects[i] && objects[i]->ref_count <= 0) {
      free(objects[i]->data);
      free(objects[i]);
      objects[i] = NULL;
    }
  }
}

void rane_gc_mark_sweep_init() {
  // Initialize mark-sweep GC
}

void rane_gc_mark_sweep_collect() {
  // Mark and sweep
}