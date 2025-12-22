#pragma once
#include "Types.h"
#include "Memory.h"
#include <array>
#include <string>

struct Flags {
  bool Z = false; // zero
  bool N = false; // negative
};

class CPU {
  std::array<u64, 32> X{};
  u64 pc = 0; // byte address
  Flags flags{};
public:
  CPU();

  void reset();
  void clearRegisters();

  u64  getPC() const { return pc; }
  void setPC(u64 newPc) { pc = newPc; }

  u64  getX(int i) const;
  void setX(int i, u64 v);

  const Flags& getFlags() const { return flags; }
  void setFlags(bool Z, bool N) { flags.Z = Z; flags.N = N; }

  // Execute one instruction at current PC. Returns false if HALT encountered.
  bool step(Memory& mem);

  // helpers for ALU ops
  static u64 add64(u64 a, u64 b);
  static u64 sub64(u64 a, u64 b, bool& Z, bool& N);
};
