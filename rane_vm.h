#pragma once

#include <stdint.h>

// Advanced lockable VM containers with user-defined lifetimes

typedef struct rane_vm_container_s {
  void* base;
  size_t size;
  uint32_t locks;
  uint64_t lifetime; // ticks or something
} rane_vm_container_t;

void rane_vm_container_create(rane_vm_container_t* cont, size_t size);
void rane_vm_container_lock(rane_vm_container_t* cont);
void rane_vm_container_unlock(rane_vm_container_t* cont);
void rane_vm_container_destroy(rane_vm_container_t* cont);