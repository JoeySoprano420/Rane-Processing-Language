#include "rane_c_backend.h"

#include "rane_file.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

static void c_append(char** buf, size_t* len, size_t* cap, const char* s) {
  if (!buf || !len || !cap || !s) return;
  size_t sl = strlen(s);
  if (*len + sl + 1 > *cap) {
    size_t nc = (*cap == 0) ? 1024 : *cap;
    while (*len + sl + 1 > nc) nc *= 2;
    char* nb = (char*)realloc(*buf, nc);
    if (!nb) return;
    *buf = nb;
    *cap = nc;
  }
  memcpy(*buf + *len, s, sl);
  *len += sl;
  (*buf)[*len] = 0;
}

static void c_appendf(char** buf, size_t* len, size_t* cap, const char* fmt, ...) {
  if (!buf || !len || !cap || !fmt) return;
  va_list ap;
  va_start(ap, fmt);

  char tmp[1024];
#if defined(_WIN32)
  int n = vsnprintf_s(tmp, sizeof(tmp), _TRUNCATE, fmt, ap);
#else
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
#endif
  va_end(ap);

  if (n < 0) return;
  tmp[sizeof(tmp) - 1] = 0;
  c_append(buf, len, cap, tmp);
}

static const char* c_reg_name(uint32_t r, char* tmp, size_t tmp_cap) {
  if (!tmp || tmp_cap == 0) return "r0";
#if defined(_WIN32)
  sprintf_s(tmp, tmp_cap, "r%u", (unsigned)r);
#else
  snprintf(tmp, tmp_cap, "r%u", (unsigned)r);
#endif
  return tmp;
}

static void c_escape_c_string(const char* in, char* out, size_t out_cap) {
  if (!out || out_cap == 0) return;
  out[0] = 0;
  if (!in) return;

  size_t w = 0;
  for (size_t i = 0; in[i] && w + 2 < out_cap; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c == '\\' || c == '"') {
      if (w + 2 >= out_cap) break;
      out[w++] = '\\';
      out[w++] = (char)c;
    } else if (c == '\n') {
      if (w + 2 >= out_cap) break;
      out[w++] = '\\';
      out[w++] = 'n';
    } else if (c == '\r') {
      if (w + 2 >= out_cap) break;
      out[w++] = '\\';
      out[w++] = 'r';
    } else if (c == '\t') {
      if (w + 2 >= out_cap) break;
      out[w++] = '\\';
      out[w++] = 't';
    } else if (c < 32 || c > 126) {
      // use \xHH
      if (w + 4 >= out_cap) break;
      const char* hex = "0123456789ABCDEF";
      out[w++] = '\\';
      out[w++] = 'x';
      out[w++] = hex[(c >> 4) & 0xF];
      out[w++] = hex[c & 0xF];
    } else {
      out[w++] = (char)c;
    }
  }
  out[w] = 0;
}

