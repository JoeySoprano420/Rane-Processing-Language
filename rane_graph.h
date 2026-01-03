#pragma once

#include <stdint.h>

// Graph data structure for RANE stdlib
typedef struct rane_graph_node_s {
  uint64_t id;
  void* data;
  struct rane_graph_edge_s* edges;
} rane_graph_node_t;

typedef struct rane_graph_edge_s {
  rane_graph_node_t* to;
  int weight;
  struct rane_graph_edge_s* next;
} rane_graph_edge_t;

typedef struct rane_graph_s {
  rane_graph_node_t** nodes;
  size_t node_count;
  size_t capacity;
} rane_graph_t;

void rane_graph_init(rane_graph_t* g);
void rane_graph_add_node(rane_graph_t* g, uint64_t id, void* data);
void rane_graph_add_edge(rane_graph_t* g, uint64_t from, uint64_t to, int weight);
rane_graph_node_t* rane_graph_get_node(rane_graph_t* g, uint64_t id);
void rane_graph_destroy(rane_graph_t* g);