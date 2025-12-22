#pragma once
#include "Types.h"
#include <optional>
#include <string>
#include <vector>

// Assembles one line of assembly into a 32-bit word.
// Returns nullopt if the line is empty/comment-only.
// Throws runtime_error on parse errors.
class Assembler {
public:
  static std::optional<u32> assembleLine(const std::string& line);
  static std::string disasm(u32 word, u64 pc);
};
