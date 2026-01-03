#include "rane_vm.h"
#include <stdlib.h>

void rane_vm_container_create(rane_vm_container_t* cont, size_t size) {
  cont->base = malloc(size);
  cont->size = size;
  cont->locks = 0;
  cont->lifetime = 0; // infinite
}

void rane_vm_container_lock(rane_vm_container_t* cont) {
  cont->locks++;
}

void rane_vm_container_unlock(rane_vm_container_t* cont) {
  if (cont->locks > 0) cont->locks--;
}

void rane_vm_container_destroy(rane_vm_container_t* cont) {
  if (cont->locks == 0) {
    free(cont->base);
  }
}