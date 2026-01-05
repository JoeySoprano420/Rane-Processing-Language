#include "rane_gc.h"
#include <assert.h>
#include <string.h>
#include <cstdint>

// Minimal internal graph node stored in GC payload.
struct node_t {
  rane_gc_object_t* left;
  rane_gc_object_t* right;
  uint64_t tag;
};

static void trace_node(rane_gc_object_t* obj) {
  if (!obj || !obj->data) return;
  node_t* n = (node_t*)obj->data;
  if (n->left) rane_gc_mark_sweep_mark(n->left);
  if (n->right) rane_gc_mark_sweep_mark(n->right);
}

static rane_gc_object_t* make_node(uint64_t tag) {
  rane_gc_object_t* o = NULL;
  node_t* n = (node_t*)rane_gc_alloc_data(sizeof(node_t), &o);
  assert(o && n);
  memset(n, 0, sizeof(*n));
  n->tag = tag;
  return o;
}

static void test_refcount_collect() {
  rane_gc_object_t* o = rane_gc_alloc(16);
  assert(o);
  assert(rane_gc_object_count() >= 1);

  rane_gc_release(o);
  rane_gc_collect();
}

static void test_mark_sweep_roots() {
  rane_gc_mark_sweep_init();
  rane_gc_mark_sweep_set_trace(trace_node);

  // Build small graph: root -> a -> b, and an unreachable c
  rane_gc_object_t* root = make_node(1);
  rane_gc_object_t* a = make_node(2);
  rane_gc_object_t* b = make_node(3);
  rane_gc_object_t* c = make_node(4);

  ((node_t*)root->data)->left = a;
  ((node_t*)a->data)->left = b;

  // register root slot
  rane_gc_object_t* root_slot = root;
  rane_gc_mark_sweep_register_root(&root_slot);

  size_t before = rane_gc_object_count();
  rane_gc_mark_sweep_collect();
  size_t after = rane_gc_object_count();

  // c should have been reclaimed, but root/a/b survive
  assert(after + 1 == before);

  // cover new maintenance step: null-out slot then compact
  rane_gc_mark_sweep_unregister_root(&root_slot);
  root_slot = NULL;
  rane_gc_mark_sweep_register_root(&root_slot);
  rane_gc_mark_sweep_compact_roots();

  // cover unregister-all
  rane_gc_mark_sweep_unregister_all_roots();

  // cleanup
  root_slot = NULL;
  rane_gc_mark_sweep_collect();
}

static void test_temp_roots() {
  rane_gc_mark_sweep_init();
  rane_gc_mark_sweep_set_trace(trace_node);

  rane_gc_object_t* tmp = make_node(10);
  rane_gc_mark_sweep_push_root(tmp);
  rane_gc_mark_sweep_collect();
  // still live
  assert(rane_gc_object_count() >= 1);

  // cover clear-temp-roots
  rane_gc_mark_sweep_clear_temp_roots();
  rane_gc_mark_sweep_collect();
}

static void test_stats_snapshot() {
  rane_gc_stats_t s = {};
  rane_gc_get_stats(&s);
  // Basic sanity (should not crash / produce nonsense)
  assert(s.object_capacity >= s.object_count);
  assert(s.root_slot_capacity >= s.root_slot_count);
  assert(s.temp_root_capacity >= s.temp_root_count);
}

void rane_gc_run_selftest() {
  rane_gc_init();
  test_refcount_collect();
  test_mark_sweep_roots();
  test_temp_roots();
  test_stats_snapshot();
  rane_gc_shutdown();
}
