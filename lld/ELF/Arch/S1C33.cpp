//===- S1C33.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33 (Seiko Epson S1C33 Family) ELF linker support.
//
// Relocation types (project-local definitions, matching S1C33ELFObjectWriter):
//
//   R_S1C33_NONE  = 0   no relocation
//   R_S1C33_REL8  = 1   8-bit PC-rel: sign8 = (S-P-2)/2, Data[0]
//                        target = P + 2 + 2*sign8
//   R_S1C33_32    = 2   32-bit absolute: write32le(loc, S)
//   R_S1C33_ABS_H = 3   bits[31:19] of S → ext_h imm13
//   R_S1C33_ABS_M = 4   bits[18:6]  of S → ext_m imm13
//   R_S1C33_ABS_L = 5   bits[5:0]   of S → ld.w imm6 (bits[9:4] of instruction)
//   R_S1C33_REL21 = 6   21-bit PC-rel combined (1×ext + call/branch)
//   R_S1C33_REL_H = 7   bits[31:22] of byte_offset → ext_h imm13 (SRF split)
//   R_S1C33_REL_M = 8   bits[21:9]  of byte_offset → ext_m imm13 (SRF split)
//   R_S1C33_REL_L = 9   bits[8:1]   of byte_offset → call/branch sign8 (SRF split)
//                        where byte_offset = target - call_addr (same as REL21)
//
// For SRF split PC-rel (REL_H/M/L), three relocations patch three consecutive
// 16-bit instructions at offsets 0/+2/+4 from ext_h.  All three encode the
// same byte_offset = target - call_addr where call_addr = P_H + 4.
// Since P = P_H / P_H+2 / P_H+4 for REL_H/M/L respectively:
//   byte_offset = (S-P_H)-4 = (S-P_M)-2 = (S-P_L)
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

class S1C33 final : public TargetInfo {
public:
  S1C33(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  int64_t getImplicitAddend(const uint8_t *buf, RelType type) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};

} // namespace

// Encode a 13-bit immediate into an ext instruction word (little-endian).
// ext encoding: Data[0]=imm13[7:0], Data[1]=0xC0|imm13[12:8]
static void writeExt13(uint8_t *loc, uint32_t imm13) {
  imm13 &= 0x1FFF;
  loc[0] = static_cast<uint8_t>(imm13 & 0xFF);
  loc[1] = static_cast<uint8_t>(0xC0 | ((imm13 >> 8) & 0x1F));
}

S1C33::S1C33(Ctx &ctx) : TargetInfo(ctx) {
  // nop (0x0060) × 2
  trapInstr = {0x60, 0x00, 0x60, 0x00};
}

RelExpr S1C33::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  switch (type) {
  case R_S1C33_NONE:
    return R_NONE;
  case R_S1C33_REL8:
  case R_S1C33_REL21:
  case R_S1C33_REL_H:
  case R_S1C33_REL_M:
  case R_S1C33_REL_L:
    return R_PC;
  default:
    return R_ABS;
  }
}

// Read the implicit addend already encoded in the instruction field.
// SRF-generated objects zero all instruction fields before linking, so
// this typically returns 0.  The implementations mirror the extraction
// inverse of relocate().
int64_t S1C33::getImplicitAddend(const uint8_t *buf, RelType type) const {
  switch (type) {
  case R_S1C33_NONE:
    return 0;
  case R_S1C33_REL8: {
    // sign8 field → addend = 2 + 2 * sign8
    int8_t sign8 = static_cast<int8_t>(buf[0]);
    return 2 + 2 * static_cast<int64_t>(sign8);
  }
  case R_S1C33_32:
    return SignExtend64<32>(read32le(buf));
  case R_S1C33_ABS_H: {
    uint32_t imm13 = buf[0] | ((buf[1] & 0x1F) << 8);
    return static_cast<int64_t>(imm13) << 19;
  }
  case R_S1C33_ABS_M: {
    uint32_t imm13 = buf[0] | ((buf[1] & 0x1F) << 8);
    return static_cast<int64_t>(imm13) << 6;
  }
  case R_S1C33_ABS_L: {
    uint8_t imm6 = ((buf[0] >> 4) & 0x0F) | ((buf[1] & 0x03) << 4);
    return static_cast<int64_t>(imm6);
  }
  case R_S1C33_REL21: {
    // Reconstruct A such that relocate(S + A - P) reproduces the encoded
    // extended_imm.  With formula extended_imm = (val - 2) / 2 and
    // val = S + A - P: A = extended_imm * 2 + 2 - (S - P).
    // For the zero-initialised placeholder (extImm = 0) A = 0, which is the
    // common case for LLVM-generated objects.
    uint32_t imm13 = buf[0] | ((buf[1] & 0x1F) << 8);
    uint8_t  sign8 = buf[2];
    uint32_t raw21 = (imm13 << 8) | sign8;
    int64_t extImm = SignExtend64<21>(raw21);
    return extImm * 2;
  }
  case R_S1C33_REL_H: {
    uint32_t imm13 = buf[0] | ((buf[1] & 0x1F) << 8);
    return SignExtend64<13>(imm13) << 22;
  }
  case R_S1C33_REL_M: {
    uint32_t imm13 = buf[0] | ((buf[1] & 0x1F) << 8);
    return SignExtend64<13>(imm13) << 9;
  }
  case R_S1C33_REL_L: {
    return static_cast<int8_t>(buf[0]) * 2;
  }
  default:
    return 0;
  }
}

