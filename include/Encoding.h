#pragma once
#include "Types.h"

// The course sheet gives bitfield layouts for R/I/D/B/CB formats.
// We implement those for the base opcodes.
//
// Extra instructions are *custom encoded* but still follow the same
// general idea: top bits contain an opcode "tag", the rest encode fields.

namespace enc {

// Bit helpers
inline u32 mask(int bits) { return bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u); }
inline u32 get(u32 w, int hi, int lo) { return (w >> lo) & mask(hi - lo + 1); }
inline u32 set(u32 w, int hi, int lo, u32 v) {
  const u32 m = mask(hi - lo + 1) << lo;
  return (w & ~m) | ((v << lo) & m);
}

// Sign-extend a value with 'bits' width to 64-bit signed
inline i64 sext(u32 x, int bits) {
  const u32 m = 1u << (bits - 1);
  u32 y = x & mask(bits);
  return (y ^ m) - m;
}

// ===== Base sheet opcodes (from CS251 summary) =====
// These are the *opcode fields* shown in the sheet (top bits).
// We match on the full opcode length per format.
//
// B-format: opcode[31:26] = 0b000101
constexpr u32 OP_B   = 0b000101;

// R-format: opcode[31:21] = 0b10001011000 (ADD)
// R-format: opcode[31:21] = 0b11001011000 (SUB)
constexpr u32 OP_ADD = 0b10001011000;
constexpr u32 OP_SUB = 0b11001011000;

// I-format: opcode[31:22] = 0b1001000100 (ADDI)
// I-format: opcode[31:22] = 0b1101000100 (SUBI)
constexpr u32 OP_ADDI = 0b1001000100;
constexpr u32 OP_SUBI = 0b1101000100;

// D-format: opcode[31:21] = 0b11111000010 (LDUR)
// D-format: opcode[31:21] = 0b11111000000 (STUR)
constexpr u32 OP_LDUR = 0b11111000010;
constexpr u32 OP_STUR = 0b11111000000;

// CB-format: opcode[31:24] = 0b10110100 (CBZ)
// CB-format: opcode[31:24] = 0b10110101 (CBNZ)
constexpr u32 OP_CBZ  = 0b10110100;
constexpr u32 OP_CBNZ = 0b10110101;

// ===== Custom extensions =====
//
// We reserve opcode[31:24] in the 0b1011011x range that doesn't collide
// with CBZ/CBNZ. Then encode a 4-bit condition in [23:20] and an imm19
// in [23:5] (like CB). For B.cond we use:
constexpr u32 OP_BCOND = 0b10110110; // custom

enum class Cond : u8 { EQ=0, NE=1, LT=2, GE=3 };

// For CMP, EOR, LSL, LSR, MUL, RET we use a custom "X-format":
// opcode[31:21] = 0b10101010101 (not used in sheet), remaining fields
// mimic R-format: Rm[20:16], shamt[15:10], Rn[9:5], Rd[4:0]
constexpr u32 OP_XEXT = 0b10101010101; // custom R-like base

enum class XFunct : u8 {
  CMP = 0, // Rd ignored; sets flags as if (Rn - Rm)
  AND = 1,
  ORR = 2,
  EOR = 3,
  LSL = 4,
  LSR = 5,
  MUL = 6,
  RET = 7, // RET uses Rn as target register; other fields ignored
};

// BL uses B-format opcode[31:26] = 0b100101 (real ARM64 BL, but not in sheet)
constexpr u32 OP_BL = 0b100101;

// NOP and HALT use a special fixed encoding:
constexpr u32 OP_NOP  = 0xD503201F; // common AArch64 NOP
constexpr u32 OP_HALT = 0xFFFFFFFF; // custom halt word

} // namespace enc
