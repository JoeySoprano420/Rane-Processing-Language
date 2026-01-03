#include "rane_graph.h"
#include <stdlib.h>
#include <string.h>

void rane_graph_init(rane_graph_t* g) {
  g->nodes = NULL;
  g->node_count = 0;
  g->capacity = 0;
}

void rane_graph_add_node(rane_graph_t* g, uint64_t id, void* data) {
  if (g->node_count >= g->capacity) {
    g->capacity = g->capacity ? g->capacity * 2 : 16;
    g->nodes = (rane_graph_node_t**)realloc(g->nodes, g->capacity * sizeof(rane_graph_node_t*));
  }
  rane_graph_node_t* node = (rane_graph_node_t*)malloc(sizeof(rane_graph_node_t));
  node->id = id;
  node->data = data;
  node->edges = NULL;
  g->nodes[g->node_count++] = node;
}

void rane_graph_add_edge(rane_graph_t* g, uint64_t from, uint64_t to, int weight) {
  rane_graph_node_t* from_node = rane_graph_get_node(g, from);
  rane_graph_node_t* to_node = rane_graph_get_node(g, to);
  if (!from_node || !to_node) return;

  rane_graph_edge_t* edge = (rane_graph_edge_t*)malloc(sizeof(rane_graph_edge_t));
  edge->to = to_node;
  edge->weight = weight;
  edge->next = from_node->edges;
  from_node->edges = edge;
}

rane_graph_node_t* rane_graph_get_node(rane_graph_t* g, uint64_t id) {
  for (size_t i = 0; i < g->node_count; i++) {
    if (g->nodes[i]->id == id) return g->nodes[i];
  }
  return NULL;
}

void rane_graph_destroy(rane_graph_t* g) {
  for (size_t i = 0; i < g->node_count; i++) {
    rane_graph_node_t* node = g->nodes[i];
    rane_graph_edge_t* edge = node->edges;
    while (edge) {
      rane_graph_edge_t* next = edge->next;
      free(edge);
      edge = next;
    }
    free(node);
  }
  free(g->nodes);
}