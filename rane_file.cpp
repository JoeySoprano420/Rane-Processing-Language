#include "rane_file.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#endif

struct rane_file_s {
  FILE* f;
};

static thread_local char g_rane_file_last_err[512] = {0};

static void rane_file_set_last_error(const char* msg) {
  if (!msg) msg = "unknown error";
#if defined(_WIN32)
  strncpy_s(g_rane_file_last_err, sizeof(g_rane_file_last_err), msg, _TRUNCATE);
#else
  strncpy(g_rane_file_last_err, msg, sizeof(g_rane_file_last_err) - 1);
  g_rane_file_last_err[sizeof(g_rane_file_last_err) - 1] = 0;
#endif
}

#if defined(_WIN32)
static void rane_file_set_last_error_win32(const char* prefix, DWORD err) {
  char sysmsg[256] = {0};
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD n = FormatMessageA(flags, NULL, err, 0, sysmsg, (DWORD)sizeof(sysmsg), NULL);
  if (!n) {
    if (prefix) {
      char buf[256];
      sprintf_s(buf, sizeof(buf), "%s (win32=%lu)", prefix, (unsigned long)err);
      rane_file_set_last_error(buf);
    } else {
      char buf[128];
      sprintf_s(buf, sizeof(buf), "win32 error %lu", (unsigned long)err);
      rane_file_set_last_error(buf);
    }
    return;
  }

  // Trim trailing newlines from FormatMessage.
  while (n > 0 && (sysmsg[n - 1] == '\r' || sysmsg[n - 1] == '\n')) sysmsg[--n] = 0;

  char buf[512];
  if (prefix) sprintf_s(buf, sizeof(buf), "%s: %s", prefix, sysmsg);
  else sprintf_s(buf, sizeof(buf), "%s", sysmsg);
  rane_file_set_last_error(buf);
}
#else
static void rane_file_set_last_error_errno(const char* prefix, int err) {
  const char* em = strerror(err);
  if (!em) em = "unknown errno";
  char buf[512];
  if (prefix) {
    snprintf(buf, sizeof(buf), "%s: %s", prefix, em);
  } else {
    snprintf(buf, sizeof(buf), "%s", em);
  }
  rane_file_set_last_error(buf);
}
#endif

const char* rane_file_last_error() {
  return g_rane_file_last_err;
}

static void rane_strcpy_trunc(char* dst, const char* src, size_t cap) {
  if (!dst || cap == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
#if defined(_WIN32)
  strncpy_s(dst, cap, src, _TRUNCATE);
#else
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
#endif
}

rane_file_t* rane_file_open(const char* path, const char* mode) {
  if (!path || !mode) {
    rane_file_set_last_error("invalid arg");
    return NULL;
  }

  FILE* fp = NULL;
#if defined(_WIN32)
  errno_t e = fopen_s(&fp, path, mode);
  if (e != 0 || !fp) {
    // fopen_s sets errno.
    rane_file_set_last_error_win32("fopen", GetLastError());
    return NULL;
  }
#else
  fp = fopen(path, mode);
  if (!fp) {
    rane_file_set_last_error_errno("fopen", errno);
    return NULL;
  }
#endif

  rane_file_t* rf = (rane_file_t*)malloc(sizeof(rane_file_t));
  if (!rf) {
    fclose(fp);
    rane_file_set_last_error("out of memory");
    return NULL;
  }
  rf->f = fp;
  return rf;
}

size_t rane_file_read(rane_file_t* f, void* buf, size_t size) {
  if (!f || !f->f || !buf) {
    rane_file_set_last_error("invalid arg");
    return 0;
  }
  if (size == 0) return 0;

  size_t n = fread(buf, 1, size, f->f);
  if (n == 0 && ferror(f->f)) {
#if defined(_WIN32)
    rane_file_set_last_error_win32("fread", GetLastError());
#else
    rane_file_set_last_error_errno("fread", errno);
#endif
  }
  return n;
}

size_t rane_file_write(rane_file_t* f, const void* buf, size_t size) {
  if (!f || !f->f || !buf) {
    rane_file_set_last_error("invalid arg");
    return 0;
  }
  if (size == 0) return 0;

  size_t n = fwrite(buf, 1, size, f->f);
  if (n != size && ferror(f->f)) {
#if defined(_WIN32)
    rane_file_set_last_error_win32("fwrite", GetLastError());
#else
    rane_file_set_last_error_errno("fwrite", errno);
#endif
  }
  return n;
}

void rane_file_close(rane_file_t* f) {
  if (!f) return;
  if (f->f) fclose(f->f);
  f->f = NULL;
  free(f);
}

int64_t rane_file_tell(rane_file_t* f) {
  if (!f || !f->f) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }
#if defined(_WIN32)
  __int64 p = _ftelli64(f->f);
  if (p < 0) {
    rane_file_set_last_error("ftell failed");
    return -1;
  }
  return (int64_t)p;
#else
  long p = ftell(f->f);
  if (p < 0) {
    rane_file_set_last_error_errno("ftell", errno);
    return -1;
  }
  return (int64_t)p;
#endif
}

int rane_file_seek(rane_file_t* f, int64_t offset, int origin) {
  if (!f || !f->f) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }
#if defined(_WIN32)
  if (_fseeki64(f->f, (__int64)offset, origin) != 0) {
    rane_file_set_last_error("fseek failed");
    return -1;
  }
  return 0;
