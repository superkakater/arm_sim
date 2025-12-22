#include "Simulator.h"
#include "Assembler.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

static std::string trim(std::string s) {
  auto l = s.find_first_not_of(" \t\r\n");
  if (l == std::string::npos) return "";
  auto r = s.find_last_not_of(" \t\r\n");
  return s.substr(l, r - l + 1);
}

static bool startsWith(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

static u64 parseHashNum(const std::string& tok) {
  // expects #... ; supports decimal or 0x...
  std::string t = tok;
  if (!t.empty() && t[0] == '#') t = t.substr(1);
  int base = 10;
  if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) base = 16;
  return (u64)std::stoull(t, nullptr, base);
}

Simulator::Simulator(): cpu(), mem(256/4), ui() {
  ui.setCursor(0);
  // Match the reference format: show memory as decoded instructions by default.
  ui.setMemMode(MemMode::CODE);
}

void Simulator::cmdMemory(const std::string& arg) {
  auto a = trim(arg);
  if (a == "hex") ui.setMemMode(MemMode::HEX);
  else if (a == "dec") ui.setMemMode(MemMode::DEC);
  else if (a == "code") ui.setMemMode(MemMode::CODE);
  else throw std::runtime_error("Usage: memory hex|dec|code");
}

void Simulator::cmdPC(const std::string& expr) {
  // PC=#00
  auto pos = expr.find('=');
  if (pos == std::string::npos) throw std::runtime_error("Usage: PC=#00");
  auto rhs = trim(expr.substr(pos + 1));
  u64 v = parseHashNum(rhs);
  cpu.setPC(v);
}

void Simulator::cmdSetMem(const std::string& expr) {
  // M[#00]=#
  auto lbr = expr.find('[');
  auto rbr = expr.find(']');
  auto eq  = expr.find('=');
  if (lbr == std::string::npos || rbr == std::string::npos || eq == std::string::npos) {
    throw std::runtime_error("Usage: M[#addr]=#value");
  }
  auto addrTok = trim(expr.substr(lbr + 1, rbr - lbr - 1));
  auto valTok  = trim(expr.substr(eq + 1));
  u64 addr = parseHashNum(addrTok);
  u64 val  = parseHashNum(valTok);
  mem.storeWord(addr, (u32)(val & 0xFFFFFFFFull));
  // Keep the memory window anchored; the reference view shows the full 0..248 window.
}

void Simulator::cmdSetReg(const std::string& expr) {
  // X#=# or R[#]=#
  auto s = trim(expr);
  if (startsWith(s, "X")) {
    auto pos = s.find('=');
    if (pos == std::string::npos) throw std::runtime_error("Usage: Xn=#value");
    int reg = std::stoi(s.substr(1, pos - 1));
    u64 val = parseHashNum(trim(s.substr(pos + 1)));
    cpu.setX(reg, val);
    return;
  }
  if (startsWith(s, "R[")) {
    auto lbr = s.find('[');
    auto rbr = s.find(']');
    auto eq  = s.find('=');
    if (lbr == std::string::npos || rbr == std::string::npos || eq == std::string::npos) {
      throw std::runtime_error("Usage: R[#]=#");
    }
    int reg = (int)parseHashNum(trim(s.substr(lbr + 1, rbr - lbr - 1)));
    u64 val = parseHashNum(trim(s.substr(eq + 1)));
    cpu.setX(reg, val);
    return;
  }
  throw std::runtime_error("Usage: Xn=#value or R[#]=#");
}

static std::vector<std::string> readAllLines(const std::string& fname) {
  std::ifstream in(fname);
  if (!in) throw std::runtime_error("Cannot open file: " + fname);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) lines.push_back(line);
  return lines;
}

static void writeAllLines(const std::string& fname, const std::vector<std::string>& lines) {
  std::ofstream out(fname);
  if (!out) throw std::runtime_error("Cannot write file: " + fname);
  for (auto& l : lines) out << l << "\n";
}

static std::string ensureArmExt(std::string f) {
  if (f.size() >= 4 && f.substr(f.size()-4) == ".arm") return f;
  return f + ".arm";
}

void Simulator::cmdSave(const std::string& fnameIn) {
  auto f = ensureArmExt(trim(fnameIn));
  // dump full memory as program words
  auto lines = mem.dumpProgramHexLines();
  lines.insert(lines.begin(), "; saved by simulator");
  writeAllLines(f, lines);
  std::cout << "Saved to " << f << "\n";
}

