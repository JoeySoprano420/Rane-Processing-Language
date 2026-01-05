#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// File I/O Library for RANE

typedef struct rane_file_s rane_file_t;

rane_file_t* rane_file_open(const char* path, const char* mode);
size_t rane_file_read(rane_file_t* f, void* buf, size_t size);
size_t rane_file_write(rane_file_t* f, const void* buf, size_t size);
void rane_file_close(rane_file_t* f);

// Random access
int64_t rane_file_tell(rane_file_t* f);
int rane_file_seek(rane_file_t* f, int64_t offset, int origin); // origin: SEEK_SET/SEEK_CUR/SEEK_END
int64_t rane_file_size(rane_file_t* f);

// Convenience helpers
// Reads entire file into a newly allocated buffer. Caller must free().
// Returns 0 on failure.
int rane_file_read_all(const char* path, void** out_data, size_t* out_len);

// Writes entire buffer to file (truncating/creating file). Returns 0 on success.
int rane_file_write_all(const char* path, const void* data, size_t len);

// Error reporting
// Returns a pointer to a thread-local, null-terminated error string for the last rane_file operation.
const char* rane_file_last_error();

// Directory ops
int rane_dir_create(const char* path);

// Lists up to max_entries names (UTF-8). Writes into caller-provided buffers.
// Returns number of entries written, or -1 on error.
int rane_dir_list(const char* path, char** entries, size_t max_entries);

#ifdef __cplusplus
} // extern "C"
#endif