#define _CRT_SECURE_NO_WARNINGS
#include "rane_stdlib.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// -----------------------------------------------------------------------------
// I/O
// -----------------------------------------------------------------------------

void rane_print(const char* str) {
  if (!str) return;
  fputs(str, stdout);
}

int rane_read_int() {
  int x = 0;
  (void)scanf("%d", &x);
  return x;
}

// -----------------------------------------------------------------------------
// Math
// -----------------------------------------------------------------------------

int rane_abs(int x) {
  return abs(x);
}

double rane_sqrt(double x) {
  return sqrt(x);
}

double rane_sin(double x) {
  return sin(x);
}

double rane_cos(double x) {
  return cos(x);
}

double rane_pow(double base, double exp) {
  return pow(base, exp);
}

// -----------------------------------------------------------------------------
// String
// -----------------------------------------------------------------------------

size_t rane_strlen(const char* s) {
  return s ? strlen(s) : 0;
}

int rane_strcmp(const char* s1, const char* s2) {
  if (!s1 && !s2) return 0;
  if (!s1) return -1;
  if (!s2) return 1;
  return strcmp(s1, s2);
}

char* rane_strcpy(char* dest, const char* src) {
  if (!dest) return NULL;
  if (!src) { dest[0] = 0; return dest; }
  return strcpy(dest, src);
}

char* rane_strcat(char* dest, const char* src) {
  if (!dest) return NULL;
  if (!src) return dest;
  return strcat(dest, src);
}

// -----------------------------------------------------------------------------
// Memory
// -----------------------------------------------------------------------------

void* rane_memcpy(void* dest, const void* src, size_t n) {
  if (!dest || !src || n == 0) return dest;
  return memcpy(dest, src, n);
}

void* rane_memset(void* s, int c, size_t n) {
  if (!s || n == 0) return s;
  return memset(s, c, n);
}

int rane_memcmp(const void* s1, const void* s2, size_t n) {
  if (n == 0) return 0;
  if (!s1 && !s2) return 0;
  if (!s1) return -1;
  if (!s2) return 1;
  return memcmp(s1, s2, n);
}

// -----------------------------------------------------------------------------
// Containers: rane_vector_t
// -----------------------------------------------------------------------------
// NOTE: Header only exposes `data` and `size`, so we store capacity in-band as a
// trailing hidden word allocated alongside the data block.
//
// Layout:
//   [elem0][elem1]...[elemN-1][capacity(size_t)]
// and `v->data` points to elem0.
//
// This keeps ABI stable with the current header while enabling amortized growth.

static size_t* rane_vec_cap_slot(void* data, size_t size) {
  if (!data) return NULL;
  return (size_t*)((uint8_t*)data + size * sizeof(void*));
}

static size_t rane_vec_get_cap(const rane_vector_t* v) {
  if (!v || !v->data) return 0;
  size_t* cap = rane_vec_cap_slot(v->data, v->size);
  return cap ? *cap : 0;
}

static void rane_vec_set_cap(rane_vector_t* v, size_t cap) {
  if (!v || !v->data) return;
  size_t* slot = rane_vec_cap_slot(v->data, v->size);
  if (slot) *slot = cap;
}

void rane_vector_init(rane_vector_t* v) {
  if (!v) return;
  v->data = NULL;
  v->size = 0;
}

static int rane_vector_reserve(rane_vector_t* v, size_t new_cap) {
  if (!v) return 0;
  if (new_cap == 0) new_cap = 1;

  size_t old_size = v->size;
  size_t old_cap = rane_vec_get_cap(v);

  if (old_cap >= new_cap) return 1;

  // Compute new capacity (growth)
  size_t cap = old_cap ? old_cap : 8;
  while (cap < new_cap) cap = cap + (cap >> 1) + 1; // ~1.5x growth

  // Allocate a new block: cap pointers + trailing size_t cap slot
  size_t bytes = cap * sizeof(void*) + sizeof(size_t);
  void** new_data = (void**)calloc(1, bytes);
  if (!new_data) return 0;

  // Copy old pointers
  if (v->data && old_size) {
    memcpy(new_data, v->data, old_size * sizeof(void*));
  }

  // Store capacity at end of new block (after cap elems)
  *(size_t*)((uint8_t*)new_data + cap * sizeof(void*)) = cap;

  // Free old block: old block allocation size was old_cap pointers + trailing size_t cap slot
  if (v->data) {
    void** old_data = (void**)v->data;
    free(old_data);
  }

  v->data = new_data;
  // NOTE: v->size unchanged.
  return 1;
}