void Simulator::cmdLoad(const std::string& fnameIn) {
  auto f = ensureArmExt(trim(fnameIn));
  auto lines = readAllLines(f);
  mem.loadProgramHexLines(lines);
  cpu.setPC(0);
  ui.setCursor(0);
  std::cout << "Loaded " << f << "\n";
}

void Simulator::cmdTitle(const std::string& rest) {
  ui.setTitle(trim(rest));
}

void Simulator::cmdClear(const std::string& whatIn) {
  auto w = trim(whatIn);
  if (w == "registers") cpu.clearRegisters();
  else if (w == "memory") mem.clear();
  else if (w.empty()) { cpu.reset(); mem.clear(); }
  else throw std::runtime_error("Usage: clear [registers|memory]");
}

void Simulator::cmdRun(const std::string& restIn) {
  std::istringstream iss(restIn);
  std::string mode;
  // steps:
  //   - if specified: execute exactly that many steps (or stop earlier on HALT)
  //   - if omitted:
  //       * run         => fast, run-until-halt
  //       * run fast    => fast, run-until-halt
  //       * run slow    => slow, defaults to 20 interactive steps
  int steps = -1;
  bool runUntilHalt = false;
  iss >> mode;
  if (mode.empty()) {
    mode = "fast";
    runUntilHalt = true;
  }
  else {
    if (mode != "fast" && mode != "slow") {
      // maybe first token was steps
      steps = std::stoi(mode);
      mode = "slow";
    }
  }

  if (steps < 0) {
    if (!(iss >> steps)) {
      if (mode == "fast") runUntilHalt = true;
      else steps = 20;
    }
  }

  const int kMaxUntilHaltSteps = 1'000'000; // safety against infinite loops
  int executed = 0;
  while (true) {
    ui.printState(cpu, mem);
    bool cont = cpu.step(mem);
    executed++;
    if (!cont) { std::cout << "\nHALT\n"; running = false; break; }
    if (mode == "slow") {
      std::cout << "Press ENTER to step...";
      std::string dummy; std::getline(std::cin, dummy);
    }
    if (!runUntilHalt && executed >= steps) break;
    if (runUntilHalt && executed >= kMaxUntilHaltSteps) {
      std::cout << "\nStopped after " << executed << " steps (safety cap).\n";
      break;
    }
  }
}

void Simulator::cmdAssembleToMemory(const std::string& line) {
  // IMPORTANT: typing an instruction in the REPL should *store* it into memory,
  // not execute it immediately. This matches the reference simulator behavior.
  auto w = Assembler::assembleLine(line);
  if (!w) return;
  u64 pc = cpu.getPC();
  mem.storeWord(pc, *w);
  // Auto-advance PC so the user can type the next instruction naturally.
  cpu.setPC(pc + 4);
}

void Simulator::repl() {
  ui.printState(cpu, mem);
  std::string line;
  while (running && std::cout << "\n> " && std::getline(std::cin, line)) {
    line = trim(line);
    if (line.empty()) continue;
    try {
      if (line == "help") { ui.printHelp(); continue; }
      if (line == "quit" || line == "exit") break;

      if (startsWith(line, "memory ")) { cmdMemory(line.substr(7)); ui.printState(cpu, mem); continue; }
      if (startsWith(line, "PC")) { cmdPC(line); ui.printState(cpu, mem); continue; }
      if (startsWith(line, "M[")) { cmdSetMem(line); ui.printState(cpu, mem); continue; }
      if (startsWith(line, "R[") || startsWith(line, "X")) { cmdSetReg(line); ui.printState(cpu, mem); continue; }
      if (startsWith(line, "save ")) { cmdSave(line.substr(5)); continue; }
      if (startsWith(line, "load ")) { cmdLoad(line.substr(5)); ui.printState(cpu, mem); continue; }
      if (startsWith(line, "title ")) { cmdTitle(line.substr(6)); ui.printState(cpu, mem); continue; }
      if (startsWith(line, "clear")) {
        std::string arg = "";
        if (line.size() > 5) arg = line.substr(5);
        cmdClear(arg);
        ui.printState(cpu, mem);
        continue;
      }
      if (startsWith(line, "run")) {
        std::string arg = "";
        if (line.size() > 3) arg = line.substr(3);
        cmdRun(arg);
        continue;
      }

      // Otherwise treat as instruction line
      cmdAssembleToMemory(line);
      ui.printState(cpu, mem);
    } catch (const std::exception& e) {
      std::cout << "Error: " << e.what() << "\n";
    }
  }
}
