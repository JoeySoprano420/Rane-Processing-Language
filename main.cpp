#include "rane_driver.h"

#include <stdio.h>
#include <string.h>

#include "rane_loader.h"
#include "rane_parser.h"
#include "rane_typecheck.h"
#include "rane_tir.h"
#include "rane_ssa.h"
#include "rane_regalloc.h"
#include "rane_optimize.h"
#include "rane_stdlib.h"
#include "rane_hashmap.h"
#include "rane_bst.h"
#include "rane_gc.h"
#include "rane_except.h"
#include "rane_aot.h"
#include "rane_vm.h"
#include "rane_graph.h"
#include "rane_heap.h"
#include "rane_net.h"
#include "rane_crypto.h"
#include "rane_file.h"
#include "rane_thread.h"
#include "rane_security.h"
#include "rane_perf.h"
#include "rane_lexer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <iostream>

#pragma comment(lib, "bcrypt.lib")

typedef struct rane_cli_options_s {
  const char* input_path;
  const char* output_path;
  int opt_level;
  int emit_c;

  int run_selftest;
  int run_demo_pipeline;

  int dump_ast;
  int dump_tir;
  int dump_ssa;
  int dump_regalloc;

  int lex_only;
  int parse_only;
  int typecheck_only;

  int time_stages;

  int run_after;          // run the produced exe
  int emit_and_run;       // compile then run (exe only)

  // New: OS tooling
  int os_info;
  const char* env_name;
  int print_cwd;
  const char* cat_path;
  const char* sha256_path;
  const char* run_args;   // string passed as-is to child process

  int show_help;
  int show_version;
} rane_cli_options_t;

typedef struct rane_timer_s {
  LARGE_INTEGER freq;
  LARGE_INTEGER t0;
} rane_timer_t;

static void rane_timer_start(rane_timer_t* t) {
  QueryPerformanceFrequency(&t->freq);
  QueryPerformanceCounter(&t->t0);
}

static double rane_timer_elapsed_ms(const rane_timer_t* t) {
  LARGE_INTEGER t1;
  QueryPerformanceCounter(&t1);
  LONGLONG dt = t1.QuadPart - t->t0.QuadPart;
  return (double)dt * 1000.0 / (double)t->freq.QuadPart;
}

static void rane_print_version() {
  // Keep version string local to main until a stable build/version module exists.
  printf("rane: bootstrap compiler (VS2026)\n");
}

static void rane_print_help() {
  rane_print_version();
  printf("\n");
  printf("Usage:\n");
  printf("  rane <input.rane> [-o <output>] [-O0|-O1|-O2|-O3] [-emit-c]\n");
  printf("\n");
  printf("Compile modes:\n");
  printf("  (default)                Compile to exe (PE x64)\n");
  printf("  -emit-c                  Compile to portable C (out.c)\n");
  printf("  --run                    Run an existing exe (requires <input.exe>)\n");
  printf("  --run --args \"...\"       Run an existing exe with arguments\n");
  printf("  --emit-and-run           Compile to exe, then run it\n");
  printf("\n");
  printf("OS tools:\n");
  printf("  --os-info                Print OS/CPU/memory info (Win32)\n");
  printf("  --cwd                    Print current directory\n");
  printf("  --env <NAME>             Print environment variable\n");
  printf("  --cat <path>             Dump file to stdout (Win32 streaming)\n");
  printf("  --sha256 <path>          SHA-256 hash a file (CNG/BCrypt)\n");
  printf("\n");
  printf("Analysis modes:\n");
  printf("  --lex                    Tokenize and print tokens\n");
  printf("  --parse-only             Parse only\n");
  printf("  --typecheck-only         Parse + typecheck only\n");
  printf("\n");
  printf("Dump modes:\n");
  printf("  --dump-ast               Parse + minimal AST summary\n");
  printf("  --dump-tir               Parse/typecheck/lower + minimal TIR summary + opcode histogram\n");
  printf("  --dump-ssa               Run SSA + print a minimal SSA status line\n");
  printf("  --dump-regalloc          Run regalloc + print a minimal regalloc status line\n");
  printf("\n");
  printf("Other:\n");
  printf("  -o <path>                Output path (defaults: a.exe or out.c when -emit-c)\n");
  printf("  -O0|-O1|-O2|-O3           Optimization level (default: -O2)\n");
  printf("  --time                   Print stage timings\n");
  printf("  --run-selftest           Run core subsystem selftests (currently: GC)\n");
  printf("  --demo-pipeline          Run in-process demo pipeline (no file IO)\n");
  printf("  --help                   Show this help\n");
  printf("  --version                Show version\n");
}