#else
  if (fseek(f->f, (long)offset, origin) != 0) {
    rane_file_set_last_error_errno("fseek", errno);
    return -1;
  }
  return 0;
#endif
}

int64_t rane_file_size(rane_file_t* f) {
  if (!f || !f->f) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }

  int64_t cur = rane_file_tell(f);
  if (cur < 0) return -1;

  if (rane_file_seek(f, 0, SEEK_END) != 0) return -1;
  int64_t end = rane_file_tell(f);
  if (end < 0) return -1;

  (void)rane_file_seek(f, cur, SEEK_SET);
  return end;
}

int rane_file_read_all(const char* path, void** out_data, size_t* out_len) {
  if (out_data) *out_data = NULL;
  if (out_len) *out_len = 0;
  if (!path || !out_data) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }

  rane_file_t* f = rane_file_open(path, "rb");
  if (!f) return -1;

  int64_t sz64 = rane_file_size(f);
  if (sz64 < 0) {
    rane_file_close(f);
    return -1;
  }

  if (sz64 > (int64_t)SIZE_MAX) {
    rane_file_close(f);
    rane_file_set_last_error("file too large");
    return -1;
  }

  size_t sz = (size_t)sz64;
  void* buf = malloc(sz ? sz : 1);
  if (!buf) {
    rane_file_close(f);
    rane_file_set_last_error("out of memory");
    return -1;
  }

  if (rane_file_seek(f, 0, SEEK_SET) != 0) {
    free(buf);
    rane_file_close(f);
    return -1;
  }

  size_t rd = 0;
  while (rd < sz) {
    size_t n = rane_file_read(f, (uint8_t*)buf + rd, sz - rd);
    if (n == 0) break;
    rd += n;
  }

  rane_file_close(f);

  if (rd != sz) {
    free(buf);
    rane_file_set_last_error("short read");
    return -1;
  }

  *out_data = buf;
  if (out_len) *out_len = sz;
  return 0;
}

int rane_file_write_all(const char* path, const void* data, size_t len) {
  if (!path || (!data && len != 0)) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }

  rane_file_t* f = rane_file_open(path, "wb");
  if (!f) return -1;

  size_t wr = 0;
  while (wr < len) {
    size_t n = rane_file_write(f, (const uint8_t*)data + wr, len - wr);
    if (n == 0) break;
    wr += n;
  }

  rane_file_close(f);

  if (wr != len) {
    rane_file_set_last_error("short write");
    return -1;
  }

  return 0;
}

int rane_dir_create(const char* path) {
  if (!path) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }
#if defined(_WIN32)
  if (CreateDirectoryA(path, NULL)) return 0;
  DWORD e = GetLastError();
  if (e == ERROR_ALREADY_EXISTS) return 0;
  rane_file_set_last_error_win32("CreateDirectory", e);
  return -1;
#else
  if (mkdir(path, 0755) == 0) return 0;
  if (errno == EEXIST) return 0;
  rane_file_set_last_error_errno("mkdir", errno);
  return -1;
#endif
}

int rane_dir_list(const char* path, char** entries, size_t max_entries) {
  if (!path || !entries) {
    rane_file_set_last_error("invalid arg");
    return -1;
  }

#if defined(_WIN32)
  // Use FindFirstFileA on "path\\*"
  char pattern[MAX_PATH];
  pattern[0] = 0;
  rane_strcpy_trunc(pattern, path, sizeof(pattern));

  size_t len = strlen(pattern);
  if (len == 0) {
    rane_file_set_last_error("invalid path");
    return -1;
  }
  if (pattern[len - 1] != '\\' && pattern[len - 1] != '/') {
    rane_strcpy_trunc(pattern + len, "\\*", sizeof(pattern) - len);
  } else {
    rane_strcpy_trunc(pattern + len, "*", sizeof(pattern) - len);
  }

  WIN32_FIND_DATAA ffd;
  HANDLE h = FindFirstFileA(pattern, &ffd);
  if (h == INVALID_HANDLE_VALUE) {
    rane_file_set_last_error_win32("FindFirstFile", GetLastError());
    return -1;
  }

  int count = 0;
  do {
    const char* name = ffd.cFileName;
    if (!name) continue;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    if ((size_t)count >= max_entries) break;
    if (entries[count]) {
      // Assume caller allocated a buffer; truncate to MAX_PATH.
      rane_strcpy_trunc(entries[count], name, MAX_PATH);
      count++;
    }
  } while (FindNextFileA(h, &ffd));

  FindClose(h);
  return count;
#else
  DIR* d = opendir(path);
  if (!d) {
    rane_file_set_last_error_errno("opendir", errno);
    return -1;
  }

  int count = 0;
  for (;;) {
    errno = 0;
    struct dirent* ent = readdir(d);
    if (!ent) break;

    const char* name = ent->d_name;
    if (!name) continue;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    if ((size_t)count >= max_entries) break;
    if (entries[count]) {
      // Caller must provide a buffer; assume 1024 cap if unknown.
      rane_strcpy_trunc(entries[count], name, 1024);
      count++;
    }
  }

  int err = errno;
  closedir(d);
  if (err != 0) {
    rane_file_set_last_error_errno("readdir", err);
    return -1;
  }
  return count;
#endif
}