static rane_error_t emit_function(char** out, size_t* out_len, size_t* out_cap, const rane_tir_function_t* f) {
  if (!out || !out_len || !out_cap || !f) return RANE_E_INVALID_ARG;

  c_appendf(out, out_len, out_cap, "static uint64_t %s(void) {\n", f->name);

  c_append(out, out_len, out_cap, "  uint64_t r[256] = {0};\n");
  c_appendf(out, out_len, out_cap, "  uint64_t s[%u] = {0};\n", (unsigned)(f->stack_slot_count ? f->stack_slot_count : 1));

  // Win64 call ABI scratch (also fine on non-Windows C compilers; unused unless calls occur)
  c_append(out, out_len, out_cap, "  uint64_t call_arg_bytes = 0;\n");
  c_append(out, out_len, out_cap, "  uint64_t* call_stack = NULL;\n");
  c_append(out, out_len, out_cap, "  size_t call_stack_words = 0;\n");

  for (uint32_t pc = 0; pc < f->inst_count; pc++) {
    const rane_tir_inst_t* in = &f->insts[pc];
    if (in->opcode == TIR_LABEL && in->operand_count == 1 && in->operands[0].kind == TIR_OPERAND_LBL) {
      c_appendf(out, out_len, out_cap, "%s:\n", in->operands[0].lbl);
      c_append(out, out_len, out_cap, "  ;\n");
      continue;
    }

    switch (in->opcode) {
      case TIR_NOP:
        break;

      case TIR_CALL_PREP: {
        if (in->operand_count < 1 || in->operands[0].kind != TIR_OPERAND_IMM) break;
        c_appendf(out, out_len, out_cap, "  call_arg_bytes = (uint64_t)%lluull;\n", (unsigned long long)in->operands[0].imm);
        c_append(out, out_len, out_cap, "  if (call_stack) { free(call_stack); call_stack = NULL; call_stack_words = 0; }\n");
        c_append(out, out_len, out_cap, "  call_stack_words = (size_t)(call_arg_bytes / 8ull);\n");
        c_append(out, out_len, out_cap, "  if (call_stack_words) { call_stack = (uint64_t*)calloc(call_stack_words, sizeof(uint64_t)); }\n");
        break;
      }

      case TIR_MOV: {
        if (in->operand_count != 2) break;
        if (in->operands[0].kind == TIR_OPERAND_R) {
          uint32_t dst = in->operands[0].r;
          if (in->operands[1].kind == TIR_OPERAND_R) {
            uint32_t src = in->operands[1].r;
            c_appendf(out, out_len, out_cap, "  r[%u] = r[%u];\n", (unsigned)dst, (unsigned)src);
          } else if (in->operands[1].kind == TIR_OPERAND_IMM) {
            if (in->type == RANE_TYPE_P64) {
              // Heuristic: treat imm as pointer to string literal created by parser.
              const char* s = (const char*)(uintptr_t)in->operands[1].imm;
              if (s && s[0]) {
                char esc[2048];
                c_escape_c_string(s, esc, sizeof(esc));
                c_appendf(out, out_len, out_cap, "  r[%u] = (uint64_t)(uintptr_t)\"%s\";\n", (unsigned)dst, esc);
              } else {
                c_appendf(out, out_len, out_cap, "  r[%u] = 0;\n", (unsigned)dst);
              }
            } else {
              c_appendf(out, out_len, out_cap, "  r[%u] = (uint64_t)%llu;\n", (unsigned)dst, (unsigned long long)in->operands[1].imm);
            }
          }
        }
        break;
      }

      case TIR_LD: {
        if (in->operand_count != 2) break;
        if (in->operands[0].kind != TIR_OPERAND_R) break;
        if (in->operands[1].kind != TIR_OPERAND_S) break;
        c_appendf(out, out_len, out_cap, "  r[%u] = s[%u];\n", (unsigned)in->operands[0].r, (unsigned)in->operands[1].s);
        break;
      }

      case TIR_ST: {
        if (in->operand_count != 2) break;
        if (in->operands[0].kind != TIR_OPERAND_S) break;
        if (in->operands[1].kind != TIR_OPERAND_R) break;

        uint32_t slot = in->operands[0].s;
        uint32_t src = in->operands[1].r;

        // Special stack-arg encoding: 0x80000000 | arg_index (0..)
        if ((slot & 0x80000000u) != 0) {
          uint32_t argi = (slot & 0x7FFFFFFFu);
          c_append(out, out_len, out_cap, "  if (call_stack) {\n");
          c_appendf(out, out_len, out_cap, "    if ((size_t)%u < call_stack_words) call_stack[%u] = r[%u];\n", (unsigned)argi, (unsigned)argi, (unsigned)src);
          c_append(out, out_len, out_cap, "  }\n");
        } else {
          c_appendf(out, out_len, out_cap, "  s[%u] = r[%u];\n", (unsigned)slot, (unsigned)src);
        }
        break;
      }

      case TIR_ADD:
      case TIR_SUB:
      case TIR_MUL:
      case TIR_DIV:
      case TIR_AND:
      case TIR_OR:
      case TIR_XOR:
      case TIR_SHL:
      case TIR_SHR:
      case TIR_SAR: {
        if (in->operand_count != 2) break;
        if (in->operands[0].kind != TIR_OPERAND_R) break;
        uint32_t dst = in->operands[0].r;

        // For now only support RHS reg/imm.
        if (in->operands[1].kind == TIR_OPERAND_R) {
          uint32_t rhs = in->operands[1].r;
          const char* op = NULL;
          switch (in->opcode) {
            case TIR_ADD: op = "+"; break;
            case TIR_SUB: op = "-"; break;
            case TIR_MUL: op = "*"; break;
            case TIR_DIV: op = "/"; break;
            case TIR_AND: op = "&"; break;
            case TIR_OR:  op = "|"; break;
            case TIR_XOR: op = "^"; break;
            default: break;
          }

          if (in->opcode == TIR_SHL) {
            c_appendf(out, out_len, out_cap, "  r[%u] = r[%u] << (r[%u] & 63ull);\n", (unsigned)dst, (unsigned)dst, (unsigned)rhs);
          } else if (in->opcode == TIR_SHR) {
            c_appendf(out, out_len, out_cap, "  r[%u] = r[%u] >> (r[%u] & 63ull);\n", (unsigned)dst, (unsigned)dst, (unsigned)rhs);
          } else if (in->opcode == TIR_SAR) {
            c_appendf(out, out_len, out_cap, "  r[%u] = (uint64_t)(((int64_t)r[%u]) >> (r[%u] & 63ull));\n", (unsigned)dst, (unsigned)dst, (unsigned)rhs);
          } else if (op) {
            c_appendf(out, out_len, out_cap, "  r[%u] = r[%u] %s r[%u];\n", (unsigned)dst, (unsigned)dst, op, (unsigned)rhs);
          }
        } else if (in->operands[1].kind == TIR_OPERAND_IMM) {
          unsigned long long imm = (unsigned long long)in->operands[1].imm;
          if (in->opcode == TIR_SHL) {
            c_appendf(out, out_len, out_cap, "  r[%u] = r[%u] << (%lluull & 63ull);\n", (unsigned)dst, (unsigned)dst, imm);
          } else if (in->opcode == TIR_SHR) {
            c_appendf(out, out_len, out_cap, "  r[%u] = r[%u] >> (%lluull & 63ull);\n", (unsigned)dst, (unsigned)dst, imm);
          } else if (in->opcode == TIR_SAR) {
            c_appendf(out, out_len, out_cap, "  r[%u] = (uint64_t)(((int64_t)r[%u]) >> (%lluull & 63ull));\n", (unsigned)dst, (unsigned)dst, imm);
          } else {
            const char* op = NULL;
            switch (in->opcode) {
              case TIR_ADD: op = "+"; break;
              case TIR_SUB: op = "-"; break;
              case TIR_MUL: op = "*"; break;
              case TIR_DIV: op = "/"; break;
              case TIR_AND: op = "&"; break;
              case TIR_OR:  op = "|"; break;
              case TIR_XOR: op = "^"; break;
              default: break;
            }
            if (op) {
              c_appendf(out, out_len, out_cap, "  r[%u] = r[%u] %s (uint64_t)%lluull;\n", (unsigned)dst, (unsigned)dst, op, imm);
            }
          }
        }
        break;
      }

      case TIR_CMP:
        // Flags are implicit in x64; for C backend we handle only the common lowering pattern,
        // where CMP is immediately followed by JCC_EXT.
        // We'll ignore CMP here and let JCC_EXT re-evaluate as needed.
        break;

      case TIR_JMP: {
        if (in->operand_count != 1 || in->operands[0].kind != TIR_OPERAND_LBL) break;
        c_appendf(out, out_len, out_cap, "  goto %s;\n", in->operands[0].lbl);
        break;
      }

      case TIR_JCC_EXT: {
        // Very small: assume previous inst is CMP rX, imm or CMP rX, rY.
        if (in->operand_count != 2) break;
        if (in->operands[0].kind != TIR_OPERAND_IMM) break;
        if (in->operands[1].kind != TIR_OPERAND_LBL) break;

        if (pc == 0) return RANE_E_INVALID_ARG;
        const rane_tir_inst_t* prev = &f->insts[pc - 1];
        if (prev->opcode != TIR_CMP || prev->operand_count != 2 || prev->operands[0].kind != TIR_OPERAND_R) {
          return RANE_E_INVALID_ARG;
        }

        uint32_t lhs = prev->operands[0].r;
        const char* label = in->operands[1].lbl;
        rane_tir_cc_t cc = (rane_tir_cc_t)in->operands[0].imm;

        // rhs can be reg or imm.
        char rhs_expr[64];
        rhs_expr[0] = 0;
        if (prev->operands[1].kind == TIR_OPERAND_R) {
#if defined(_WIN32)
          sprintf_s(rhs_expr, sizeof(rhs_expr), "r[%u]", (unsigned)prev->operands[1].r);
#else
          snprintf(rhs_expr, sizeof(rhs_expr), "r[%u]", (unsigned)prev->operands[1].r);
#endif
        } else if (prev->operands[1].kind == TIR_OPERAND_IMM) {
#if defined(_WIN32)
          sprintf_s(rhs_expr, sizeof(rhs_expr), "%lluull", (unsigned long long)prev->operands[1].imm);
#else
          snprintf(rhs_expr, sizeof(rhs_expr), "%lluull", (unsigned long long)prev->operands[1].imm);
#endif
        } else {
          return RANE_E_INVALID_ARG;
        }

        const char* cmpop = "==";
        switch (cc) {
          case TIR_CC_E:  cmpop = "=="; break;
          case TIR_CC_NE: cmpop = "!="; break;
          case TIR_CC_L:  cmpop = "<"; break;
          case TIR_CC_LE: cmpop = "<="; break;
          case TIR_CC_G:  cmpop = ">"; break;
          case TIR_CC_GE: cmpop = ">="; break;
          default: cmpop = "=="; break;
        }

        c_appendf(out, out_len, out_cap, "  if ((int64_t)r[%u] %s (int64_t)(%s)) goto %s;\n", (unsigned)lhs, cmpop, rhs_expr, label);
        break;
      }

      case TIR_CALL_IMPORT:
      case TIR_CALL_LOCAL: {
        if (in->operand_count != 1 || in->operands[0].kind != TIR_OPERAND_LBL) return RANE_E_INVALID_ARG;
        const char* target = in->operands[0].lbl;

        // Prepare a shadow area + stack args. We model it with a local struct so we can pass stack args
        // by value through a function pointer call where needed. For now, only support 0..4 args.
        // Stack args beyond 4 are not supported in this backend yet.
        c_append(out, out_len, out_cap, "  {\n");
        c_append(out, out_len, out_cap, "    // args in r[1], r[2], r[8], r[9] (Win64)\n");

        if (in->opcode == TIR_CALL_IMPORT && strcmp(target, "rane_rt_print") == 0) {
          c_append(out, out_len, out_cap, "    extern void rane_rt_print(const char*);\n");
          c_append(out, out_len, out_cap, "    rane_rt_print((const char*)(uintptr_t)r[1]);\n");
          c_append(out, out_len, out_cap, "    r[0] = 0;\n");
          c_append(out, out_len, out_cap, "  }\n");
          break;
        }

        // For general calls, only support 0..4 register args. If stack args were requested, reject.
        c_append(out, out_len, out_cap, "    if (call_stack_words != 0) { /* stack args unsupported in C backend for general calls */ return (uint64_t)0; }\n");

        // Emit call based on whether it's local or import (both are plain C symbols here)
        c_appendf(out, out_len, out_cap, "    extern uint64_t %s(void);\n", target);
        // If there are register args, we need a function pointer cast. We'll infer arg count by checking which regs are non-zero is not safe.
        // Instead, treat as 0-arg call for now except rane_rt_print.
        c_appendf(out, out_len, out_cap, "    r[0] = %s();\n", target);
        c_append(out, out_len, out_cap, "  }\n");
        break;
      }

      case TIR_RET:
        c_append(out, out_len, out_cap, "  if (call_stack) free(call_stack);\n");
        c_append(out, out_len, out_cap, "  return r[0];\n");
        break;

      case TIR_RET_VAL:
        if (in->operand_count == 1 && in->operands[0].kind == TIR_OPERAND_R) {
          c_append(out, out_len, out_cap, "  if (call_stack) free(call_stack);\n");
          c_appendf(out, out_len, out_cap, "  return r[%u];\n", (unsigned)in->operands[0].r);
        } else {
          c_append(out, out_len, out_cap, "  if (call_stack) free(call_stack);\n");
          c_append(out, out_len, out_cap, "  return r[0];\n");
        }
        break;

      default:
        return RANE_E_INVALID_ARG;
    }
  }

  c_append(out, out_len, out_cap, "  if (call_stack) free(call_stack);\n");
  c_append(out, out_len, out_cap, "  return r[0];\n");
  c_append(out, out_len, out_cap, "}\n\n");
  return RANE_OK;
}