static int rane_streq(const char* a, const char* b) {
  if (!a || !b) return 0;
  return strcmp(a, b) == 0;
}

static int rane_parse_opt_level(const char* s, int* out_level) {
  if (!s || !out_level) return 0;
  if (rane_streq(s, "-O0")) { *out_level = 0; return 1; }
  if (rane_streq(s, "-O1")) { *out_level = 1; return 1; }
  if (rane_streq(s, "-O2")) { *out_level = 2; return 1; }
  if (rane_streq(s, "-O3")) { *out_level = 3; return 1; }
  return 0;
}

static int rane_cli_parse(int argc, char** argv, rane_cli_options_t* out) {
  if (!out) return 0;
  memset(out, 0, sizeof(*out));

  out->opt_level = 2;
  out->emit_c = 0;
  out->output_path = NULL;
  out->input_path = NULL;

  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];

    if (rane_streq(a, "--help") || rane_streq(a, "-h") || rane_streq(a, "/?")) {
      out->show_help = 1;
      return 1;
    }

    if (rane_streq(a, "--version")) {
      out->show_version = 1;
      return 1;
    }

    if (rane_streq(a, "-emit-c")) { out->emit_c = 1; continue; }
    if (rane_streq(a, "--run-selftest")) { out->run_selftest = 1; continue; }
    if (rane_streq(a, "--demo-pipeline")) { out->run_demo_pipeline = 1; continue; }

    if (rane_streq(a, "--dump-ast")) { out->dump_ast = 1; continue; }
    if (rane_streq(a, "--dump-tir")) { out->dump_tir = 1; continue; }
    if (rane_streq(a, "--dump-ssa")) { out->dump_ssa = 1; continue; }
    if (rane_streq(a, "--dump-regalloc")) { out->dump_regalloc = 1; continue; }

    if (rane_streq(a, "--lex")) { out->lex_only = 1; continue; }
    if (rane_streq(a, "--parse-only")) { out->parse_only = 1; continue; }
    if (rane_streq(a, "--typecheck-only")) { out->typecheck_only = 1; continue; }

    if (rane_streq(a, "--time")) { out->time_stages = 1; continue; }

    if (rane_streq(a, "--run")) { out->run_after = 1; continue; }
    if (rane_streq(a, "--emit-and-run")) { out->emit_and_run = 1; continue; }

    // OS tools
    if (rane_streq(a, "--os-info")) { out->os_info = 1; continue; }
    if (rane_streq(a, "--cwd")) { out->print_cwd = 1; continue; }

    if (rane_streq(a, "--env")) {
      if (i + 1 >= argc) return 0;
      out->env_name = argv[++i];
      continue;
    }

    if (rane_streq(a, "--cat")) {
      if (i + 1 >= argc) return 0;
      out->cat_path = argv[++i];
      continue;
    }

    if (rane_streq(a, "--sha256")) {
      if (i + 1 >= argc) return 0;
      out->sha256_path = argv[++i];
      continue;
    }

    if (rane_streq(a, "--args")) {
      if (i + 1 >= argc) return 0;
      out->run_args = argv[++i];
      continue;
    }

    int lvl = 0;
    if (rane_parse_opt_level(a, &lvl)) {
      out->opt_level = lvl;
      continue;
    }

    if (rane_streq(a, "-o")) {
      if (i + 1 >= argc) return 0;
      out->output_path = argv[++i];
      continue;
    }

    if (!out->input_path) {
      out->input_path = a;
      continue;
    }

    return 0;
  }

  return 1;
}

static int rane_read_entire_file(const char* path, char** out_buf, size_t* out_len) {
  if (out_buf) *out_buf = NULL;
  if (out_len) *out_len = 0;
  if (!path || !out_buf || !out_len) return 0;

  FILE* f = NULL;
  fopen_s(&f, path, "rb");
  if (!f) return 0;

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) { fclose(f); return 0; }

  char* buf = (char*)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return 0; }

  size_t rd = fread(buf, 1, (size_t)sz, f);
  fclose(f);

  buf[rd] = 0;
  *out_buf = buf;
  *out_len = rd;
  return 1;
}

