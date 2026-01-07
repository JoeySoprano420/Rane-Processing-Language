#include "rane_vm.h"

#include <stdlib.h>
#include <string.h>

static size_t rane_vm_align_up_size(size_t v, size_t a) {
  return (v + (a - 1)) & ~(a - 1);
}

static int rane_vm_add_overflow_size(size_t a, size_t b, size_t* out) {
  if (!out) return 1;
  if (SIZE_MAX - a < b) return 1;
  *out = a + b;
  return 0;
}

static int rane_vm_container_ensure_owned(rane_vm_container_t* cont) {
  if (!cont) return 0;
  if (cont->base && (cont->flags & RANE_VM_CONT_F_OWNED) == 0) return 0;
  return 1;
}

void rane_vm_container_init(rane_vm_container_t* cont) {
  if (!cont) return;
  memset(cont, 0, sizeof(*cont));
}

int rane_vm_container_is_valid(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  if (cont->size > cont->capacity) return 0;
  if (cont->capacity != 0 && cont->base == NULL) return 0;
  return 1;
}

int rane_vm_container_create(rane_vm_container_t* cont, size_t size) {
  return rane_vm_container_create_with_lifetime(cont, size, 0);
}

int rane_vm_container_create_with_lifetime(rane_vm_container_t* cont, size_t size, uint64_t lifetime_ticks) {
  if (!cont) return 0;

  // If reusing an existing container, destroy first (respect locks).
  // Deterministic: if locked, refuse to re-create.
  if (cont->base != NULL || cont->capacity != 0 || cont->size != 0) {
    if (cont->locks != 0) return 0;
    rane_vm_container_destroy_force(cont);
  }

  rane_vm_container_init(cont);

  cont->lifetime = lifetime_ticks;
  cont->flags = RANE_VM_CONT_F_NONE;

  if (size == 0) {
    // Empty valid container.
    return 1;
  }

  size_t cap = rane_vm_align_up_size(size, 16);
  void* mem = malloc(cap);
  if (!mem) return 0;

  memset(mem, 0, cap);

  cont->base = mem;
  cont->size = size;
  cont->capacity = cap;
  cont->locks = 0;
  cont->flags |= RANE_VM_CONT_F_OWNED;

  return 1;
}

void rane_vm_container_lock(rane_vm_container_t* cont) {
  if (!cont) return;
  cont->locks++;
}

void rane_vm_container_unlock(rane_vm_container_t* cont) {
  if (!cont) return;
  if (cont->locks > 0) cont->locks--;
  // If it expired while locked, allow auto-destroy once unlocked.
  if (cont->locks == 0 && (cont->flags & RANE_VM_CONT_F_EXPIRED)) {
    rane_vm_container_destroy_force(cont);
  }
}

void rane_vm_container_set_lifetime(rane_vm_container_t* cont, uint64_t lifetime_ticks) {
  if (!cont) return;
  cont->lifetime = lifetime_ticks;
  if (lifetime_ticks == 0) {
    cont->flags &= ~RANE_VM_CONT_F_EXPIRED;
  } else if (lifetime_ticks == 0) {
    cont->flags |= RANE_VM_CONT_F_EXPIRED;
  }
}

uint64_t rane_vm_container_get_lifetime(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return cont->lifetime;
}

int rane_vm_container_is_expired(const rane_vm_container_t* cont) {
  if (!cont) return 0;
  return (cont->flags & RANE_VM_CONT_F_EXPIRED) != 0;
}

void rane_vm_container_tick(rane_vm_container_t* cont, uint64_t ticks) {
  if (!cont) return;
  if (cont->lifetime == 0) return; // infinite

  if (ticks >= cont->lifetime) cont->lifetime = 0;
  else cont->lifetime -= ticks;

  if (cont->lifetime == 0) {
    cont->flags |= RANE_VM_CONT_F_EXPIRED;
    if (cont->locks == 0) {
      rane_vm_container_destroy_force(cont);
    }
  }
}