void S1C33::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  // For PC-relative relocs lld passes val = S + A - P (signed).
  // For absolute relocs val = S + A.
  switch (rel.type) {
  case R_S1C33_NONE:
    break;

  case R_S1C33_REL8: {
    // sign8 = (val - 2) / 2  (val = target - P, branch target = P+2+2*sign8)
    int64_t sv = static_cast<int64_t>(val) - 2;
    if (sv & 1)
      Err(ctx) << getErrorLoc(ctx, loc) << "R_S1C33_REL8: misaligned target";
    int64_t sign8 = sv / 2;
    checkInt(ctx, loc, sign8, 8, rel);
    loc[0] = static_cast<uint8_t>(sign8);
    break;
  }

  case R_S1C33_32:
    write32le(loc, static_cast<uint32_t>(val));
    break;

  case R_S1C33_ABS_H: {
    // bits[31:19] of absolute address → ext_h imm13
    uint32_t imm13 = (static_cast<uint32_t>(val) >> 19) & 0x1FFF;
    writeExt13(loc, imm13);
    break;
  }

  case R_S1C33_ABS_M: {
    // bits[18:6] of absolute address → ext_m imm13
    uint32_t imm13 = (static_cast<uint32_t>(val) >> 6) & 0x1FFF;
    writeExt13(loc, imm13);
    break;
  }

  case R_S1C33_ABS_L: {
    // bits[5:0] of absolute address → ld.w imm6 field.
    // The imm6 occupies bits[9:4] of the 16-bit instruction word (LE):
    //   Data[0] bits[7:4] = imm6[3:0], Data[1] bits[1:0] = imm6[5:4].
    uint8_t imm6 = static_cast<uint8_t>(val & 0x3F);
    loc[0] |= static_cast<uint8_t>((imm6 & 0x0F) << 4);
    loc[1] |= static_cast<uint8_t>((imm6 >> 4) & 0x03);
    break;
  }

  case R_S1C33_REL21: {
    // Single-ext + branch/call 4-byte sequence at loc (= ext_addr).
    // S1C33 branch formula (with ext): target = call_addr + 2 * extended_imm
    //   call_addr = loc + 2, val = S + A - P (R_PC, P = loc = ext_addr)
    //   extended_imm = (val - 2) / 2
    int64_t sv = static_cast<int64_t>(val) - 2;
    if (sv & 1)
      Err(ctx) << getErrorLoc(ctx, loc) << "R_S1C33_REL21: misaligned target";
    int64_t extImm = sv / 2;
    checkInt(ctx, loc, extImm, 21, rel);
    uint32_t raw21 = static_cast<uint32_t>(extImm & 0x1FFFFF);
    uint32_t imm13 = (raw21 >> 8) & 0x1FFF;
    uint8_t  sign8 = static_cast<uint8_t>(raw21 & 0xFF);
    writeExt13(loc, imm13);     // patch ext word at loc[0..1]
    loc[2] = sign8;             // patch branch sign8 at loc[2] (byte 0 of branch word)
    break;
  }

  case R_S1C33_REL_H: {
    // Three-instruction split PC-rel, ext_h instruction at loc.
    // byte_offset = target - call_addr = val - 4  (call_addr = P_H+4, val = S-P_H)
    int64_t byteOff = static_cast<int64_t>(val) - 4;
    // bits[31:22] of byte_offset → ext_h imm13
    uint32_t imm13 = static_cast<uint32_t>((byteOff >> 22) & 0x1FFF);
    writeExt13(loc, imm13);
    break;
  }

  case R_S1C33_REL_M: {
    // ext_m instruction at loc (+2 from ext_h).
    // byte_offset = val - 2  (val = S-P_M; P_M = P_H+2; call_addr = P_H+4)
    int64_t byteOff = static_cast<int64_t>(val) - 2;
    // bits[21:9] of byte_offset → ext_m imm13
    uint32_t imm13 = static_cast<uint32_t>((byteOff >> 9) & 0x1FFF);
    writeExt13(loc, imm13);
    break;
  }

  case R_S1C33_REL_L: {
    // call/branch instruction at loc (+4 from ext_h = call_addr).
    // byte_offset = val  (val = S-P_L; P_L = call_addr = P_H+4)
    int64_t byteOff = static_cast<int64_t>(val);
    // bits[8:1] of byte_offset = bits[7:0] of word_offset → sign8 field (Data[0])
    loc[0] = static_cast<uint8_t>((byteOff >> 1) & 0xFF);
    break;
  }

  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized S1C33 relocation "
             << rel.type;
  }
}

void elf::setS1C33TargetInfo(Ctx &ctx) { ctx.target.reset(new S1C33(ctx)); }