// ---------------------------
// Optimized ABI: OS wrappers (C ABI, no exceptions)
// ---------------------------
extern "C" {

typedef struct rane_os_buf_s {
  void* data;
  size_t len;
} rane_os_buf_t;

static void rane_os_buf_free(rane_os_buf_t* b) {
  if (!b) return;
  if (b->data) HeapFree(GetProcessHeap(), 0, b->data);
  b->data = NULL;
  b->len = 0;
}

static int rane_os_read_file(const char* path, rane_os_buf_t* out) {
  if (out) { out->data = NULL; out->len = 0; }
  if (!path || !out) return 0;

  HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) return 0;

  LARGE_INTEGER sz;
  if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || (unsigned long long)sz.QuadPart > (unsigned long long)SIZE_MAX) {
    CloseHandle(h);
    return 0;
  }

  size_t len = (size_t)sz.QuadPart;
  void* mem = HeapAlloc(GetProcessHeap(), 0, len ? len : 1);
  if (!mem) {
    CloseHandle(h);
    return 0;
  }

  size_t off = 0;
  while (off < len) {
    DWORD chunk = 0;
    DWORD want = (DWORD)((len - off) > 0x7fffffffu ? 0x7fffffffu : (len - off));
    if (!ReadFile(h, (uint8_t*)mem + off, want, &chunk, NULL) || chunk == 0) {
      HeapFree(GetProcessHeap(), 0, mem);
      CloseHandle(h);
      return 0;
    }
    off += (size_t)chunk;
  }

  CloseHandle(h);
  out->data = mem;
  out->len = len;
  return 1;
}

static int rane_os_write_file(const char* path, const void* data, size_t len) {
  if (!path || (!data && len != 0)) return 0;

  HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) return 0;

  size_t off = 0;
  while (off < len) {
    DWORD chunk = 0;
    DWORD want = (DWORD)((len - off) > 0x7fffffffu ? 0x7fffffffu : (len - off));
    if (!WriteFile(h, (const uint8_t*)data + off, want, &chunk, NULL)) {
      CloseHandle(h);
      return 0;
    }
    off += (size_t)chunk;
  }

  CloseHandle(h);
  return 1;
}