void rane_vector_push(rane_vector_t* v, void* item) {
  if (!v) return;

  size_t cap = rane_vec_get_cap(v);
  if (v->data == NULL || cap == 0) {
    if (!rane_vector_reserve(v, 8)) return;
    cap = rane_vec_get_cap(v);
    (void)cap;
  }

  if (v->size >= cap) {
    if (!rane_vector_reserve(v, v->size + 1)) return;
  }

  ((void**)v->data)[v->size++] = item;

  // Capacity is stored in the allocation, but the slot location depends on `cap`.
  // We stored it at [cap], so nothing to update here.
}

// -----------------------------------------------------------------------------
// Sorting
// -----------------------------------------------------------------------------

static int rane_cmp_int_qsort(const void* a, const void* b) {
  const int ia = *(const int*)a;
  const int ib = *(const int*)b;
  return (ia > ib) - (ia < ib);
}

static int rane_cmp_double_qsort(const void* a, const void* b) {
  const double da = *(const double*)a;
  const double db = *(const double*)b;

  // Ensure deterministic ordering for NaNs: place NaNs at the end.
  const int a_nan = (da != da);
  const int b_nan = (db != db);
  if (a_nan && b_nan) return 0;
  if (a_nan) return 1;
  if (b_nan) return -1;

  return (da > db) - (da < db);
}

void rane_sort_int(int* arr, size_t n) {
  if (!arr || n <= 1) return;
  qsort(arr, n, sizeof(int), rane_cmp_int_qsort);
}

void rane_sort_double(double* arr, size_t n) {
  if (!arr || n <= 1) return;
  qsort(arr, n, sizeof(double), rane_cmp_double_qsort);
}

// -----------------------------------------------------------------------------
// Searching
// -----------------------------------------------------------------------------

int rane_binary_search_int(const int* arr, size_t n, int key) {
  if (!arr || n == 0) return -1;
  size_t low = 0, high = n;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (arr[mid] < key) low = mid + 1;
    else high = mid;
  }
  if (low < n && arr[low] == key) return (int)low;
  return -1;
}

// -----------------------------------------------------------------------------
// Algorithms: Dijkstra (real implementation)
// -----------------------------------------------------------------------------

typedef struct rane_dijk_heap_item_s {
  uint64_t node_index;
  uint64_t dist;
} rane_dijk_heap_item_t;

typedef struct rane_dijk_heap_s {
  rane_dijk_heap_item_t* items;
  size_t size;
  size_t cap;
} rane_dijk_heap_t;

static void dijk_heap_init(rane_dijk_heap_t* h) {
  if (!h) return;
  h->items = NULL;
  h->size = 0;
  h->cap = 0;
}

static void dijk_heap_destroy(rane_dijk_heap_t* h) {
  if (!h) return;
  free(h->items);
  h->items = NULL;
  h->size = 0;
  h->cap = 0;
}

static int dijk_heap_reserve(rane_dijk_heap_t* h, size_t need) {
  if (!h) return 0;
  if (h->cap >= need) return 1;
  size_t nc = h->cap ? h->cap : 64;
  while (nc < need) nc = nc + (nc >> 1) + 1;
  rane_dijk_heap_item_t* p = (rane_dijk_heap_item_t*)realloc(h->items, nc * sizeof(rane_dijk_heap_item_t));
  if (!p) return 0;
  h->items = p;
  h->cap = nc;
  return 1;
}

static void dijk_heap_swap(rane_dijk_heap_item_t* a, rane_dijk_heap_item_t* b) {
  rane_dijk_heap_item_t t = *a;
  *a = *b;
  *b = t;
}

static void dijk_heap_sift_up(rane_dijk_heap_t* h, size_t i) {
  while (i > 0) {
    size_t p = (i - 1) / 2;
    if (h->items[p].dist <= h->items[i].dist) break;
    dijk_heap_swap(&h->items[p], &h->items[i]);
    i = p;
  }
}

static void dijk_heap_sift_down(rane_dijk_heap_t* h, size_t i) {
  for (;;) {
    size_t l = i * 2 + 1;
    size_t r = i * 2 + 2;
    size_t m = i;

    if (l < h->size && h->items[l].dist < h->items[m].dist) m = l;
    if (r < h->size && h->items[r].dist < h->items[m].dist) m = r;
    if (m == i) break;

    dijk_heap_swap(&h->items[m], &h->items[i]);
    i = m;
  }
}

