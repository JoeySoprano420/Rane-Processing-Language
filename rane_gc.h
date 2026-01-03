#pragma once

// Basic reference counting GC for RANE

typedef struct rane_gc_object_s {
  int ref_count;
  void* data;
  size_t size;
} rane_gc_object_t;

void rane_gc_init();
rane_gc_object_t* rane_gc_alloc(size_t size);
void rane_gc_retain(rane_gc_object_t* obj);
void rane_gc_release(rane_gc_object_t* obj);
void rane_gc_collect();

// Mark-sweep GC
void rane_gc_mark_sweep_init();
void rane_gc_mark_sweep_collect();