static int rane_os_sha256_file(const char* path, uint8_t out32[32]) {
  if (!path || !out32) return 0;

  BCRYPT_ALG_HANDLE hAlg = NULL;
  BCRYPT_HASH_HANDLE hHash = NULL;
  uint8_t* hash_obj = NULL;
  DWORD hash_obj_len = 0;
  DWORD cb = 0;

  if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return 0;

  if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_obj_len, sizeof(hash_obj_len), &cb, 0) != 0) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  hash_obj = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, hash_obj_len ? hash_obj_len : 1);
  if (!hash_obj) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  if (BCryptCreateHash(hAlg, &hHash, (PUCHAR)hash_obj, hash_obj_len, NULL, 0, 0) != 0) {
    HeapFree(GetProcessHeap(), 0, hash_obj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE) {
    BCryptDestroyHash(hHash);
    HeapFree(GetProcessHeap(), 0, hash_obj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  uint8_t buf[64 * 1024];
  for (;;) {
    DWORD rd = 0;
    if (!ReadFile(f, buf, (DWORD)sizeof(buf), &rd, NULL)) {
      CloseHandle(f);
      BCryptDestroyHash(hHash);
      HeapFree(GetProcessHeap(), 0, hash_obj);
      BCryptCloseAlgorithmProvider(hAlg, 0);
      return 0;
    }
    if (rd == 0) break;
    if (BCryptHashData(hHash, (PUCHAR)buf, rd, 0) != 0) {
      CloseHandle(f);
      BCryptDestroyHash(hHash);
      HeapFree(GetProcessHeap(), 0, hash_obj);
      BCryptCloseAlgorithmProvider(hAlg, 0);
      return 0;
    }
  }

  CloseHandle(f);

  if (BCryptFinishHash(hHash, (PUCHAR)out32, 32, 0) != 0) {
    BCryptDestroyHash(hHash);
    HeapFree(GetProcessHeap(), 0, hash_obj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
  }

  BCryptDestroyHash(hHash);
  HeapFree(GetProcessHeap(), 0, hash_obj);
  BCryptCloseAlgorithmProvider(hAlg, 0);
  return 1;
}

typedef struct rane_os_proc_result_s {
  uint32_t exit_code;
} rane_os_proc_result_t;

static int rane_os_run_process(const char* exe_path, const char* args, rane_os_proc_result_t* out_res) {
  if (out_res) out_res->exit_code = 0;
  if (!exe_path) return 0;

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));
  si.cb = sizeof(si);

  // Build command line: "exe_path" + optional args.
  char cmd[4096];
  cmd[0] = 0;

  // Quote exe for CreateProcess parsing.
  if (strchr(exe_path, ' ') || strchr(exe_path, '\t')) {
    strcpy_s(cmd, sizeof(cmd), "\"");
    strcat_s(cmd, sizeof(cmd), exe_path);
    strcat_s(cmd, sizeof(cmd), "\"");
  } else {
    strcpy_s(cmd, sizeof(cmd), exe_path);
  }

  if (args && args[0]) {
    strcat_s(cmd, sizeof(cmd), " ");
    strcat_s(cmd, sizeof(cmd), args);
  }

  BOOL ok = CreateProcessA(
    NULL,
    cmd,
    NULL,
    NULL,
    FALSE,
    0,
    NULL,
    NULL,
    &si,
    &pi
  );

  if (!ok) return 0;

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exit_code = 0;
  GetExitCodeProcess(pi.hProcess, &exit_code);

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  if (out_res) out_res->exit_code = (uint32_t)exit_code;
  return 1;
}

} // extern "C"

// ---------------------------
// Existing compiler helpers
// ---------------------------
static void rane_dump_ast_minimal(const rane_stmt_t* root) {
  if (!root) {
    printf("AST: <null>\n");
    return;
  }

  // Minimal deterministic summary. (No full pretty-printer exists here yet.)
  // Root is usually STMT_BLOCK for a file.
  printf("AST: root kind=%d\n", (int)root->kind);
  if (root->kind == STMT_BLOCK) {
    printf("AST: top-level statements=%u\n", (unsigned)root->block.stmt_count);
  }
}

static void rane_dump_tir_minimal_with_histogram(const rane_tir_module_t* mod) {
  if (!mod) {
    printf("TIR: <null>\n");
    return;
  }

  printf("TIR: functions=%u\n", (unsigned)mod->function_count);

  // opcode histogram across the module
  uint32_t hist[256];
  memset(hist, 0, sizeof(hist));

  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    const rane_tir_function_t* f = &mod->functions[fi];
    printf("  fn %u: %s insts=%u stack_slots=%u\n",
           (unsigned)fi,
           f->name,
           (unsigned)f->inst_count,
           (unsigned)f->stack_slot_count);

    for (uint32_t ii = 0; ii < f->inst_count; ii++) {
      uint32_t op = (uint32_t)f->insts[ii].opcode;
      if (op < 256) hist[op]++;
    }
  }

  printf("TIR: opcode histogram (non-zero):\n");
  for (uint32_t i = 0; i < 256; i++) {
    if (hist[i] == 0) continue;
    // No opcode->string utility in public headers; print numeric opcode.
    printf("  op %u : %u\n", (unsigned)i, (unsigned)hist[i]);
  }
}