static int dijk_heap_push(rane_dijk_heap_t* h, uint64_t node_index, uint64_t dist) {
  if (!h) return 0;
  if (!dijk_heap_reserve(h, h->size + 1)) return 0;
  h->items[h->size].node_index = node_index;
  h->items[h->size].dist = dist;
  dijk_heap_sift_up(h, h->size);
  h->size++;
  return 1;
}

static int dijk_heap_pop_min(rane_dijk_heap_t* h, rane_dijk_heap_item_t* out) {
  if (!h || h->size == 0) return 0;
  if (out) *out = h->items[0];
  h->size--;
  if (h->size) {
    h->items[0] = h->items[h->size];
    dijk_heap_sift_down(h, 0);
  }
  return 1;
}

void rane_dijkstra(rane_graph_t* g, uint64_t start, uint64_t* dist, uint64_t* prev) {
  if (!g || !dist || !prev) return;
  if (!g->nodes || g->node_count == 0) return;

  for (size_t i = 0; i < g->node_count; i++) {
    dist[i] = UINT64_MAX;
    prev[i] = UINT64_MAX;
  }

  // Find start index by id.
  uint64_t start_idx = UINT64_MAX;
  for (size_t i = 0; i < g->node_count; i++) {
    if (g->nodes[i] && g->nodes[i]->id == start) {
      start_idx = (uint64_t)i;
      break;
    }
  }
  if (start_idx == UINT64_MAX) return;

  dist[start_idx] = 0;

  rane_dijk_heap_t heap;
  dijk_heap_init(&heap);
  (void)dijk_heap_push(&heap, start_idx, 0);

  while (heap.size) {
    rane_dijk_heap_item_t it;
    if (!dijk_heap_pop_min(&heap, &it)) break;

    // Stale entry check
    if (it.dist != dist[it.node_index]) continue;

    rane_graph_node_t* n = g->nodes[it.node_index];
    for (rane_graph_edge_t* e = n ? n->edges : NULL; e; e = e->next) {
      if (!e->to) continue;

      // Find target index by pointer equality (O(n) but ok for bootstrap stdlib).
      uint64_t to_idx = UINT64_MAX;
      for (size_t j = 0; j < g->node_count; j++) {
        if (g->nodes[j] == e->to) { to_idx = (uint64_t)j; break; }
      }
      if (to_idx == UINT64_MAX) continue;

      if (e->weight < 0) continue; // Dijkstra requires non-negative weights

      uint64_t w = (uint64_t)e->weight;
      uint64_t nd = (dist[it.node_index] == UINT64_MAX) ? UINT64_MAX : (dist[it.node_index] + w);
      if (nd < dist[to_idx]) {
        dist[to_idx] = nd;
        prev[to_idx] = it.node_index;
        (void)dijk_heap_push(&heap, to_idx, nd);
      }
    }
  }

  dijk_heap_destroy(&heap);
}

// -----------------------------------------------------------------------------
// Concurrency (Win32 implementation)
// -----------------------------------------------------------------------------

struct rane_thread_s {
  HANDLE h;
  void (*fn)(void*);
  void* arg;
};

struct rane_mutex_s {
  CRITICAL_SECTION cs;
  uint8_t init;
};

struct rane_channel_s {
  CRITICAL_SECTION cs;
  CONDITION_VARIABLE cv_send;
  CONDITION_VARIABLE cv_recv;

  void** buf;
  size_t cap;
  size_t head;
  size_t tail;
  size_t count;
  uint8_t init;
};

struct rane_barrier_s {
  CRITICAL_SECTION cs;
  CONDITION_VARIABLE cv;
  size_t target;
  size_t count;
  size_t generation;
};

static DWORD WINAPI rane_thread_trampoline(LPVOID p) {
  rane_thread_t* t = (rane_thread_t*)p;
  if (t && t->fn) t->fn(t->arg);
  return 0;
}

rane_thread_t* rane_thread_create(void (*fn)(void*), void* arg) {
  if (!fn) return NULL;

  rane_thread_t* t = (rane_thread_t*)calloc(1, sizeof(rane_thread_t));
  if (!t) return NULL;

  t->fn = fn;
  t->arg = arg;
  t->h = CreateThread(NULL, 0, rane_thread_trampoline, t, 0, NULL);
  if (!t->h) {
    free(t);
    return NULL;
  }

  return t;
}

