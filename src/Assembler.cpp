#include "Assembler.h"
#include "Encoding.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <iomanip>

static std::string up(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
static std::string trim(std::string s) {
  auto l = s.find_first_not_of(" \t\r\n");
  if (l == std::string::npos) return "";
  auto r = s.find_last_not_of(" \t\r\n");
  return s.substr(l, r - l + 1);
}
static void stripComment(std::string& s) {
  // In this project, immediates use the ARM-style '#' prefix (e.g. #4),
  // so '#' cannot be treated as a comment delimiter.
  // We support ';' and '//' comments.
  auto semi = s.find(';');
  auto slashes = s.find("//");
  size_t p = std::string::npos;
  if (semi != std::string::npos) p = semi;
  if (slashes != std::string::npos) p = (p == std::string::npos) ? slashes : std::min(p, slashes);
  if (p != std::string::npos) s = s.substr(0, p);
}
static std::vector<std::string> splitTokens(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char ch : s) {
    if (ch == ',' || std::isspace(static_cast<unsigned char>(ch))) {
      if (!cur.empty()) { out.push_back(cur); cur.clear(); }
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}
static int parseReg(const std::string& tok) {
  std::string t = up(tok);
  if (t.size() < 2 || t[0] != 'X') throw std::runtime_error("Expected register like X3.");
  int v = std::stoi(t.substr(1));
  if (v < 0 || v > 31) throw std::runtime_error("Register out of range X0..X31.");
  return v;
}
static i64 parseImm(const std::string& tok) {
  std::string t = tok;
  if (!t.empty() && t[0] == '#') t = t.substr(1);
  // allow hex: 0x..
  int base = 10;
  if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) base = 16;
  return std::stoll(t, nullptr, base);
}

// Assemble base formats (matching the CS251 sheet fields)
static u32 encR(u32 op11, int rm, int shamt, int rn, int rd) {
  u32 w = 0;
  w = enc::set(w, 31, 21, op11);
  w = enc::set(w, 20, 16, (u32)rm);
  w = enc::set(w, 15, 10, (u32)shamt);
  w = enc::set(w, 9, 5, (u32)rn);
  w = enc::set(w, 4, 0, (u32)rd);
  return w;
}
static u32 encI(u32 op10, int imm12, int rn, int rd) {
  u32 w = 0;
  w = enc::set(w, 31, 22, op10);
  w = enc::set(w, 21, 10, (u32)imm12);
  w = enc::set(w, 9, 5, (u32)rn);
  w = enc::set(w, 4, 0, (u32)rd);
  return w;
}
static u32 encD(u32 op11, int addr9, int rn, int rt) {
  u32 w = 0;
  w = enc::set(w, 31, 21, op11);
  w = enc::set(w, 20, 12, (u32)addr9);
  w = enc::set(w, 11, 10, 0); // op field unused for sheet's LDUR/STUR
  w = enc::set(w, 9, 5, (u32)rn);
  w = enc::set(w, 4, 0, (u32)rt);
  return w;
}
static u32 encB(u32 op6, i64 imm26) {
  u32 w = 0;
  w = enc::set(w, 31, 26, op6);
  w = enc::set(w, 25, 0, (u32)(imm26 & 0x03FFFFFF));
  return w;
}
static u32 encCB(u32 op8, i64 imm19, int rt) {
  u32 w = 0;
  w = enc::set(w, 31, 24, op8);
  w = enc::set(w, 23, 5, (u32)(imm19 & 0x7FFFF));
  w = enc::set(w, 4, 0, (u32)rt);
  return w;
}

// Custom B.cond: opcode[31:24]=OP_BCOND, cond[23:20], imm19[23:5], rt unused (set 31)
static u32 encBCOND(enc::Cond cond, i64 imm19) {
  u32 w = 0;
  w = enc::set(w, 31, 24, enc::OP_BCOND);
  w = enc::set(w, 23, 20, (u32)cond);
  w = enc::set(w, 23, 5, (u32)(imm19 & 0x7FFFF));
  w = enc::set(w, 4, 0, 31);
  return w;
}

// Custom XEXT: opcode[31:21]=OP_XEXT, funct[15:10] uses shamt field to store function id,
// other fields like R.
static u32 encXEXT(enc::XFunct f, int rm, int rn, int rd, int shamt=0) {
  return encR(enc::OP_XEXT, rm, ((int)f & 0x3F), rn, rd) | ((u32)shamt << 10); // shamt normal ignored for now
}

std::optional<u32> Assembler::assembleLine(const std::string& lineIn) {
  std::string line = lineIn;
  stripComment(line);
  line = trim(line);
  if (line.empty()) return std::nullopt;

  auto toks = splitTokens(line);
  if (toks.empty()) return std::nullopt;

  std::string op = up(toks[0]);

  // ----- pseudo / fixed -----
  if (op == "NOP") return enc::OP_NOP;
  if (op == "HALT") return enc::OP_HALT;

  // ----- conditional branches -----
  if (op == "B.EQ" || op == "B.NE" || op == "B.LT" || op == "B.GE") {
    if (toks.size() != 2) throw std::runtime_error("B.<cond> expects one immediate like #25.");
    i64 imm = parseImm(toks[1]);
    enc::Cond c = enc::Cond::EQ;
    if (op == "B.EQ") c = enc::Cond::EQ;
    else if (op == "B.NE") c = enc::Cond::NE;
    else if (op == "B.LT") c = enc::Cond::LT;
    else if (op == "B.GE") c = enc::Cond::GE;
    return encBCOND(c, imm);
  }

  // ----- base B / BL -----
  if (op == "B" || op == "BL") {
    if (toks.size() != 2) throw std::runtime_error("B/BL expects one immediate like #25.");
    i64 imm = parseImm(toks[1]);
    if (imm < -(1ll<<25) || imm > ((1ll<<25)-1)) throw std::runtime_error("B immediate out of 26-bit range.");
    return encB(op=="B" ? enc::OP_B : enc::OP_BL, imm);
  }

  // ----- CBZ/CBNZ -----
  if (op == "CBZ" || op == "CBNZ") {
    if (toks.size() != 3) throw std::runtime_error("CBZ/CBNZ expects: CBZ Xn, #imm19");
    int rt = parseReg(toks[1]);
    i64 imm = parseImm(toks[2]);
    return encCB(op=="CBZ" ? enc::OP_CBZ : enc::OP_CBNZ, imm, rt);
  }

  // ----- loads/stores -----
  if (op == "LDUR" || op == "STUR") {
    // LDUR Xt, [Xn, #imm]
    if (toks.size() != 4) throw std::runtime_error("LDUR/STUR expects: LDUR Xt, [Xn, #imm]");
    int rt = parseReg(toks[1]);
    // toks[2] should be like [Xn
    std::string t2 = toks[2];
    if (t2.size() < 3 || t2[0] != '[') throw std::runtime_error("Expected [Xn, in LDUR/STUR.");
    t2 = t2.substr(1);
    int rn = parseReg(t2);
    std::string t3 = toks[3];
    if (!t3.empty() && t3.back() == ']') t3.pop_back();
    i64 imm = parseImm(t3);
    if (imm < -(1<<8) || imm > ((1<<8)-1)) throw std::runtime_error("D-format address out of 9-bit signed range.");
    // store in 9-bit field as two's complement
    u32 addr9 = (u32)(imm & 0x1FF);
    return encD(op=="LDUR" ? enc::OP_LDUR : enc::OP_STUR, (int)addr9, rn, rt);
  }

  // ----- ALU R-format -----
  if (op == "ADD" || op == "SUB" || op == "AND" || op == "ORR" || op == "EOR" || op == "MUL") {
    if (toks.size() != 4) throw std::runtime_error(op + " expects: " + op + " Xd, Xn, Xm");
    int rd = parseReg(toks[1]);
    int rn = parseReg(toks[2]);
    int rm = parseReg(toks[3]);

    if (op == "ADD") return encR(enc::OP_ADD, rm, 0, rn, rd);
    if (op == "SUB") return encR(enc::OP_SUB, rm, 0, rn, rd);
    if (op == "AND") return encXEXT(enc::XFunct::AND, rm, rn, rd);
    if (op == "ORR") return encXEXT(enc::XFunct::ORR, rm, rn, rd);
    if (op == "EOR") return encXEXT(enc::XFunct::EOR, rm, rn, rd);
    if (op == "MUL") return encXEXT(enc::XFunct::MUL, rm, rn, rd);
  }

  // ----- shifts -----
  if (op == "LSL" || op == "LSR") {
    if (toks.size() != 4) throw std::runtime_error(op + " expects: " + op + " Xd, Xn, #shamt");
    int rd = parseReg(toks[1]);
    int rn = parseReg(toks[2]);
    i64 sh = parseImm(toks[3]);
    if (sh < 0 || sh > 63) throw std::runtime_error("Shift amount must be 0..63.");
    // encode sh in Rm field as 0 and use rd/rn; function id picks LSL/LSR; we store sh in bits [20:16] (rm) for convenience
    // But to keep decode simple we keep rm field as shamt and read it that way.
    // We'll put sh in Rm, and set shamt field to function id.
    u32 w = 0;
    w = enc::set(w, 31, 21, enc::OP_XEXT);
    w = enc::set(w, 20, 16, (u32)sh);         // using Rm field as immediate shift
    w = enc::set(w, 15, 10, (u32)(op=="LSL" ? (u32)enc::XFunct::LSL : (u32)enc::XFunct::LSR));
    w = enc::set(w, 9, 5, (u32)rn);
    w = enc::set(w, 4, 0, (u32)rd);
    return w;
  }

  // ----- immediate ALU -----
  if (op == "ADDI" || op == "SUBI") {
    if (toks.size() != 4) throw std::runtime_error(op + " expects: " + op + " Xd, Xn, #imm12");
    int rd = parseReg(toks[1]);
    int rn = parseReg(toks[2]);
    i64 imm = parseImm(toks[3]);
    if (imm < 0 || imm > 4095) throw std::runtime_error("I-format imm12 must be 0..4095 in this project.");
    return encI(op=="ADDI" ? enc::OP_ADDI : enc::OP_SUBI, (int)imm, rn, rd);
  }

  // ----- CMP -----
  if (op == "CMP") {
    if (toks.size() != 3) throw std::runtime_error("CMP expects: CMP Xn, Xm");
    int rn = parseReg(toks[1]);
    int rm = parseReg(toks[2]);
    // Rd ignored; set to 31
    return encXEXT(enc::XFunct::CMP, rm, rn, 31);
  }

  // ----- RET -----
  if (op == "RET") {
    // RET Xn   (default X30 if omitted)
    int rn = 30;
    if (toks.size() == 2) rn = parseReg(toks[1]);
    else if (toks.size() != 1) throw std::runtime_error("RET expects: RET or RET Xn");
    return encXEXT(enc::XFunct::RET, 0, rn, 0);
  }

  throw std::runtime_error("Unknown/unsupported instruction: " + op);
}

std::string Assembler::disasm(u32 w, u64 pc) {
  using namespace enc;
  std::ostringstream oss;

  if (w == OP_NOP) return "NOP";
  if (w == OP_HALT) return "HALT";

  u32 op6  = get(w,31,26);
  u32 op8  = get(w,31,24);
  u32 op10 = get(w,31,22);
  u32 op11 = get(w,31,21);

  if (op6 == OP_B || op6 == OP_BL) {
    i64 imm = sext(get(w,25,0), 26);
    oss << (op6==OP_B ? "B" : "BL") << " #" << imm;
    return oss.str();
  }

  if (op8 == OP_CBZ || op8 == OP_CBNZ) {
    i64 imm = sext(get(w,23,5), 19);
    int rt = (int)get(w,4,0);
    oss << (op8==OP_CBZ ? "CBZ" : "CBNZ") << " X" << rt << ", #" << imm;
    return oss.str();
  }

  if (op8 == OP_BCOND) {
    u32 cond = get(w,23,20);
    i64 imm = sext(get(w,23,5), 19);
    const char* c = "EQ";
    if (cond == (u32)Cond::EQ) c = "EQ";
    else if (cond == (u32)Cond::NE) c = "NE";
    else if (cond == (u32)Cond::LT) c = "LT";
    else if (cond == (u32)Cond::GE) c = "GE";
    oss << "B." << c << " #" << imm;
    return oss.str();
  }

  if (op11 == OP_ADD || op11 == OP_SUB || op11 == OP_LDUR || op11 == OP_STUR || op11 == OP_XEXT) {
    int rm = (int)get(w,20,16);
    int shamt = (int)get(w,15,10);
    int rn = (int)get(w,9,5);
    int rd = (int)get(w,4,0);

    if (op11 == OP_ADD) { oss << "ADD X" << rd << ", X" << rn << ", X" << rm; return oss.str(); }
    if (op11 == OP_SUB) { oss << "SUB X" << rd << ", X" << rn << ", X" << rm; return oss.str(); }

    if (op11 == OP_LDUR || op11 == OP_STUR) {
      i64 addr = sext(get(w,20,12), 9);
      int rt = rd;
      oss << (op11==OP_LDUR ? "LDUR" : "STUR") << " X" << rt << ", [X" << rn << ", #" << addr << "]";
      return oss.str();
    }

    if (op11 == OP_XEXT) {
      auto f = (XFunct)shamt;
      if (f == XFunct::CMP) { oss << "CMP X" << rn << ", X" << rm; return oss.str(); }
      if (f == XFunct::AND) { oss << "AND X" << rd << ", X" << rn << ", X" << rm; return oss.str(); }
      if (f == XFunct::ORR) { oss << "ORR X" << rd << ", X" << rn << ", X" << rm; return oss.str(); }
      if (f == XFunct::EOR) { oss << "EOR X" << rd << ", X" << rn << ", X" << rm; return oss.str(); }
      if (f == XFunct::MUL) { oss << "MUL X" << rd << ", X" << rn << ", X" << rm; return oss.str(); }
      if (f == XFunct::LSL) { oss << "LSL X" << rd << ", X" << rn << ", #" << rm; return oss.str(); }
      if (f == XFunct::LSR) { oss << "LSR X" << rd << ", X" << rn << ", #" << rm; return oss.str(); }
      if (f == XFunct::RET) { oss << "RET X" << rn; return oss.str(); }
    }
  }

  if (op10 == OP_ADDI || op10 == OP_SUBI) {
    int imm = (int)get(w,21,10);
    int rn = (int)get(w,9,5);
    int rd = (int)get(w,4,0);
    oss << (op10==OP_ADDI ? "ADDI" : "SUBI") << " X" << rd << ", X" << rn << ", #" << imm;
    return oss.str();
  }

  oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << w;
  return oss.str();
}
