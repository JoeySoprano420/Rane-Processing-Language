#include "rane_x64.h"
#include "rane_aot.h"
#include <string.h>
#include <assert.h>

void rane_x64_emit_bytes(rane_x64_emitter_t* e, const uint8_t* bytes, uint64_t count);

// Register codes (low 3 bits)
static uint8_t reg_code(uint8_t reg) {
  // Assume reg 0-15: RAX=0, RCX=1, RDX=2, RBX=3, RSP=4, RBP=5, RSI=6, RDI=7, R8=8, etc.
  return reg & 0x7;
}

// REX byte for 64-bit
static uint8_t rex_w() { return 0x48; }
static uint8_t rex_w_r(uint8_t r) { return 0x48 | ((r >> 3) & 0x1); } // for reg in opcode
static uint8_t rex_w_rm(uint8_t reg, uint8_t rm) { return 0x48 | (((reg >> 3) & 0x1) << 2) | ((rm >> 3) & 0x1); }

// ModRM byte
static uint8_t modrm_byte(uint8_t mod, uint8_t reg, uint8_t rm) {
  return (mod << 6) | (reg << 3) | rm;
}

static uint32_t align_up_u32(uint32_t v, uint32_t a) {
  return (v + (a - 1)) & ~(a - 1);
}

void rane_x64_init_emitter(rane_x64_emitter_t* e, uint8_t* buf, uint64_t size) {
  e->buffer = buf;
  e->buffer_size = size;
  e->offset = 0;
}

int rane_x64_ensure_space(rane_x64_emitter_t* e, uint64_t needed) {
  if (e->offset + needed > e->buffer_size) return 0;
  return 1;
}

// Emit bytes
void rane_x64_emit_bytes(rane_x64_emitter_t* e, const uint8_t* bytes, uint64_t count) {
  if (!rane_x64_ensure_space(e, count)) return; // error, but for now ignore
  memcpy(e->buffer + e->offset, bytes, count);
  e->offset += count;
}

void rane_x64_emit_mov_reg_imm64(rane_x64_emitter_t* e, uint8_t reg, uint64_t imm) {
  uint8_t rex = rex_w_r(reg);
  uint8_t opcode = 0xB8 + reg_code(reg);
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&imm, 8);
}

