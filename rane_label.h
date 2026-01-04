#pragma once

#include <stdint.h>
#include "rane_common.h"

// Simple label generator + fixup model for TIR lowering/codegen.

typedef struct rane_label_gen_s {
  uint32_t next_id;
} rane_label_gen_t;

static inline void rane_label_gen_init(rane_label_gen_t* g) { g->next_id = 1; }

// Produces labels like "L1", "L2"...
void rane_label_gen_make(rane_label_gen_t* g, char out_lbl[64]);
