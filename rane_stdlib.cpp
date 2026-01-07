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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
// Containers: rane_vector_t (stable ABI: data/size/capacity)
// -----------------------------------------------------------------------------

static size_t rane_vec_grow_to(size_t cur, size_t need) {
  size_t cap = cur ? cur : 8u;
  while (cap < need) {
    size_t next = cap + (cap >> 1) + 1u; // ~1.5x growth
    if (next < cap) return need;         // overflow fallback
    cap = next;
  }
  return cap;
}

void rane_vector_init(rane_vector_t* v) {
  if (!v) return;
  v->data = NULL;
  v->size = 0;
  v->capacity = 0;
}

void rane_vector_free(rane_vector_t* v) {
  if (!v) return;
  free(v->data);
  v->data = NULL;
  v->size = 0;
  v->capacity = 0;
}

void rane_vector_clear(rane_vector_t* v) {
  if (!v) return;
  v->size = 0;
}

void rane_vector_reserve(rane_vector_t* v, size_t capacity) {
  if (!v) return;
  if (capacity <= v->capacity) return;

  size_t new_cap = rane_vec_grow_to(v->capacity, capacity);
  void** nd = (void**)realloc(v->data, new_cap * sizeof(void*));
  if (!nd) return;

  v->data = nd;
  v->capacity = new_cap;
}

void rane_vector_push(rane_vector_t* v, void* item) {
  if (!v) return;
  if (v->size == v->capacity) {
    rane_vector_reserve(v, v->size + 1);
    if (v->size == v->capacity) return; // OOM
  }
  v->data[v->size++] = item;
}

void* rane_vector_pop(rane_vector_t* v) {
  if (!v || v->size == 0) return NULL;
  void* out = v->data[v->size - 1];
  v->size--;
  return out;
}

void* rane_vector_get(const rane_vector_t* v, size_t index) {
  if (!v || !v->data) return NULL;
  if (index >= v->size) return NULL;
  return v->data[index];
}

void rane_vector_set(rane_vector_t* v, size_t index, void* item) {
  if (!v || !v->data) return;
  if (index >= v->size) return;
  v->data[index] = item;
}

void rane_vector_insert(rane_vector_t* v, size_t index, void* item) {
  if (!v) return;
  if (index > v->size) index = v->size;

  if (v->size == v->capacity) {
    rane_vector_reserve(v, v->size + 1);
    if (v->size == v->capacity) return; // OOM
  }

  if (index < v->size) {
    memmove(&v->data[index + 1], &v->data[index], (v->size - index) * sizeof(void*));
  }

  v->data[index] = item;
  v->size++;
}