void rane_x64_emit_mov_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  uint8_t rex = rex_w_rm(src_reg, dst_reg); // reg=src, rm=dst
  uint8_t opcode = 0x89;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(src_reg), reg_code(dst_reg)); // mod=11 for reg-reg
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_mov_reg_mem(rane_x64_emitter_t* e, uint8_t reg, uint8_t base_reg, int32_t disp) {
  uint8_t rex = rex_w_rm(reg, base_reg);
  uint8_t opcode = 0x8B;
  uint8_t mod = (disp == 0) ? 0x0 : 0x2; // use disp32 when non-zero
  uint8_t modrm = modrm_byte(mod, reg_code(reg), reg_code(base_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  if (mod == 0x2) rane_x64_emit_bytes(e, (uint8_t*)&disp, 4);
}

void rane_x64_emit_mov_mem_reg(rane_x64_emitter_t* e, uint8_t base_reg, int32_t disp, uint8_t reg) {
  uint8_t rex = rex_w_rm(reg, base_reg);
  uint8_t opcode = 0x89;
  uint8_t mod = (disp == 0) ? 0x0 : 0x2;
  uint8_t modrm_val = modrm_byte(mod, reg_code(reg), reg_code(base_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
  if (mod == 0x2) rane_x64_emit_bytes(e, (uint8_t*)&disp, 4);
}

// [RSP + disp32] = reg
void rane_x64_emit_mov_rsp_disp_reg(rane_x64_emitter_t* e, int32_t disp, uint8_t reg) {
  // mov [rsp+disp32], r64 => REX.W + 89 /r with ModRM rm=100 (SIB), mod=10, plus SIB(0x24), disp32
  uint8_t rex = rex_w_rm(reg, 4 /*RSP*/);
  uint8_t op = 0x89;
  uint8_t modrm = modrm_byte(0x2, reg_code(reg), 0x4);
  uint8_t sib = 0x24;
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_bytes(e, &sib, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&disp, 4);
}

void rane_x64_emit_add_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  uint8_t rex = rex_w_rm(src_reg, dst_reg);
  uint8_t opcode = 0x01;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(src_reg), reg_code(dst_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_sub_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  uint8_t rex = rex_w_rm(src_reg, dst_reg);
  uint8_t opcode = 0x29;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(src_reg), reg_code(dst_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_cmp_reg_reg(rane_x64_emitter_t* e, uint8_t reg1, uint8_t reg2) {
  uint8_t rex = rex_w_rm(reg2, reg1);
  uint8_t opcode = 0x39;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(reg2), reg_code(reg1));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_jmp_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode = 0xE9;
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_je_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode1 = 0x0F;
  uint8_t opcode2 = 0x84;
  rane_x64_emit_bytes(e, &opcode1, 1);
  rane_x64_emit_bytes(e, &opcode2, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_jne_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode1 = 0x0F;
  uint8_t opcode2 = 0x85;
  rane_x64_emit_bytes(e, &opcode1, 1);
  rane_x64_emit_bytes(e, &opcode2, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_call_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode = 0xE8;
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_ret(rane_x64_emitter_t* e) {
  uint8_t opcode = 0xC3;
  rane_x64_emit_bytes(e, &opcode, 1);
}

void rane_x64_emit_mul_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  (void)src_reg;
  // IMUL r/m64 via /5, uses rm operand for multiplicand. For bootstrap ops we only need r0*=r1 style.
  uint8_t rex = rex_w_rm(dst_reg, dst_reg);
  uint8_t opcode = 0xF7;
  uint8_t modrm_val = modrm_byte(0x3, 0x5, reg_code(dst_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_div_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  (void)dst_reg;
  // DIV is tricky; bootstrap keeps existing behavior (assumes RAX has dividend).
  uint8_t rex = rex_w_r(src_reg);
  uint8_t opcode = 0xF7;
  uint8_t modrm_val = modrm_byte(0x3, 0x6, reg_code(src_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_cmp_reg_imm(rane_x64_emitter_t* e, uint8_t reg, uint64_t imm) {
  uint8_t rex = rex_w_r(reg);
  uint8_t opcode = 0x81;
  uint8_t modrm_val = modrm_byte(0x3, 0x7, reg_code(reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&imm, 4);
}

// push r64: 0x50 + reg (low regs) + optional REX for r8-r15
void rane_x64_emit_push_reg(rane_x64_emitter_t* e, uint8_t reg) {
  uint8_t op = (uint8_t)(0x50 + reg_code(reg));
  if (reg >= 8) {
    uint8_t rex = 0x41;
    rane_x64_emit_bytes(e, &rex, 1);
  }
  rane_x64_emit_bytes(e, &op, 1);
}

void rane_x64_emit_pop_reg(rane_x64_emitter_t* e, uint8_t reg) {
  uint8_t op = (uint8_t)(0x58 + reg_code(reg));
  if (reg >= 8) {
    uint8_t rex = 0x41;
    rane_x64_emit_bytes(e, &rex, 1);
  }
  rane_x64_emit_bytes(e, &op, 1);
}

void rane_x64_emit_sub_rsp_imm32(rane_x64_emitter_t* e, uint32_t imm) {
  // sub rsp, imm32 => 48 81 EC imm32
  uint8_t rex = 0x48;
  uint8_t op = 0x81;
  uint8_t modrm = 0xEC;
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&imm, 4);
}

void rane_x64_emit_add_rsp_imm32(rane_x64_emitter_t* e, uint32_t imm) {
  // add rsp, imm32 => 48 81 C4 imm32
  uint8_t rex = 0x48;
  uint8_t op = 0x81;
  uint8_t modrm = 0xC4;
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&imm, 4);
}

void rane_x64_emit_sub_rsp_imm8(rane_x64_emitter_t* e, uint8_t imm) {
  // sub rsp, imm8 => 48 83 EC imm8
  uint8_t rex = 0x48;
  uint8_t op = 0x83;
  uint8_t modrm = 0xEC;
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_bytes(e, &imm, 1);
}

void rane_x64_emit_add_rsp_imm8(rane_x64_emitter_t* e, uint8_t imm) {
  // add rsp, imm8 => 48 83 C4 imm8
  uint8_t rex = 0x48;
  uint8_t op = 0x83;
  uint8_t modrm = 0xC4;
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_bytes(e, &imm, 1);
}

void rane_x64_emit_call_placeholder(rane_x64_emitter_t* e) {
  // 9-byte placeholder so the PE patcher can replace it with:
  //   48 8B 05 disp32
  //   FF D0
  uint8_t nop = 0x90;
  for (int i = 0; i < 9; i++) rane_x64_emit_bytes(e, &nop, 1);
}

void rane_x64_emit_jl_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode1 = 0x0F;
  uint8_t opcode2 = 0x8C;
  rane_x64_emit_bytes(e, &opcode1, 1);
  rane_x64_emit_bytes(e, &opcode2, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_jle_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode1 = 0x0F;
  uint8_t opcode2 = 0x8E;
  rane_x64_emit_bytes(e, &opcode1, 1);
  rane_x64_emit_bytes(e, &opcode2, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_jg_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode1 = 0x0F;
  uint8_t opcode2 = 0x8F;
  rane_x64_emit_bytes(e, &opcode1, 1);
  rane_x64_emit_bytes(e, &opcode2, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_jge_rel32(rane_x64_emitter_t* e, int32_t rel) {
  uint8_t opcode1 = 0x0F;
  uint8_t opcode2 = 0x8D;
  rane_x64_emit_bytes(e, &opcode1, 1);
  rane_x64_emit_bytes(e, &opcode2, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel, 4);
}

void rane_x64_emit_xor_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  uint8_t rex = rex_w_rm(src_reg, dst_reg);
  uint8_t opcode = 0x31;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(src_reg), reg_code(dst_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_and_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  uint8_t rex = rex_w_rm(src_reg, dst_reg);
  uint8_t opcode = 0x21;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(src_reg), reg_code(dst_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_or_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  uint8_t rex = rex_w_rm(src_reg, dst_reg);
  uint8_t opcode = 0x09;
  uint8_t modrm_val = modrm_byte(0x3, reg_code(src_reg), reg_code(dst_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_shl_reg_cl(rane_x64_emitter_t* e, uint8_t reg) {
  uint8_t rex = rex_w_r(reg);
  uint8_t op = 0xD3;
  uint8_t modrm = modrm_byte(0x3, 0x4, reg_code(reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
}

void rane_x64_emit_shr_reg_cl(rane_x64_emitter_t* e, uint8_t reg) {
  uint8_t rex = rex_w_r(reg);
  uint8_t op = 0xD3;
  uint8_t modrm = modrm_byte(0x3, 0x5, reg_code(reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
}

void rane_x64_emit_sar_reg_cl(rane_x64_emitter_t* e, uint8_t reg) {
  uint8_t rex = rex_w_r(reg);
  uint8_t op = 0xD3;
  uint8_t modrm = modrm_byte(0x3, 0x7, reg_code(reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
}

static void rane_x64_emit_prologue(rane_x64_emitter_t* e, uint32_t frame_bytes) {
  // push rbp; mov rbp, rsp; sub rsp, frame_bytes
  rane_x64_emit_push_reg(e, 5);
  {
    uint8_t rex = 0x48;
    uint8_t op = 0x89;
    uint8_t modrm = 0xE5;
    rane_x64_emit_bytes(e, &rex, 1);
    rane_x64_emit_bytes(e, &op, 1);
    rane_x64_emit_bytes(e, &modrm, 1);
  }
  if (frame_bytes) {
    rane_x64_emit_sub_rsp_imm32(e, frame_bytes);
  }
}

static void rane_x64_emit_epilogue(rane_x64_emitter_t* e) {
  // mov rsp, rbp; pop rbp; ret
  uint8_t rex = 0x48;
  uint8_t op = 0x89;
  uint8_t modrm = 0xEC;
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_pop_reg(e, 5);
  rane_x64_emit_ret(e);
}

// Per-function call prep state
typedef struct rane_call_prep_state_s {
  uint32_t sub_amount;       // bytes subtracted from RSP for outgoing args+shadow+align
  uint32_t stack_arg_bytes;  // requested stack args bytes (not including shadow)
  uint8_t active;
} rane_call_prep_state_t;

static void record_call_fixup(rane_codegen_ctx_t* ctx, const char* sym, uint32_t code_off) {
  if (!ctx || !sym) return;
  if (!ctx->call_fixups || ctx->call_fixup_capacity == 0) return;
  if (ctx->call_fixup_count >= ctx->call_fixup_capacity) return;

  rane_aot_call_fixup_t* fx = (rane_aot_call_fixup_t*)ctx->call_fixups;
  strncpy_s(fx[ctx->call_fixup_count].sym, sizeof(fx[ctx->call_fixup_count].sym), sym, _TRUNCATE);
  fx[ctx->call_fixup_count].code_offset = code_off;
  ctx->call_fixup_count++;
}

static uint32_t win64_call_sub_amount(uint32_t stack_arg_bytes) {
  // Always allocate shadow space.
  uint32_t raw = 32u + stack_arg_bytes;
  // Ensure that after `sub rsp, raw`, RSP is 16-byte aligned.
  // With our prologue `push rbp`, RSP is 16-aligned inside the function.
  // CALL pushes return address, so at the call site we need RSP % 16 == 8.
  // Therefore allocate an extra 8 bytes when raw is 0 mod 16.
  if ((raw & 0xFu) == 0) raw += 8u;
  return raw;
}

static void emit_call_prep(rane_x64_emitter_t* e, rane_call_prep_state_t* st, uint32_t stack_arg_bytes) {
  if (!e || !st) return;
  uint32_t sub = win64_call_sub_amount(stack_arg_bytes);
  st->sub_amount = sub;
  st->stack_arg_bytes = stack_arg_bytes;
  st->active = 1;
  if (sub) rane_x64_emit_sub_rsp_imm32(e, sub);
}

static void emit_call_cleanup(rane_x64_emitter_t* e, rane_call_prep_state_t* st) {
  if (!e || !st || !st->active) return;
  if (st->sub_amount) rane_x64_emit_add_rsp_imm32(e, st->sub_amount);
  st->active = 0;
}

static void rane_x64_emit_tir_inst_impl(rane_x64_emitter_t* e, const rane_tir_inst_t* inst, 
  rane_codegen_ctx_t* ctx, const rane_x64_label_map_entry_t* labels, uint32_t label_count, rane_call_prep_state_t* cps) {
  if (!e || !inst) return;

  switch (inst->opcode) {
    case TIR_LABEL:
      // labels are handled in pass 1; no bytes emitted
      return;

    case TIR_ADDR_OF: {
      // ADDR_OF rD, label => lea rD, [rip+rel32]
      if (inst->operand_count != 2) return;
      if (inst->operands[0].kind != TIR_OPERAND_R) return;
      if (inst->operands[1].kind != TIR_OPERAND_LBL) return;

      uint64_t target = 0;
      for (uint32_t i = 0; i < label_count; i++) {
        if (strcmp(labels[i].label, inst->operands[1].lbl) == 0) { target = labels[i].offset; break; }
      }

      // rel32 is relative to next instruction.
      int32_t rel = (int32_t)(target - (e->offset + 7));
      rane_x64_emit_lea_reg_rip_rel32(e, (uint8_t)inst->operands[0].r, rel);
      return;
    }

    case TIR_MOV: {
      if (inst->operand_count != 2) return;
      if (inst->operands[0].kind != TIR_OPERAND_R) return;
      const uint8_t dst = (uint8_t)inst->operands[0].r;
      if (inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_mov_reg_reg(e, dst, (uint8_t)inst->operands[1].r);
      } else if (inst->operands[1].kind == TIR_OPERAND_IMM) {
        rane_x64_emit_mov_reg_imm64(e, dst, inst->operands[1].imm);
      }
      return;
    }

    case TIR_LD: {
      // LD rD, slot => mov rD, [rbp - (slot+1)*8]
      if (inst->operand_count != 2) return;
      if (inst->operands[0].kind != TIR_OPERAND_R) return;
      if (inst->operands[1].kind != TIR_OPERAND_S) return;
      uint32_t slot = inst->operands[1].s;
      int32_t disp = -(int32_t)((slot + 1u) * 8u);
      rane_x64_emit_mov_reg_mem(e, (uint8_t)inst->operands[0].r, 5 /*RBP*/, disp);
      return;
    }

    case TIR_ST: {
      // ST slot, rS
      if (inst->operand_count != 2) return;
      if (inst->operands[0].kind != TIR_OPERAND_S) return;
      if (inst->operands[1].kind != TIR_OPERAND_R) return;
      uint32_t slot = inst->operands[0].s;
      uint8_t src = (uint8_t)inst->operands[1].r;

      // Special encoding: stack-arg store for CALL_PREP: slot has high bit set
      if ((slot & 0x80000000u) != 0) {
        uint32_t arg_index = slot & 0x7FFFFFFFu;
        // stack args start at [rsp + shadow(32) + arg_index*8]
        int32_t disp = (int32_t)(32 + (int32_t)(arg_index * 8u));
        rane_x64_emit_mov_rsp_disp_reg(e, disp, src);
        return;
      }

      int32_t disp = -(int32_t)((slot + 1u) * 8u);
      rane_x64_emit_mov_mem_reg(e, 5 /*RBP*/, disp, src);
      return;
    }

    case TIR_LDV: {
      // LDV rD, [base+disp] (bootstrap supports only base+disp)
      if (inst->operand_count != 2) return;
      if (inst->operands[0].kind != TIR_OPERAND_R) return;
      if (inst->operands[1].kind != TIR_OPERAND_M) return;
      uint8_t dst = (uint8_t)inst->operands[0].r;
      uint8_t base = (uint8_t)inst->operands[1].m.base_r;
      int32_t disp = inst->operands[1].m.disp;
      rane_x64_emit_mov_reg_mem(e, dst, base, disp);
      return;
    }

    case TIR_ADD:
      if (inst->operand_count == 2 && inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_add_reg_reg(e, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
      }
      return;

    case TIR_SUB:
      if (inst->operand_count == 2 && inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_sub_reg_reg(e, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
      }
      return;

    case TIR_CMP:
      if (inst->operand_count == 2 && inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_R) {
        rane_x64_emit_cmp_reg_reg(e, (uint8_t)inst->operands[0].r, (uint8_t)inst->operands[1].r);
      } else if (inst->operand_count == 2 && inst->operands[0].kind == TIR_OPERAND_R && inst->operands[1].kind == TIR_OPERAND_IMM) {
        rane_x64_emit_cmp_reg_imm(e, (uint8_t)inst->operands[0].r, inst->operands[1].imm);
      }
      return;

    case TIR_JMP: {
      if (inst->operand_count != 1 || inst->operands[0].kind != TIR_OPERAND_LBL) return;
      uint64_t target = 0;
      if (!labels) return;
      for (uint32_t i = 0; i < label_count; i++) {
        if (strcmp(labels[i].label, inst->operands[0].lbl) == 0) { target = labels[i].offset; break; }
      }
      int32_t rel = (int32_t)(target - (e->offset + 5));
      rane_x64_emit_jmp_rel32(e, rel);
      return;
    }

    case TIR_JCC_EXT: {
      if (inst->operand_count != 2 || inst->operands[0].kind != TIR_OPERAND_IMM || inst->operands[1].kind != TIR_OPERAND_LBL) return;
      uint64_t target = 0;
      for (uint32_t i = 0; i < label_count; i++) {
        if (strcmp(labels[i].label, inst->operands[1].lbl) == 0) { target = labels[i].offset; break; }
      }
      rane_tir_cc_t cc = (rane_tir_cc_t)inst->operands[0].imm;
      int32_t rel = 0;
      switch (cc) {
        case TIR_CC_E:  rel = (int32_t)(target - (e->offset + 6)); rane_x64_emit_je_rel32(e, rel); break;
        case TIR_CC_NE: rel = (int32_t)(target - (e->offset + 6)); rane_x64_emit_jne_rel32(e, rel); break;
        case TIR_CC_L:  rel = (int32_t)(target - (e->offset + 6)); rane_x64_emit_jl_rel32(e, rel); break;
        case TIR_CC_LE: rel = (int32_t)(target - (e->offset + 6)); rane_x64_emit_jle_rel32(e, rel); break;
        case TIR_CC_G:  rel = (int32_t)(target - (e->offset + 6)); rane_x64_emit_jg_rel32(e, rel); break;
        case TIR_CC_GE: rel = (int32_t)(target - (e->offset + 6)); rane_x64_emit_jge_rel32(e, rel); break;
        default: break;
      }
      return;
    }

    case TIR_CALL_PREP: {
      if (inst->operand_count < 1 || inst->operands[0].kind != TIR_OPERAND_IMM) return;
      emit_call_prep(e, cps, (uint32_t)inst->operands[0].imm);
      return;
    }

    case TIR_CALL_LOCAL: {
      if (inst->operand_count != 1 || inst->operands[0].kind != TIR_OPERAND_LBL) return;
      uint64_t target = 0;
      for (uint32_t i = 0; i < label_count; i++) {
        if (strcmp(labels[i].label, inst->operands[0].lbl) == 0) { target = labels[i].offset; break; }
      }
      int32_t rel = (int32_t)(target - (e->offset + 5));
      rane_x64_emit_call_rel32(e, rel);
      emit_call_cleanup(e, cps);
      return;
    }

    case TIR_CALL_IMPORT: {
      if (inst->operand_count != 1 || inst->operands[0].kind != TIR_OPERAND_LBL) return;

      // Special-case lowering: `rane_rt_print(s)` becomes `printf("%s", s)`.
      if (strcmp(inst->operands[0].lbl, "rane_rt_print") == 0) {
        // RDX = RCX (s)
        rane_x64_emit_mov_reg_reg(e, 2 /*RDX*/, 1 /*RCX*/);
        // RCX = fmt (patched via string pool scanning)
        // Use heap pointer as a sentinel, then patch to .rdata VA like other strings.
        const char* fmt = "%s";
        rane_x64_emit_mov_reg_imm64(e, 1 /*RCX*/, (uint64_t)(uintptr_t)fmt);

        // call printf via import fixup
        uint32_t off = (uint32_t)e->offset;
        rane_x64_emit_call_placeholder(e);
        record_call_fixup(ctx, "printf", off);
        emit_call_cleanup(e, cps);
        return;
      }

      uint32_t off = (uint32_t)e->offset;
      rane_x64_emit_call_placeholder(e);
      record_call_fixup(ctx, inst->operands[0].lbl, off);
      emit_call_cleanup(e, cps);
      return;
    }

    case TIR_RET:
      // Ensure any active call prep (shouldn't happen) is cleaned up.
      emit_call_cleanup(e, cps);
      rane_x64_emit_epilogue(e);
      return;

    case TIR_RET_VAL:
      // return value is assumed to already be in RAX (r0)
      emit_call_cleanup(e, cps);
      rane_x64_emit_epilogue(e);
      return;

    case TIR_TRAP: {
      // Emit an `int3` breakpoint. (Immediate operand is currently informational.)
      (void)ctx;
      (void)labels;
      (void)label_count;
      (void)cps;
      uint8_t op = 0xCC;
      rane_x64_emit_bytes(e, &op, 1);
      return;
    }

    case TIR_HALT: {
      // Emit `ud2` to terminate with an illegal instruction.
      (void)ctx;
      (void)labels;
      (void)label_count;
      (void)cps;
      uint8_t op[2] = {0x0F, 0x0B};
      rane_x64_emit_bytes(e, op, 2);
      return;
    }

    default:
      return;
  }
}

static uint32_t compute_function_frame_bytes(const rane_tir_function_t* f) {
  // 8 bytes per slot, plus keep 16-byte alignment.
  uint32_t bytes = (f ? f->stack_slot_count : 0u) * 8u;
  return align_up_u32(bytes, 16u);
}

static uint32_t collect_labels(const rane_tir_function_t* f, rane_x64_emitter_t* e, rane_x64_label_map_entry_t* out, uint32_t cap) {
  uint32_t n = 0;
  // Pass 1: simulate code size to compute offsets precisely.
  // For bootstrap we do a conservative approach: emit into a temporary emitter with same semantics.
  rane_x64_emitter_t tmp = *e;
  tmp.offset = 0;

  // prologue placeholder
  uint32_t frame = compute_function_frame_bytes(f);
  // push rbp (1-2), mov rbp,rsp (3), sub rsp,imm32 (7) if frame
  tmp.offset += 1 + 3 + (frame ? 7 : 0);

  rane_call_prep_state_t cps = {};

  for (uint32_t i = 0; i < f->inst_count; i++) {
    const rane_tir_inst_t* in = &f->insts[i];
    if (in->opcode == TIR_LABEL && in->operand_count == 1 && in->operands[0].kind == TIR_OPERAND_LBL) {
      if (n < cap) {
        strncpy_s(out[n].label, sizeof(out[n].label), in->operands[0].lbl, _TRUNCATE);
        out[n].offset = tmp.offset;
        n++;
      }
      continue;
    }

    // Size model (must match emit lengths)
    switch (in->opcode) {
      case TIR_ADDR_OF:
        tmp.offset += 7; // lea r64, [rip+rel32]
        break;

      case TIR_MOV:
        if (in->operand_count == 2 && in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_IMM) tmp.offset += 10;
        else if (in->operand_count == 2 && in->operands[0].kind == TIR_OPERAND_R && in->operands[1].kind == TIR_OPERAND_R) tmp.offset += 3;
        break;
      case TIR_LD:
      case TIR_LDV:
        tmp.offset += 7; // mov r64, [rbp+disp32]
        break;
      case TIR_ST:
        if (in->operand_count == 2 && in->operands[0].kind == TIR_OPERAND_S && (in->operands[0].s & 0x80000000u)) tmp.offset += 8; // mov [rsp+disp32], reg
        else tmp.offset += 7; // mov [rbp+disp32], reg
        break;
      case TIR_ADD:
      case TIR_SUB:
        tmp.offset += 3;
        break;
      case TIR_CMP:
        // We emit CMP in two forms; both are currently 3 bytes in our emitter subset.
        tmp.offset += 3;
        break;
      case TIR_JMP:
        tmp.offset += 5;
        break;
      case TIR_JCC_EXT:
        tmp.offset += 6;
        break;
      case TIR_CALL_PREP: {
        uint32_t stack_arg_bytes = (uint32_t)in->operands[0].imm;
        uint32_t sub = win64_call_sub_amount(stack_arg_bytes);
        cps.sub_amount = sub;
        cps.stack_arg_bytes = stack_arg_bytes;
        cps.active = 1;
        tmp.offset += 7; // sub rsp, imm32
        break;
      }
      case TIR_CALL_LOCAL:
        tmp.offset += 5;
        if (cps.active) { tmp.offset += 7; cps.active = 0; }
        break;
      case TIR_CALL_IMPORT:
        if (strcmp(in->operands[0].lbl, "rane_rt_print") == 0) {
          tmp.offset += 3;  // mov rdx, rcx
          tmp.offset += 10; // mov rcx, imm64
        }
        tmp.offset += 9; // call placeholder
        if (cps.active) { tmp.offset += 7; cps.active = 0; }
        break;
      case TIR_RET:
      case TIR_RET_VAL:
        tmp.offset += 4; // mov rsp, rbp
        tmp.offset += 1 + 1; // pop rbp; ret
        break;
      default:
        break;
    }
  }

  return n;
}

rane_error_t rane_x64_emit_module(rane_x64_emitter_t* e, const rane_tir_module_t* mod) {
  if (!e || !mod || !mod->functions || mod->function_count == 0) return RANE_E_INVALID_ARG;

  rane_x64_label_map_entry_t labels[512];
  uint32_t label_count = 0;

  // For now, emit all functions back-to-back, and label them by their function name.
  // Calls use those labels.

  // Pre-scan with a temp offset model per function.
  uint64_t base_off = 0;
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    const rane_tir_function_t* f = &mod->functions[fi];

    // Function entry label
    if (label_count < 512) {
      strncpy_s(labels[label_count].label, sizeof(labels[label_count].label), f->name, _TRUNCATE);
      labels[label_count].offset = base_off;
      label_count++;
    }

    rane_x64_emitter_t tmp = *e;
    tmp.offset = 0;
    rane_x64_label_map_entry_t local[256];
    uint32_t local_n = collect_labels(f, &tmp, local, 256);
    for (uint32_t i = 0; i < local_n && label_count < 512; i++) {
      strncpy_s(labels[label_count].label, sizeof(labels[label_count].label), local[i].label, _TRUNCATE);
      labels[label_count].offset = base_off + local[i].offset;
      label_count++;
    }

    // Advance base_off by computed size (tmp.offset ends at end of function)
    // collect_labels sets tmp.offset to final.
    // Recompute exact size by reusing collect_labels side effects:
    // tmp.offset already includes prologue+inst sizes.
    // Need the max offset from local offsets isn't enough, so use a second temp walk matching collect_labels sizes.
    // collect_labels already walked through and updated tmp.offset, so use that.
    // Build temp again with collect_labels to get final offset.
    rane_x64_emitter_t tmp2 = *e;
    tmp2.offset = 0;
    rane_x64_label_map_entry_t dummy[1];
    collect_labels(f, &tmp2, dummy, 0);
    base_off += tmp2.offset;
  }

  // Emit actual code
  for (uint32_t fi = 0; fi < mod->function_count; fi++) {
    const rane_tir_function_t* f = &mod->functions[fi];
    uint32_t frame = compute_function_frame_bytes(f);
    rane_x64_emit_prologue(e, frame);

    // Emit body
    rane_call_prep_state_t cps = {};
    for (uint32_t ii = 0; ii < f->inst_count; ii++) {
      rane_x64_emit_tir_inst_impl(e, &f->insts[ii], NULL, labels, label_count, &cps);
    }

    // Ensure function ends in RET even if missing in IR
    if (f->inst_count == 0 || (f->insts[f->inst_count - 1].opcode != TIR_RET && f->insts[f->inst_count - 1].opcode != TIR_RET_VAL)) {
      rane_x64_emit_epilogue(e);
    }
  }

  return RANE_OK;
}

rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* tir_module, rane_codegen_ctx_t* ctx) {
  if (!tir_module || !ctx || !ctx->code_buffer) return RANE_E_INVALID_ARG;

  rane_x64_emitter_t e;
  rane_x64_init_emitter(&e, ctx->code_buffer, ctx->buffer_size);

  // We need fixups during emission, so mirror `rane_x64_emit_module` but pass ctx.
  // Currently `rane_x64_emit_module` doesn't take ctx; do it inline here.

  rane_x64_label_map_entry_t labels[512];
  uint32_t label_count = 0;

  uint64_t base_off = 0;
  for (uint32_t fi = 0; fi < tir_module->function_count; fi++) {
    const rane_tir_function_t* f = &tir_module->functions[fi];

    if (label_count < 512) {
      strncpy_s(labels[label_count].label, sizeof(labels[label_count].label), f->name, _TRUNCATE);
      labels[label_count].offset = base_off;
      label_count++;
    }

    rane_x64_emitter_t tmp = e;
    tmp.offset = 0;

    rane_x64_label_map_entry_t local[256];
    uint32_t local_n = collect_labels(f, &tmp, local, 256);
    for (uint32_t i = 0; i < local_n && label_count < 512; i++) {
      strncpy_s(labels[label_count].label, sizeof(labels[label_count].label), local[i].label, _TRUNCATE);
      labels[label_count].offset = base_off + local[i].offset;
      label_count++;
    }

    // function size
    rane_x64_emitter_t tmp2 = e;
    tmp2.offset = 0;
    rane_x64_label_map_entry_t dummy[1];
    collect_labels(f, &tmp2, dummy, 0);
    base_off += tmp2.offset;
  }

  for (uint32_t fi = 0; fi < tir_module->function_count; fi++) {
    const rane_tir_function_t* f = &tir_module->functions[fi];
    uint32_t frame = compute_function_frame_bytes(f);
    rane_x64_emit_prologue(&e, frame);

    rane_call_prep_state_t cps = {};
    for (uint32_t ii = 0; ii < f->inst_count; ii++) {
      rane_x64_emit_tir_inst_impl(&e, &f->insts[ii], ctx, labels, label_count, &cps);
    }

    if (f->inst_count == 0 || (f->insts[f->inst_count - 1].opcode != TIR_RET && f->insts[f->inst_count - 1].opcode != TIR_RET_VAL)) {
      rane_x64_emit_epilogue(&e);
    }
  }

  ctx->code_size = e.offset;
  return RANE_OK;
}

// Legacy stub (older signature)
rane_error_t rane_x64_codegen_tir_to_machine(const rane_tir_module_t* mod, uint8_t** out_code) { *out_code = NULL; return RANE_OK; }

void rane_x64_emit_lea_reg_rip_rel32(rane_x64_emitter_t* e, uint8_t reg, int32_t rel32) {
  // lea r64, [rip+rel32] => REX.W + 8D /r with ModRM mod=00 rm=101, disp32
  uint8_t rex = rex_w_r(reg);
  uint8_t op = 0x8D;
  uint8_t modrm = modrm_byte(0x0, reg_code(reg), 0x5);
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &op, 1);
  rane_x64_emit_bytes(e, &modrm, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&rel32, 4);
}

