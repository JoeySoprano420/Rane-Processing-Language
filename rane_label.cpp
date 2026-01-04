#include "rane_label.h"
#include <stdio.h>
#include <string.h>

void rane_label_gen_make(rane_label_gen_t* g, char out_lbl[64]) {
  if (!g || !out_lbl) return;
  sprintf_s(out_lbl, 64, "L%u", (unsigned)g->next_id++);
}
