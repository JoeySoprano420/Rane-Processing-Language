#include "rane_perf.h"
#include <cstddef>

rane_profiler_t* rane_profiler_start() {
  return NULL;
}

void rane_profiler_stop(rane_profiler_t* p) {
}

uint64_t rane_profiler_elapsed_ns(rane_profiler_t* p) {
  return 0;
}

void rane_parallel_sort(int* arr, size_t n) {
}

void rane_parallel_for(size_t start, size_t end, void (*fn)(size_t)) {
}
