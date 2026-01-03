#include "rane_x64.h"
#include <string.h>

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
  uint8_t mod = (disp == 0) ? 0x0 : 0x2; // assume disp8 or disp32, for simplicity use disp32
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
  uint8_t modrm_byte = modrm_byte(mod, reg_code(reg), reg_code(base_reg));
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_byte, 1);
  if (mod == 0x2) rane_x64_emit_bytes(e, (uint8_t*)&disp, 4);
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
  uint8_t rex = rex_w_rm(src_reg, dst_reg);
  uint8_t opcode = 0xF7; // IMUL r/m64
  uint8_t modrm_val = modrm_byte(0x3, 0x5, reg_code(dst_reg)); // /5 for IMUL
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_div_reg_reg(rane_x64_emitter_t* e, uint8_t dst_reg, uint8_t src_reg) {
  // DIV is tricky, assumes RAX / src, result in RAX
  // For simplicity, assume dst is RAX
  uint8_t rex = rex_w_r(src_reg);
  uint8_t opcode = 0xF7;
  uint8_t modrm_val = modrm_byte(0x3, 0x6, reg_code(src_reg)); // /6 for DIV
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
}

void rane_x64_emit_cmp_reg_imm(rane_x64_emitter_t* e, uint8_t reg, uint64_t imm) {
  uint8_t rex = rex_w_r(reg);
  uint8_t opcode = 0x81; // CMP r/m64, imm32, but for imm64 need special
  // For simplicity, assume imm fits in 32 bits
  uint8_t modrm_val = modrm_byte(0x3, 0x7, reg_code(reg)); // /7 for CMP
  rane_x64_emit_bytes(e, &rex, 1);
  rane_x64_emit_bytes(e, &opcode, 1);
  rane_x64_emit_bytes(e, &modrm_val, 1);
  rane_x64_emit_bytes(e, (uint8_t*)&imm, 4); // assume 32-bit
}

// Stub for TIR inst emission (needs register mapping, labels, etc.)
void rane_x64_emit_tir_inst(rane_x64_emitter_t* e, const rane_tir_inst_t* inst, uint64_t* label_offsets) {
  // TODO: Map TIR operands to x64 regs, handle labels
  // For now, skip or implement simple cases
}

// Stub for module emission
rane_error_t rane_x64_emit_module(rane_x64_emitter_t* e, const rane_tir_module_t* mod) {
  // TODO: Emit functions, handle labels
  return RANE_OK;
}