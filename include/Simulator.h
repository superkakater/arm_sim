#pragma once
#include "CPU.h"
#include "Memory.h"
#include "UI.h"
#include <string>

class Simulator {
  CPU cpu;
  Memory mem;
  UI ui;

  bool running = true;

  void cmdMemory(const std::string& arg);
  void cmdPC(const std::string& expr);
  void cmdSetMem(const std::string& expr);
  void cmdSetReg(const std::string& expr);
  void cmdSave(const std::string& fname);
  void cmdLoad(const std::string& fname);
  void cmdTitle(const std::string& rest);
  void cmdClear(const std::string& what);
  void cmdRun(const std::string& rest);
  void cmdAssembleToMemory(const std::string& line);

public:
  Simulator();
  void repl();
};
