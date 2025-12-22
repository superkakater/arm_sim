#include "CPU.h"
#include "Encoding.h"
#include "Assembler.h"
#include <stdexcept>

CPU::CPU() { reset(); }

void CPU::reset() {
  clearRegisters();
  pc = 0;
  flags = {};
}

void CPU::clearRegisters() {
  X.fill(0);
}

u64 CPU::getX(int i) const {
  if (i < 0 || i > 31) throw std::runtime_error("Register index out of range.");
  return X[static_cast<std::size_t>(i)];
}

void CPU::setX(int i, u64 v) {
  if (i < 0 || i > 31) throw std::runtime_error("Register index out of range.");
  X[static_cast<std::size_t>(i)] = v;
}

u64 CPU::add64(u64 a, u64 b) { return a + b; }

u64 CPU::sub64(u64 a, u64 b, bool& Z, bool& N) {
  u64 r = a - b;
  Z = (r == 0);
  N = (static_cast<i64>(r) < 0);
  return r;
}

bool CPU::step(Memory& mem) {
  using namespace enc;
  u32 instr = mem.loadWord(pc);

  if (instr == OP_HALT) return false;
  if (instr == OP_NOP) { pc += 4; return true; }

  u32 op6  = get(instr,31,26);
  u32 op8  = get(instr,31,24);
  u32 op10 = get(instr,31,22);
  u32 op11 = get(instr,31,21);

  // B / BL
  if (op6 == OP_B || op6 == OP_BL) {
    i64 imm = sext(get(instr,25,0), 26);
    if (op6 == OP_BL) {
      // link register X30 stores return address (next PC)
      X[30] = pc + 4;
    }
    pc = pc + 4ull * (u64)imm;
    return true;
  }

  // CBZ / CBNZ
  if (op8 == OP_CBZ || op8 == OP_CBNZ) {
    i64 imm = sext(get(instr,23,5), 19);
    int rt = (int)get(instr,4,0);
    u64 v = X[rt];
    bool take = (op8 == OP_CBZ) ? (v == 0) : (v != 0);
    if (take) pc = pc + 4ull * (u64)imm;
    else pc += 4;
    return true;
  }

  // B.cond (custom)
  if (op8 == OP_BCOND) {
    u32 cond = get(instr,23,20);
    i64 imm = sext(get(instr,23,5), 19);
    bool take = false;
    if (cond == (u32)Cond::EQ) take = flags.Z;
    else if (cond == (u32)Cond::NE) take = !flags.Z;
    else if (cond == (u32)Cond::LT) take = flags.N;         // signed less-than after CMP (N==1)
    else if (cond == (u32)Cond::GE) take = !flags.N;         // signed >= after CMP
    if (take) pc = pc + 4ull * (u64)imm;
    else pc += 4;
    return true;
  }

  // I-format ADDI/SUBI
  if (op10 == OP_ADDI || op10 == OP_SUBI) {
    u32 imm12 = get(instr,21,10);
    int rn = (int)get(instr,9,5);
    int rd = (int)get(instr,4,0);

    u64 a = X[rn];
    u64 b = (u64)imm12;
    u64 r = (op10 == OP_ADDI) ? add64(a, b) : (a - b);
    X[rd] = r;
    pc += 4;
    return true;
  }

  // D-format LDUR/STUR
  if (op11 == OP_LDUR || op11 == OP_STUR) {
    i64 addr9 = sext(get(instr,20,12), 9);
    int rn = (int)get(instr,9,5);
    int rt = (int)get(instr,4,0);

    u64 ea = X[rn] + (i64)addr9;  // byte addr (must be aligned to 4 for word)
    if (op11 == OP_LDUR) {
      u32 w = mem.loadWord(ea);
      X[rt] = (u64)w;
    } else {
      mem.storeWord(ea, (u32)(X[rt] & 0xFFFFFFFFull));
    }
    pc += 4;
    return true;
  }

  // R-format ADD/SUB
  if (op11 == OP_ADD || op11 == OP_SUB) {
    int rm = (int)get(instr,20,16);
    int rn = (int)get(instr,9,5);
    int rd = (int)get(instr,4,0);
    u64 a = X[rn];
    u64 b = X[rm];
    if (op11 == OP_ADD) X[rd] = add64(a, b);
    else {
      bool Z=false, N=false;
      X[rd] = sub64(a, b, Z, N);
      // SUB doesn't set flags in sheet; we leave flags unchanged.
    }
    pc += 4;
    return true;
  }

  // Custom XEXT
  if (op11 == OP_XEXT) {
    int rm = (int)get(instr,20,16);
    int funct = (int)get(instr,15,10);
    int rn = (int)get(instr,9,5);
    int rd = (int)get(instr,4,0);
    auto f = (XFunct)funct;

    if (f == XFunct::CMP) {
      bool Z=false, N=false;
      (void)sub64(X[rn], X[rm], Z, N);
      flags.Z = Z;
      flags.N = N;
      pc += 4;
      return true;
    }
    if (f == XFunct::AND) {
      X[rd] = X[rn] & X[rm];
      pc += 4;
      return true;
    }
    if (f == XFunct::ORR) {
      X[rd] = X[rn] | X[rm];
      pc += 4;
      return true;
    }
    if (f == XFunct::EOR) {
      X[rd] = X[rn] ^ X[rm];
      pc += 4;
      return true;
    }
    if (f == XFunct::LSL) {
      u64 sh = (u64)rm;
      X[rd] = X[rn] << (sh & 63ull);
      pc += 4;
      return true;
    }
    if (f == XFunct::LSR) {
      u64 sh = (u64)rm;
      X[rd] = X[rn] >> (sh & 63ull);
      pc += 4;
      return true;
    }
    if (f == XFunct::MUL) {
      X[rd] = X[rn] * X[rm];
      pc += 4;
      return true;
    }
    if (f == XFunct::RET) {
      pc = X[rn];
      return true;
    }
  }

  throw std::runtime_error("Unknown instruction word at PC.");
}
