#pragma once
#include "Types.h"
#include <stdexcept>
#include <string>
#include <vector>

class Memory {
  std::vector<u32> words; // 4-byte words
public:
  explicit Memory(std::size_t nWords = 256/4); // default 256 bytes
  void clear();
  std::size_t sizeWords() const { return words.size(); }

  // Byte address must be multiple of 4 for word access.
  u32  loadWord(u64 byteAddr) const;
  void storeWord(u64 byteAddr, u32 value);

  // For printing, get raw word at word index.
  u32 getWordIndex(std::size_t i) const;
  void setWordIndex(std::size_t i, u32 v);

  static void requireAligned4(u64 byteAddr);
  static std::size_t addrToIndex(u64 byteAddr);

  void loadProgramHexLines(const std::vector<std::string>& lines);
  std::vector<std::string> dumpProgramHexLines(std::size_t maxWords = 0) const;
};
