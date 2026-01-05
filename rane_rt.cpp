#include "rane_rt.h"

#include <cstdio>

void rane_rt_print(const char* s) {
  if (!s) return;
  fputs(s, stdout);
}
