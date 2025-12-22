#include "Simulator.h"
#include <iostream>

int main() {
  try {
    Simulator sim;
    sim.repl();
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