rane_error_t rane_emit_c_from_tir(const rane_tir_module_t* mod, const rane_c_backend_options_t* opts) {
  if (!mod || !opts || !opts->output_c_path) return RANE_E_INVALID_ARG;

  char* out = NULL;
  size_t out_len = 0;
  size_t out_cap = 0;

  c_append(&out, &out_len, &out_cap, "// Generated by RANE C backend (bootstrap)\n");
  c_append(&out, &out_len, &out_cap, "#include <stdint.h>\n#include <stddef.h>\n#include <inttypes.h>\n#include <stdio.h>\n#include <stdlib.h>\n\n");
  c_append(&out, &out_len, &out_cap, "#include \"rane_rt.h\"\n\n");

  // Emit forward decls
  for (uint32_t i = 0; i < mod->function_count; i++) {
    c_appendf(&out, &out_len, &out_cap, "static uint64_t %s(void);\n", mod->functions[i].name);
  }
  c_append(&out, &out_len, &out_cap, "\n");

  // Emit functions
  for (uint32_t i = 0; i < mod->function_count; i++) {
    rane_error_t e = emit_function(&out, &out_len, &out_cap, &mod->functions[i]);
    if (e != RANE_OK) {
      free(out);
      return e;
    }
  }

  c_append(&out, &out_len, &out_cap, "int main(int argc, char** argv) { (void)argc; (void)argv; (void)main(); return 0; }\n");

  int wr = rane_file_write_all(opts->output_c_path, out, out_len);
  free(out);
  if (wr != 0) return RANE_E_OS_API_FAIL;
  return RANE_OK;
}