static rane_error_t rane_lex_file(const char* path) {
  char* buf = NULL;
  size_t len = 0;
  if (!rane_read_entire_file(path, &buf, &len)) {
    fprintf(stderr, "rane: failed to open %s\n", path);
    return RANE_E_OS_API_FAIL;
  }

  rane_lexer_t lex;
  rane_lexer_init(&lex, buf, len);

  for (;;) {
    rane_token_t t = rane_lexer_next(&lex);
    printf("%5u:%-4u  %-18s  len=%u  ",
           (unsigned)t.line,
           (unsigned)t.col,
           rane_token_type_str(t.type),
           (unsigned)t.length);

    // Print a truncated token preview for deterministic debug output.
    const uint32_t max_preview = 48;
    uint32_t n = t.length;
    if (n > max_preview) n = max_preview;

    printf("\"");
    for (uint32_t i = 0; i < n; i++) {
      char c = t.start ? t.start[i] : 0;
      if (c == '\r') { printf("\\r"); continue; }
      if (c == '\n') { printf("\\n"); continue; }
      if (c == '\t') { printf("\\t"); continue; }
      if ((unsigned char)c < 32 || (unsigned char)c >= 127) { printf("?"); continue; }
      printf("%c", c);
    }
    if (t.length > max_preview) printf("...");
    printf("\"\n");

    if (t.type == TOK_EOF || t.type == TOK_ERROR) break;
  }

  free(buf);
  return RANE_OK;
}

static rane_error_t rane_parse_typecheck_lower_for_file(
  const rane_cli_options_t* cli,
  rane_stmt_t** out_ast,
  rane_tir_module_t* out_tir
) {
  if (out_ast) *out_ast = NULL;
  if (out_tir) memset(out_tir, 0, sizeof(*out_tir));
  if (!cli || !cli->input_path) return RANE_E_INVALID_ARG;

  char* buf = NULL;
  size_t len = 0;
  if (!rane_read_entire_file(cli->input_path, &buf, &len)) {
    fprintf(stderr, "rane: failed to open %s\n", cli->input_path);
    return RANE_E_OS_API_FAIL;
  }

  rane_timer_t tm;
  if (cli->time_stages) rane_timer_start(&tm);

  rane_stmt_t* ast = nullptr;
  rane_diag_t diag = {};
  rane_error_t err = rane_parse_source_len_ex(buf, len, &ast, &diag);
  if (err != RANE_OK) {
    fprintf(stderr, "rane: parse failed (%u:%u): %s\n",
            (unsigned)diag.span.line, (unsigned)diag.span.col, diag.message);
    free(buf);
    return err;
  }

  double t_parse = cli->time_stages ? rane_timer_elapsed_ms(&tm) : 0.0;
  if (cli->time_stages) rane_timer_start(&tm);

  diag = {};
  err = rane_typecheck_ast_ex(ast, &diag);
  if (err != RANE_OK) {
    fprintf(stderr, "rane: typecheck failed (%u:%u): %s\n",
            (unsigned)diag.span.line, (unsigned)diag.span.col, diag.message);
    free(buf);
    return err;
  }

  double t_tc = cli->time_stages ? rane_timer_elapsed_ms(&tm) : 0.0;
  if (cli->time_stages) rane_timer_start(&tm);

  rane_tir_module_t tir_mod = {};
  err = rane_lower_ast_to_tir(ast, &tir_mod);
  if (err != RANE_OK) {
    fprintf(stderr, "rane: lowering failed (%d)\n", (int)err);
    free(buf);
    return err;
  }

  double t_lower = cli->time_stages ? rane_timer_elapsed_ms(&tm) : 0.0;

  if (cli->time_stages) {
    printf("time: parse=%.3fms typecheck=%.3fms lower=%.3fms\n", t_parse, t_tc, t_lower);
  }

  free(buf);

  if (out_ast) *out_ast = ast;
  if (out_tir) *out_tir = tir_mod;
  return RANE_OK;
}

static rane_error_t rane_compile_file_via_driver(const rane_cli_options_t* cli) {
  if (!cli || !cli->input_path) return RANE_E_INVALID_ARG;

  const char* out = cli->output_path;
  if (!out) out = cli->emit_c ? "out.c" : "a.exe";

  rane_driver_options_t opts;
  opts.input_path = cli->input_path;
  opts.output_path = out;
  opts.opt_level = cli->opt_level;

  rane_timer_t tm;
  if (cli->time_stages) rane_timer_start(&tm);

  rane_error_t err = cli->emit_c ? rane_compile_file_to_c(&opts)
                                 : rane_compile_file_to_exe(&opts);

  if (cli->time_stages) {
    printf("time: driver_compile=%.3fms\n", rane_timer_elapsed_ms(&tm));
  }

  if (err != RANE_OK) {
    fprintf(stderr, "rane: compile failed (%d)\n", (int)err);
    return err;
  }

  printf("rane: wrote %s\n", out);
  return RANE_OK;
}

