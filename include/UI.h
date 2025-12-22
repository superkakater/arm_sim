#pragma once
#include "CPU.h"
#include "Memory.h"
#include <string>

enum class MemMode { HEX, DEC, CODE };

class UI {
  std::string title = "ARM Simulator";
  MemMode memMode = MemMode::DEC;
  u64 memCursor = 0; // byte addr, aligned to 8 for printing convenience
public:
  void setTitle(const std::string& t) { title = t; }
  void setMemMode(MemMode m) { memMode = m; }
  MemMode getMemMode() const { return memMode; }

  void setCursor(u64 byteAddr) { memCursor = byteAddr; }
  u64  getCursor() const { return memCursor; }

  void printState(const CPU& cpu, const Memory& mem) const;
  void printHelp() const;
};
