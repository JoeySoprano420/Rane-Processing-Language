#pragma once

// File I/O Library for RANE

typedef struct rane_file_s rane_file_t;

rane_file_t* rane_file_open(const char* path, const char* mode);
size_t rane_file_read(rane_file_t* f, void* buf, size_t size);
size_t rane_file_write(rane_file_t* f, const void* buf, size_t size);
void rane_file_close(rane_file_t* f);

// Directory ops
int rane_dir_create(const char* path);
int rane_dir_list(const char* path, char** entries, size_t max_entries);