static void rane_print_os_info() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);

  MEMORYSTATUSEX ms;
  memset(&ms, 0, sizeof(ms));
  ms.dwLength = sizeof(ms);
  GlobalMemoryStatusEx(&ms);

  printf("OS: Windows\n");
  printf("CPU: processors=%lu page_size=%lu alloc_granularity=%lu\n",
         (unsigned long)si.dwNumberOfProcessors,
         (unsigned long)si.dwPageSize,
         (unsigned long)si.dwAllocationGranularity);

  printf("MEM: phys=%lluMB avail_phys=%lluMB commit_limit=%lluMB commit_avail=%lluMB\n",
         (unsigned long long)(ms.ullTotalPhys / (1024ull * 1024ull)),
         (unsigned long long)(ms.ullAvailPhys / (1024ull * 1024ull)),
         (unsigned long long)(ms.ullTotalPageFile / (1024ull * 1024ull)),
         (unsigned long long)(ms.ullAvailPageFile / (1024ull * 1024ull)));
}

static rane_error_t rane_print_cwd() {
  char buf[MAX_PATH];
  DWORD n = GetCurrentDirectoryA((DWORD)sizeof(buf), buf);
  if (n == 0 || n >= (DWORD)sizeof(buf)) return RANE_E_OS_API_FAIL;
  printf("%s\n", buf);
  return RANE_OK;
}

static rane_error_t rane_print_env(const char* name) {
  if (!name) return RANE_E_INVALID_ARG;
  char buf[32768];
  DWORD n = GetEnvironmentVariableA(name, buf, (DWORD)sizeof(buf));
  if (n == 0) return RANE_E_OS_API_FAIL;
  if (n >= (DWORD)sizeof(buf)) return RANE_E_OS_API_FAIL;
  printf("%s\n", buf);
  return RANE_OK;
}

static rane_error_t rane_cat_file(const char* path) {
  if (!path) return RANE_E_INVALID_ARG;

  HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) return RANE_E_OS_API_FAIL;

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out == INVALID_HANDLE_VALUE) {
    CloseHandle(h);
    return RANE_E_OS_API_FAIL;
  }

  uint8_t buf[64 * 1024];
  for (;;) {
    DWORD rd = 0;
    if (!ReadFile(h, buf, (DWORD)sizeof(buf), &rd, NULL)) {
      CloseHandle(h);
      return RANE_E_OS_API_FAIL;
    }
    if (rd == 0) break;

    DWORD wr = 0;
    if (!WriteFile(out, buf, rd, &wr, NULL) || wr != rd) {
      CloseHandle(h);
      return RANE_E_OS_API_FAIL;
    }
  }

  CloseHandle(h);
  return RANE_OK;
}

static rane_error_t rane_sha256_file_cmd(const char* path) {
  uint8_t h[32];
  if (!rane_os_sha256_file(path, h)) {
    fprintf(stderr, "rane: sha256 failed for %s\n", path ? path : "<null>");
    return RANE_E_OS_API_FAIL;
  }

  for (int i = 0; i < 32; i++) printf("%02x", (unsigned)h[i]);
  printf("  %s\n", path);
  return RANE_OK;
}

static rane_error_t rane_run_exe(const char* exe_path, const char* args) {
  if (!exe_path) return RANE_E_INVALID_ARG;

  rane_os_proc_result_t r = {};
  if (!rane_os_run_process(exe_path, args, &r)) {
    DWORD gle = GetLastError();
    fprintf(stderr, "rane: failed to run %s (GetLastError=%lu)\n", exe_path, (unsigned long)gle);
    return RANE_E_OS_API_FAIL;
  }

  printf("rane: program exit code %lu\n", (unsigned long)r.exit_code);
  return (r.exit_code == 0) ? RANE_OK : RANE_E_UNKNOWN;
}

