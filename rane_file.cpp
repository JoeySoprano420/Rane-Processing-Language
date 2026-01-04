#include "rane_file.h"
#include <cstddef>

rane_file_t* rane_file_open(const char* path, const char* mode) {
  return NULL;
}

size_t rane_file_read(rane_file_t* f, void* buf, size_t size) {
  return 0;
}

size_t rane_file_write(rane_file_t* f, const void* buf, size_t size) {
  return 0;
}

void rane_file_close(rane_file_t* f) {
}

int rane_dir_create(const char* path) {
  return -1;
}

int rane_dir_list(const char* path, char** entries, size_t max_entries) {
  return -1;
}