int rane_vm_container_reserve(rane_vm_container_t* cont, size_t capacity) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_container_ensure_owned(cont)) return 0;

  if (capacity <= cont->capacity) return 1;
  if (cont->locks != 0) return 0;

  size_t new_cap = rane_vm_align_up_size(capacity, 16);
  void* nb = realloc(cont->base, new_cap);
  if (!nb) return 0;

  // Zero new region for determinism.
  if (new_cap > cont->capacity) {
    memset((uint8_t*)nb + cont->capacity, 0, new_cap - cont->capacity);
  }

  cont->base = nb;
  cont->capacity = new_cap;
  if (cont->size > cont->capacity) cont->size = cont->capacity;
  cont->flags |= RANE_VM_CONT_F_OWNED;
  return 1;
}

int rane_vm_container_resize(rane_vm_container_t* cont, size_t new_size, uint8_t fill_byte) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;

  if (new_size > cont->capacity) {
    if (!rane_vm_container_reserve(cont, new_size)) return 0;
  }

  if (new_size > cont->size && cont->base) {
    memset((uint8_t*)cont->base + cont->size, (int)fill_byte, new_size - cont->size);
  }

  cont->size = new_size;
  return 1;
}

static int rane_vm_bounds_ok(const rane_vm_container_t* c, size_t off, size_t len) {
  if (!c) return 0;
  if (len == 0) return 1;
  if (!c->base) return 0;
  size_t end = 0;
  if (rane_vm_add_overflow_size(off, len, &end)) return 0;
  return end <= c->size;
}

int rane_vm_container_write(rane_vm_container_t* cont, size_t off, const void* src, size_t len) {
  if (!cont || (!src && len != 0)) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  if (len == 0) return 1;

  memcpy((uint8_t*)cont->base + off, src, len);
  return 1;
}

int rane_vm_container_read(const rane_vm_container_t* cont, size_t off, void* dst, size_t len) {
  if (!cont || (!dst && len != 0)) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  if (len == 0) return 1;

  memcpy(dst, (const uint8_t*)cont->base + off, len);
  return 1;
}

int rane_vm_container_fill(rane_vm_container_t* cont, size_t off, uint8_t byte, size_t len) {
  if (!cont) return 0;
  if (!rane_vm_container_is_valid(cont)) return 0;
  if (!rane_vm_bounds_ok(cont, off, len)) return 0;
  if (len == 0) return 1;

  memset((uint8_t*)cont->base + off, (int)byte, len);
  return 1;
}

int rane_vm_container_copy(
  rane_vm_container_t* dst, size_t dst_off,
  const rane_vm_container_t* src, size_t src_off,
  size_t len
) {
  if (!dst || !src) return 0;
  if (!rane_vm_container_is_valid(dst) || !rane_vm_container_is_valid(src)) return 0;

  if (!rane_vm_bounds_ok(src, src_off, len)) return 0;
  if (!rane_vm_bounds_ok(dst, dst_off, len)) return 0;
  if (len == 0) return 1;

  memmove((uint8_t*)dst->base + dst_off, (const uint8_t*)src->base + src_off, len);
  return 1;
}

void rane_vm_container_destroy(rane_vm_container_t* cont) {
  if (!cont) return;
  if (!rane_vm_container_ensure_owned(cont)) return;

  if (cont->locks == 0) {
    rane_vm_container_destroy_force(cont);
  } else {
    // Defer freeing until unlocked.
    cont->flags |= RANE_VM_CONT_F_EXPIRED;
    cont->lifetime = 0;
  }
}

void rane_vm_container_destroy_force(rane_vm_container_t* cont) {
  if (!cont) return;
  if (!rane_vm_container_ensure_owned(cont)) return;

  if (cont->base) free(cont->base);

  cont->base = NULL;
  cont->size = 0;
  cont->capacity = 0;
  cont->locks = 0;
  cont->lifetime = 0;
  cont->flags = RANE_VM_CONT_F_NONE;
}