int main(int argc, char** argv) {
  rane_cli_options_t cli;
  if (!rane_cli_parse(argc, argv, &cli)) {
    rane_print_help();
    return 2;
  }

  if (cli.show_version) { rane_print_version(); return 0; }
  if (cli.show_help) { rane_print_help(); return 0; }

  if (cli.os_info) {
    rane_print_os_info();
    return 0;
  }

  if (cli.print_cwd) {
    return (rane_print_cwd() == RANE_OK) ? 0 : 1;
  }

  if (cli.env_name) {
    return (rane_print_env(cli.env_name) == RANE_OK) ? 0 : 1;
  }

  if (cli.cat_path) {
    return (rane_cat_file(cli.cat_path) == RANE_OK) ? 0 : 1;
  }

  if (cli.sha256_path) {
    return (rane_sha256_file_cmd(cli.sha256_path) == RANE_OK) ? 0 : 1;
  }

  if (cli.run_selftest) {
    rane_gc_run_selftest();
  }

  if (cli.run_demo_pipeline) {
    const char* source = "let x = 42;";
    rane_stmt_t* ast = nullptr;
    rane_diag_t diag = {};
    rane_error_t err = rane_parse_source_ex(source, &ast, &diag);
    if (err != RANE_OK) {
      fprintf(stderr, "rane: parse failed (%u:%u): %s\n",
              (unsigned)diag.span.line, (unsigned)diag.span.col, diag.message);
      return 1;
    }
    diag = {};
    err = rane_typecheck_ast_ex(ast, &diag);
    if (err != RANE_OK) {
      fprintf(stderr, "rane: typecheck failed (%u:%u): %s\n",
              (unsigned)diag.span.line, (unsigned)diag.span.col, diag.message);
      return 1;
    }
    rane_tir_module_t tir_mod = {};
    err = rane_lower_ast_to_tir(ast, &tir_mod);
    if (err != RANE_OK) return 1;

    err = rane_build_ssa(&tir_mod);
    if (err != RANE_OK) return 1;

    err = rane_allocate_registers(&tir_mod);
    if (err != RANE_OK) return 1;

    err = rane_optimize_tir(&tir_mod);
    if (err != RANE_OK) return 1;

    printf("rane: demo pipeline completed.\n");
    rane_dump_tir_minimal_with_histogram(&tir_mod);
    return 0;
  }

  if (!cli.input_path) {
    rane_print_help();
    return 2;
  }

  // --run: run an existing executable path directly.
  if (cli.run_after && !cli.emit_and_run) {
    rane_error_t err = rane_run_exe(cli.input_path, cli.run_args);
    return (err == RANE_OK) ? 0 : 1;
  }

  // --lex
  if (cli.lex_only) {
    rane_error_t err = rane_lex_file(cli.input_path);
    return (err == RANE_OK) ? 0 : 1;
  }

  // parse/typecheck only
  if (cli.parse_only || cli.typecheck_only || cli.dump_ast || cli.dump_tir || cli.dump_ssa || cli.dump_regalloc) {
    rane_stmt_t* ast = nullptr;
    rane_tir_module_t tir_mod = {};
    rane_error_t err = rane_parse_typecheck_lower_for_file(&cli, &ast, &tir_mod);
    if (err != RANE_OK) return 1;

    if (cli.dump_ast) rane_dump_ast_minimal(ast);

    if (cli.parse_only) {
      printf("rane: parse ok\n");
      return 0;
    }

    if (cli.typecheck_only) {
      printf("rane: typecheck ok\n");
      return 0;
    }

    if (cli.dump_tir) rane_dump_tir_minimal_with_histogram(&tir_mod);

    if (cli.dump_ssa) {
      err = rane_build_ssa(&tir_mod);
      if (err != RANE_OK) return 1;
      printf("SSA: built\n");
    }

    if (cli.dump_regalloc) {
      err = rane_build_ssa(&tir_mod);
      if (err != RANE_OK) return 1;
      err = rane_allocate_registers(&tir_mod);
      if (err != RANE_OK) return 1;
      printf("Regalloc: completed\n");
    }

    return 0;
  }

  // Default compile
  rane_error_t err = rane_compile_file_via_driver(&cli);
  if (err != RANE_OK) return 1;

  if (cli.emit_and_run) {
    if (cli.emit_c) {
      fprintf(stderr, "rane: --emit-and-run requires exe output (omit -emit-c)\n");
      return 1;
    }
    const char* out = cli.output_path ? cli.output_path : "a.exe";
    err = rane_run_exe(out, cli.run_args);
    return (err == RANE_OK) ? 0 : 1;
  }

  return 0;
}
