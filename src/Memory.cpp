#include "Memory.h"
#include <sstream>
#include <iomanip>

Memory::Memory(std::size_t nWords): words(nWords, 0) {}

void Memory::clear() {
  std::fill(words.begin(), words.end(), 0u);
}

void Memory::requireAligned4(u64 byteAddr) {
  if (byteAddr % 4 != 0) throw std::runtime_error("Unaligned address (must be multiple of 4).");
}
std::size_t Memory::addrToIndex(u64 byteAddr) {
  requireAligned4(byteAddr);
  return static_cast<std::size_t>(byteAddr / 4);
}

u32 Memory::loadWord(u64 byteAddr) const {
  auto i = addrToIndex(byteAddr);
  if (i >= words.size()) throw std::runtime_error("Memory read out of range.");
  return words[i];
}

void Memory::storeWord(u64 byteAddr, u32 value) {
  auto i = addrToIndex(byteAddr);
  if (i >= words.size()) throw std::runtime_error("Memory write out of range.");
  words[i] = value;
}

u32 Memory::getWordIndex(std::size_t i) const {
  if (i >= words.size()) throw std::runtime_error("Memory index out of range.");
  return words[i];
}

void Memory::setWordIndex(std::size_t i, u32 v) {
  if (i >= words.size()) throw std::runtime_error("Memory index out of range.");
  words[i] = v;
}

static bool parseHexWord(const std::string& s, u32& out) {
  std::string t = s;
  // strip comments
  auto pos = t.find_first_of(";#");
  if (pos != std::string::npos) t = t.substr(0, pos);
  // trim
  auto l = t.find_first_not_of(" \t\r\n");
  if (l == std::string::npos) return false;
  auto r = t.find_last_not_of(" \t\r\n");
  t = t.substr(l, r - l + 1);

  if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) t = t.substr(2);
  if (t.empty()) return false;
  for (char c: t) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
  }
  std::stringstream ss;
  ss << std::hex << t;
  unsigned long long v = 0;
  ss >> v;
  out = static_cast<u32>(v & 0xFFFFFFFFull);
  return true;
}

void Memory::loadProgramHexLines(const std::vector<std::string>& lines) {
  clear();
  std::size_t idx = 0;
  for (const auto& line : lines) {
    u32 w = 0;
    if (!parseHexWord(line, w)) continue;
    if (idx >= words.size()) throw std::runtime_error("Program too large for memory.");
    words[idx++] = w;
  }
}

std::vector<std::string> Memory::dumpProgramHexLines(std::size_t maxWords) const {
  std::vector<std::string> out;
  std::size_t n = (maxWords == 0) ? words.size() : std::min(maxWords, words.size());
  for (std::size_t i = 0; i < n; i++) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << words[i];
    out.push_back(oss.str());
  }
  return out;
}