void rane_thread_join(rane_thread_t* t) {
  if (!t) return;
  if (t->h) {
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
    t->h = NULL;
  }
  free(t);
}

void rane_mutex_init(rane_mutex_t* m) {
  if (!m) return;
  if (m->init) return;
  InitializeCriticalSection(&m->cs);
  m->init = 1;
}

void rane_mutex_lock(rane_mutex_t* m) {
  if (!m) return;
  if (!m->init) rane_mutex_init(m);
  EnterCriticalSection(&m->cs);
}

void rane_mutex_unlock(rane_mutex_t* m) {
  if (!m || !m->init) return;
  LeaveCriticalSection(&m->cs);
}

static void rane_channel_reserve(rane_channel_t* c, size_t cap) {
  if (!c) return;
  if (c->cap >= cap) return;

  size_t nc = c->cap ? c->cap : 16;
  while (nc < cap) nc = nc + (nc >> 1) + 1;

  void** nb = (void**)calloc(nc, sizeof(void*));
  if (!nb) return;

  // Copy existing elements in FIFO order.
  for (size_t i = 0; i < c->count; i++) {
    size_t idx = (c->head + i) % c->cap;
    nb[i] = c->buf[idx];
  }

  free(c->buf);
  c->buf = nb;
  c->cap = nc;
  c->head = 0;
  c->tail = c->count;
}

void rane_channel_init(rane_channel_t* c) {
  if (!c) return;
  if (c->init) return;

  InitializeCriticalSection(&c->cs);
  InitializeConditionVariable(&c->cv_send);
  InitializeConditionVariable(&c->cv_recv);

  c->buf = NULL;
  c->cap = 0;
  c->head = 0;
  c->tail = 0;
  c->count = 0;
  c->init = 1;

  // Default capacity
  rane_channel_reserve(c, 64);
}

void rane_channel_send(rane_channel_t* c, void* data) {
  if (!c) return;
  if (!c->init) rane_channel_init(c);

  EnterCriticalSection(&c->cs);

  // Unbounded channel: grow if full.
  if (c->count == c->cap) {
    rane_channel_reserve(c, c->cap ? (c->cap + 1) : 1);
  }

  if (c->cap != 0) {
    c->buf[c->tail] = data;
    c->tail = (c->tail + 1) % c->cap;
    c->count++;
  }

  WakeConditionVariable(&c->cv_recv);
  LeaveCriticalSection(&c->cs);
}

void* rane_channel_recv(rane_channel_t* c) {
  if (!c) return NULL;
  if (!c->init) rane_channel_init(c);

  EnterCriticalSection(&c->cs);
  while (c->count == 0) {
    SleepConditionVariableCS(&c->cv_recv, &c->cs, INFINITE);
  }

  void* v = NULL;
  if (c->cap != 0) {
    v = c->buf[c->head];
    c->buf[c->head] = NULL;
    c->head = (c->head + 1) % c->cap;
    c->count--;
  }

  WakeConditionVariable(&c->cv_send);
  LeaveCriticalSection(&c->cs);
  return v;
}

rane_barrier_t* rane_barrier_create(size_t count) {
  if (count == 0) return NULL;

  rane_barrier_t* b = (rane_barrier_t*)calloc(1, sizeof(rane_barrier_t));
  if (!b) return NULL;

  InitializeCriticalSection(&b->cs);
  InitializeConditionVariable(&b->cv);
  b->target = count;
  b->count = 0;
  b->generation = 0;
  return b;
}

void rane_barrier_wait(rane_barrier_t* b) {
  if (!b) return;

  EnterCriticalSection(&b->cs);

  size_t gen = b->generation;
  b->count++;
  if (b->count == b->target) {
    b->generation++;
    b->count = 0;
    WakeAllConditionVariable(&b->cv);
    LeaveCriticalSection(&b->cs);
    return;
  }

  while (gen == b->generation) {
    SleepConditionVariableCS(&b->cv, &b->cs, INFINITE);
  }

  LeaveCriticalSection(&b->cs);
}

void rane_barrier_destroy(rane_barrier_t* b) {
  if (!b) return;
  DeleteCriticalSection(&b->cs);
  free(b);
}
