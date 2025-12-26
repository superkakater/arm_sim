#include "UI.h"
#include "Assembler.h"
#include <iomanip>
#include <iostream>
#include <sstream>

static std::string hex32(u32 v) {
  std::ostringstream oss;
  oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << v;
  return oss.str();
}

void UI::printState(const CPU& cpu, const Memory& mem) const {
  using std::cout;
  using std::setw;

  u64 pc = cpu.getPC();
  u32 instr = 0;
  try {
    instr = mem.loadWord(pc);
  } catch (...) {
    instr = 0;
  }

  cout << "\n" << title << "\n";
  cout << "PC = " << pc << ", instruction = " << hex32(instr) << " =\n";
  cout << setw(42) << (u64)instr << "\n\n";

  cout << "Registers" << setw(44) << "Memory\n";
  cout << "-------------" << setw(41) << "---------------------------------------------------------\n";

  // print 32 registers left; memory right shows 32 rows (0..248 step 8)
  // Each row shows two words: M[addr] and M[addr+4].
  // The '>' marker highlights the current PC word (either column).
  for (int i = 0; i < 32; i++) {
    cout << "X" << std::setfill('0') << std::setw(2) << i << std::setfill(' ') << setw(20) << cpu.getX(i);

    // memory row index (reference simulator shows the whole fixed 0..248 window)
    u64 addr = (u64)(i * 8);
    auto printWordAt = [&](u64 a) -> std::string {
      try {
        u32 w = mem.loadWord(a);
        if (memMode == MemMode::HEX) return hex32(w);
        if (memMode == MemMode::CODE) return Assembler::disasm(w, a);
        return std::to_string((u64)w);
      } catch (...) {
        return "?";
      }
    };

    std::string left = printWordAt(addr);
    std::string right = printWordAt(addr + 4);

    const u64 pcNow = cpu.getPC();
    const char leftMark  = (pcNow == addr) ? '>' : ' ';
    const char rightMark = (pcNow == addr + 4) ? '>' : ' ';

    cout << "  " << leftMark << " M[" << std::setw(3) << std::setfill('0') << addr << std::setfill(' ') << "] = " << left;
    cout << setw(10) << " ";
    cout << rightMark << " M[" << std::setw(3) << std::setfill('0') << (addr + 4) << std::setfill(' ') << "]=" << right;
    cout << "\n";
  }

  auto fl = cpu.getFlags();
  cout << "\nFlags: Z=" << (fl.Z ? 1 : 0) << " N=" << (fl.N ? 1 : 0) << "\n";
}

void UI::printHelp() const {
  using std::cout;
  cout << "memory hex, memory dec, memory code\n";
  cout << "PC=#00\n";
  cout << "M[#00]=#\n";
  cout << "R[#]=#, X#=#\n";
  cout << "break [#addr] | break list | break del #addr | break toggle #addr | break clear\n";
  cout << "step [n] (execute n instructions, stops before next breakpoint)\n";
  cout << "continue | cont | c (continue execution; steps once if currently on a breakpoint)\n";
  cout << "save fname[.arm]\n";
  cout << "load fname[.arm]\n";
  cout << "title title\n";
  cout << "clear registers, clear memory, clear\n";
  cout << "ARM instruction (LDUR,STUR,B,CBZ,CBNZ,ADD,SUB,AND,ORR,ADDI,SUBI + extras)\n";
  cout << "run [fast|slow] [nsteps] (default: 20 steps for slow; fast runs until HALT)\n";
}