void* rane_vector_remove_at(rane_vector_t* v, size_t index) {
  if (!v || !v->data) return NULL;
  if (index >= v->size) return NULL;

  void* out = v->data[index];
  if (index + 1 < v->size) {
    memmove(&v->data[index], &v->data[index + 1], (v->size - index - 1) * sizeof(void*));
  }
  v->size--;
  return out;
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

    if (it.dist != dist[it.node_index]) continue;

    rane_graph_node_t* n = g->nodes[it.node_index];
    for (rane_graph_edge_t* e = n ? n->edges : NULL; e; e = e->next) {
      if (!e->to) continue;

      uint64_t to_idx = UINT64_MAX;
      for (size_t j = 0; j < g->node_count; j++) {
        if (g->nodes[j] == e->to) { to_idx = (uint64_t)j; break; }
      }
      if (to_idx == UINT64_MAX) continue;

      if (e->weight < 0) continue;

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
  size_t cap;          // capacity of ring buffer
  size_t head;
  size_t tail;
  size_t count;

  uint8_t init;
  uint8_t bounded;     // if set, send blocks when full (instead of growing)
};

struct rane_barrier_s {
  CRITICAL_SECTION cs;
  CONDITION_VARIABLE cv;
  size_t target;
  size_t count;
  size_t generation;
};

struct rane_rwlock_s {
  SRWLOCK lock;
};

struct rane_semaphore_s {
  HANDLE h;
};

struct rane_once_s {
  INIT_ONCE once;
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
  c->bounded = 0;

  rane_channel_reserve(c, 64);
}

void rane_channel_init_bounded(rane_channel_t* c, size_t capacity) {
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
  c->bounded = 1;

  if (capacity == 0) capacity = 1;
  rane_channel_reserve(c, capacity);
}

void rane_channel_send(rane_channel_t* c, void* data) {
  if (!c) return;
  if (!c->init) rane_channel_init(c);

  EnterCriticalSection(&c->cs);

  while (c->bounded && (c->count == c->cap)) {
    SleepConditionVariableCS(&c->cv_send, &c->cs, INFINITE);
  }

  if (!c->bounded && (c->count == c->cap)) {
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

void* rane_channel_recv_timeout_ms(rane_channel_t* c, uint32_t timeout_ms) {
  if (!c) return NULL;
  if (!c->init) rane_channel_init(c);

  EnterCriticalSection(&c->cs);

  while (c->count == 0) {
    BOOL ok = SleepConditionVariableCS(&c->cv_recv, &c->cs, timeout_ms);
    if (!ok) {
      DWORD err = GetLastError();
      if (err == ERROR_TIMEOUT) {
        LeaveCriticalSection(&c->cs);
        return NULL;
      }
      // For other errors, treat as timeout-like failure.
      LeaveCriticalSection(&c->cs);
      return NULL;
    }
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

// RWLock (SRWLOCK)

rane_rwlock_t* rane_rwlock_create(void) {
  rane_rwlock_t* l = (rane_rwlock_t*)calloc(1, sizeof(rane_rwlock_t));
  if (!l) return NULL;
  InitializeSRWLock(&l->lock);
  return l;
}

void rane_rwlock_destroy(rane_rwlock_t* l) {
  if (!l) return;
  free(l);
}

void rane_rwlock_read_lock(rane_rwlock_t* l) {
  if (!l) return;
  AcquireSRWLockShared(&l->lock);
}

void rane_rwlock_read_unlock(rane_rwlock_t* l) {
  if (!l) return;
  ReleaseSRWLockShared(&l->lock);
}

void rane_rwlock_write_lock(rane_rwlock_t* l) {
  if (!l) return;
  AcquireSRWLockExclusive(&l->lock);
}

void rane_rwlock_write_unlock(rane_rwlock_t* l) {
  if (!l) return;
  ReleaseSRWLockExclusive(&l->lock);
}

// Semaphore

rane_semaphore_t* rane_semaphore_create(int32_t initial_count, int32_t max_count) {
  if (max_count <= 0) return NULL;
  if (initial_count < 0) initial_count = 0;
  if (initial_count > max_count) initial_count = max_count;

  rane_semaphore_t* s = (rane_semaphore_t*)calloc(1, sizeof(rane_semaphore_t));
  if (!s) return NULL;

  s->h = CreateSemaphoreA(NULL, initial_count, max_count, NULL);
  if (!s->h) {
    free(s);
    return NULL;
  }

  return s;
}

void rane_semaphore_destroy(rane_semaphore_t* s) {
  if (!s) return;
  if (s->h) CloseHandle(s->h);
  s->h = NULL;
  free(s);
}

void rane_semaphore_release(rane_semaphore_t* s, int32_t release_count) {
  if (!s || !s->h) return;
  if (release_count <= 0) return;
  ReleaseSemaphore(s->h, release_count, NULL);
}

int rane_semaphore_wait_ms(rane_semaphore_t* s, uint32_t timeout_ms) {
  if (!s || !s->h) return 0;
  DWORD r = WaitForSingleObject(s->h, timeout_ms);
  return (r == WAIT_OBJECT_0) ? 1 : 0;
}

// Once init

void rane_once_init(rane_once_t* o) {
  if (!o) return;
  o->once = INIT_ONCE_STATIC_INIT;
}

typedef struct rane_once_call_s {
  void (*fn)(void*);
  void* arg;
} rane_once_call_t;

static BOOL CALLBACK rane_once_trampoline(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context) {
  (void)InitOnce;
  (void)Context;
  rane_once_call_t* c = (rane_once_call_t*)Parameter;
  if (c && c->fn) c->fn(c->arg);
  return TRUE;
}

void rane_once_do(rane_once_t* o, void (*fn)(void*), void* arg) {
  if (!o || !fn) return;
  rane_once_call_t call = {};
  call.fn = fn;
  call.arg = arg;
  InitOnceExecuteOnce(&o->once, rane_once_trampoline, &call, NULL);
}

// Atomics (Interlocked*)

uint32_t rane_atomic_load_u32(volatile uint32_t* p) {
  if (!p) return 0;
  return (uint32_t)InterlockedCompareExchange((volatile LONG*)p, 0, 0);
}

void rane_atomic_store_u32(volatile uint32_t* p, uint32_t v) {
  if (!p) return;
  InterlockedExchange((volatile LONG*)p, (LONG)v);
}

uint32_t rane_atomic_add_u32(volatile uint32_t* p, uint32_t v) {
  if (!p) return 0;
  return (uint32_t)InterlockedAdd((volatile LONG*)p, (LONG)v);
}

uint32_t rane_atomic_cas_u32(volatile uint32_t* p, uint32_t expected, uint32_t desired) {
  if (!p) return 0;
  return (uint32_t)InterlockedCompareExchange((volatile LONG*)p, (LONG)desired, (LONG)expected);
}

uint64_t rane_atomic_load_u64(volatile uint64_t* p) {
  if (!p) return 0;
  return (uint64_t)InterlockedCompareExchange64((volatile LONGLONG*)p, 0, 0);
}

void rane_atomic_store_u64(volatile uint64_t* p, uint64_t v) {
  if (!p) return;
  InterlockedExchange64((volatile LONGLONG*)p, (LONGLONG)v);
}

uint64_t rane_atomic_add_u64(volatile uint64_t* p, uint64_t v) {
  if (!p) return 0;
  return (uint64_t)InterlockedAdd64((volatile LONGLONG*)p, (LONGLONG)v);
}

uint64_t rane_atomic_cas_u64(volatile uint64_t* p, uint64_t expected, uint64_t desired) {
  if (!p) return 0;
  return (uint64_t)InterlockedCompareExchange64((volatile LONGLONG*)p, (LONGLONG)desired, (LONGLONG)expected);
}

// -----------------------------------------------------------------------------
// DSP / Audio utilities (pure functions)
// -----------------------------------------------------------------------------

static float rane_f32_abs(float x) {
  return (x < 0.0f) ? -x : x;
}

void rane_dsp_window_hann_f32(float* dst, const float* src, size_t n) {
  if (!dst || !src || n == 0) return;
  if (n == 1) { dst[0] = src[0]; return; }

  const float inv = 1.0f / (float)(n - 1);
  for (size_t i = 0; i < n; i++) {
    float w = 0.5f - 0.5f * (float)cos(2.0 * M_PI * (double)((float)i * inv));
    dst[i] = src[i] * w;
  }
}

void rane_dsp_window_hamming_f32(float* dst, const float* src, size_t n) {
  if (!dst || !src || n == 0) return;
  if (n == 1) { dst[0] = src[0]; return; }

  const float inv = 1.0f / (float)(n - 1);
  for (size_t i = 0; i < n; i++) {
    float w = 0.54f - 0.46f * (float)cos(2.0 * M_PI * (double)((float)i * inv));
    dst[i] = src[i] * w;
  }
}

void rane_dsp_window_blackman_f32(float* dst, const float* src, size_t n) {
  if (!dst || !src || n == 0) return;
  if (n == 1) { dst[0] = src[0]; return; }

  const float inv = 1.0f / (float)(n - 1);
  for (size_t i = 0; i < n; i++) {
    float a = (float)i * inv;
    float w = 0.42f
      - 0.5f * (float)cos(2.0 * M_PI * (double)a)
      + 0.08f * (float)cos(4.0 * M_PI * (double)a);
    dst[i] = src[i] * w;
  }
}

float rane_dsp_rms_f32(const float* x, size_t n) {
  if (!x || n == 0) return 0.0f;
  double acc = 0.0;
  for (size_t i = 0; i < n; i++) {
    double v = (double)x[i];
    acc += v * v;
  }
  return (float)sqrt(acc / (double)n);
}

float rane_dsp_peak_f32(const float* x, size_t n) {
  if (!x || n == 0) return 0.0f;
  float peak = 0.0f;
  for (size_t i = 0; i < n; i++) {
    float a = rane_f32_abs(x[i]);
    if (a > peak) peak = a;
  }
  return peak;
}

void rane_dsp_normalize_f32(float* x, size_t n, float peak_target) {
  if (!x || n == 0) return;
  if (peak_target <= 0.0f) return;

  float p = rane_dsp_peak_f32(x, n);
  if (p <= 0.0f) return;

  float g = peak_target / p;
  for (size_t i = 0; i < n; i++) {
    x[i] *= g;
  }
}

void rane_dsp_mix_f32(float* dst, const float* src, size_t n, float gain) {
  if (!dst || !src || n == 0) return;
  for (size_t i = 0; i < n; i++) {
    dst[i] += src[i] * gain;
  }
}

void rane_dsp_biquad_reset_f32(rane_biquad_f32_t* b) {
  if (!b) return;
  b->z1 = 0.0f;
  b->z2 = 0.0f;
}

static void rane_biquad_set_coeffs(rane_biquad_f32_t* b, double b0, double b1, double b2, double a0, double a1, double a2) {
  // Normalize (a0==1)
  if (!b) return;
  if (a0 == 0.0) return;
  double inv = 1.0 / a0;
  b->b0 = (float)(b0 * inv);
  b->b1 = (float)(b1 * inv);
  b->b2 = (float)(b2 * inv);
  b->a1 = (float)(a1 * inv);
  b->a2 = (float)(a2 * inv);
}

void rane_dsp_biquad_init_lpf_f32(rane_biquad_f32_t* b, float sample_rate_hz, float cutoff_hz, float q) {
  if (!b) return;
  if (sample_rate_hz <= 0.0f) return;
  if (cutoff_hz <= 0.0f) return;
  if (q <= 0.0f) q = 0.70710678f;

  double w0 = 2.0 * M_PI * (double)cutoff_hz / (double)sample_rate_hz;
  double cw = cos(w0);
  double sw = sin(w0);
  double alpha = sw / (2.0 * (double)q);

  double b0 = (1.0 - cw) / 2.0;
  double b1 = 1.0 - cw;
  double b2 = (1.0 - cw) / 2.0;
  double a0 = 1.0 + alpha;
  double a1 = -2.0 * cw;
  double a2 = 1.0 - alpha;

  rane_biquad_set_coeffs(b, b0, b1, b2, a0, a1, a2);
  rane_dsp_biquad_reset_f32(b);
}

void rane_dsp_biquad_init_hpf_f32(rane_biquad_f32_t* b, float sample_rate_hz, float cutoff_hz, float q) {
  if (!b) return;
  if (sample_rate_hz <= 0.0f) return;
  if (cutoff_hz <= 0.0f) return;
  if (q <= 0.0f) q = 0.70710678f;

  double w0 = 2.0 * M_PI * (double)cutoff_hz / (double)sample_rate_hz;
  double cw = cos(w0);
  double sw = sin(w0);
  double alpha = sw / (2.0 * (double)q);

  double b0 = (1.0 + cw) / 2.0;
  double b1 = -(1.0 + cw);
  double b2 = (1.0 + cw) / 2.0;
  double a0 = 1.0 + alpha;
  double a1 = -2.0 * cw;
  double a2 = 1.0 - alpha;

  rane_biquad_set_coeffs(b, b0, b1, b2, a0, a1, a2);
  rane_dsp_biquad_reset_f32(b);
}

void rane_dsp_biquad_process_inplace_f32(rane_biquad_f32_t* b, float* x, size_t n) {
  if (!b || !x) return;
  float z1 = b->z1;
  float z2 = b->z2;

  for (size_t i = 0; i < n; i++) {
    float in = x[i];
    float out = (b->b0 * in) + z1;
    z1 = (b->b1 * in) + z2 - (b->a1 * out);
    z2 = (b->b2 * in) - (b->a2 * out);
    x[i] = out;
  }

  b->z1 = z1;
  b->z2 = z2;
}

// FFT utilities

static int rane_is_pow2_size(size_t n) {
  return (n != 0) && ((n & (n - 1)) == 0);
}

static uint32_t rane_log2_u32(size_t n) {
  uint32_t k = 0;
  while ((size_t)1u << k < n) k++;
  return k;
}

static uint32_t rane_reverse_bits_u32(uint32_t x, uint32_t bits) {
  uint32_t r = 0;
  for (uint32_t i = 0; i < bits; i++) {
    r = (r << 1) | (x & 1u);
    x >>= 1;
  }
  return r;
}

void rane_dsp_fft_cplx_f32(float* re, float* im, size_t n, int inverse) {
  if (!re || !im) return;
  if (n == 0) return;
  if (!rane_is_pow2_size(n)) return;

  uint32_t bits = rane_log2_u32(n);

  // Bit-reversal permutation
  for (uint32_t i = 0; i < (uint32_t)n; i++) {
    uint32_t j = rane_reverse_bits_u32(i, bits);
    if (j > i) {
      float tr = re[i]; re[i] = re[j]; re[j] = tr;
      float ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
  }

  // Iterative Cooley-Tukey
  const double sign = inverse ? 1.0 : -1.0;

  for (size_t len = 2; len <= n; len <<= 1) {
    double ang = sign * (2.0 * M_PI / (double)len);
    double wlen_r = cos(ang);
    double wlen_i = sin(ang);

    for (size_t i = 0; i < n; i += len) {
      double wr = 1.0;
      double wi = 0.0;

      size_t half = len >> 1;
      for (size_t j = 0; j < half; j++) {
        size_t u = i + j;
        size_t v = i + j + half;

        double ur = (double)re[u];
        double ui = (double)im[u];
        double vr = (double)re[v] * wr - (double)im[v] * wi;
        double vi = (double)re[v] * wi + (double)im[v] * wr;

        re[u] = (float)(ur + vr);
        im[u] = (float)(ui + vi);
        re[v] = (float)(ur - vr);
        im[v] = (float)(ui - vi);

        double nwr = wr * wlen_r - wi * wlen_i;
        double nwi = wr * wlen_i + wi * wlen_r;
        wr = nwr;
        wi = nwi;
      }
    }
  }

  if (inverse) {
    for (size_t i = 0; i < n; i++) {
      re[i] /= (float)n;
      im[i] /= (float)n;
    }
  }
}
