//===-- S1C33MCCodeEmitter.cpp - S1C33 code to machine code ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the S1C33MCCodeEmitter class.
// S1C33 instructions are 16-bit fixed-length (+ optional ext prefix words).
//
//===----------------------------------------------------------------------===//

#include "S1C33FixupKinds.h"
#include "S1C33MCExpr.h"
#include "S1C33MCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/Casting.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

namespace {

class S1C33MCCodeEmitter : public MCCodeEmitter {
  const MCInstrInfo &MCII;
  MCContext &Ctx;

public:
  S1C33MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
      : MCII(MCII), Ctx(Ctx) {}
  S1C33MCCodeEmitter(const S1C33MCCodeEmitter &) = delete;
  void operator=(const S1C33MCCodeEmitter &) = delete;
  ~S1C33MCCodeEmitter() override = default;

  void encodeInstruction(const MCInst &Inst, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  // TableGen-generated functions.
  uint64_t getBinaryCodeForInstr(const MCInst &Inst,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;
  unsigned getMachineOpValue(const MCInst &Inst, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;
};

} // end anonymous namespace

// Return the base 16-bit instruction word (with sign8=0) for an EXT1 pseudo,
// or 0 if Opcode is not an EXT1 pseudo.
// Encodings: |000|op1(4)|d(1)|sign8(8)| with sign8=0.
static uint16_t getExt1BranchWord(unsigned Opcode) {
  switch (Opcode) {
  case S1C33::JRGT_EXT1:  return 0x0800;
  case S1C33::JRGE_EXT1:  return 0x0A00;
  case S1C33::JRLT_EXT1:  return 0x0C00;
  case S1C33::JRLE_EXT1:  return 0x0E00;
  case S1C33::JRUGT_EXT1: return 0x1000;
  case S1C33::JRUGE_EXT1: return 0x1200;
  case S1C33::JRULT_EXT1: return 0x1400;
  case S1C33::JRULE_EXT1: return 0x1600;
  case S1C33::JREQ_EXT1:  return 0x1800;
  case S1C33::JRNE_EXT1:  return 0x1A00;
  case S1C33::JP_EXT1:    return 0x1E00;
  case S1C33::JP_D_EXT1:  return 0x1F00;
  case S1C33::CALL_EXT1:  return 0x1C00;
  default:                return 0;
  }
}

void S1C33MCCodeEmitter::encodeInstruction(const MCInst &Inst,
                                            SmallVectorImpl<char> &CB,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  // Global address materialization: LDW_SYM_EXT0/EXT1/EXT2.
  // Operand 0: GR32 destination register. Operand 1: MCExpr symbol.
  // EXT0: ld.w %rd, sym@l  (2 bytes, abs_l fixup at offset 0)
  // EXT1: ext sym@m + ld.w %rd, sym@l  (4 bytes, abs_m@0 + abs_l@2)
  // EXT2: ext sym@h + ext sym@m + ld.w %rd, sym@l  (6 bytes, abs_h@0+abs_m@2+abs_l@4)
  unsigned LdwSymOp = Inst.getOpcode();
  if (LdwSymOp == S1C33::LDW_SYM_EXT0 || LdwSymOp == S1C33::LDW_SYM_EXT1 ||
      LdwSymOp == S1C33::LDW_SYM_EXT2) {
    unsigned RdEnc = Ctx.getRegisterInfo()->getEncodingValue(
        Inst.getOperand(0).getReg());
    const MCExpr *Sym = Inst.getOperand(1).getExpr();
    uint16_t LdwWord = static_cast<uint16_t>(0x6C00 | RdEnc);
    if (LdwSymOp == S1C33::LDW_SYM_EXT2) {
      support::endian::write<uint16_t>(CB, 0xC000, llvm::endianness::little);
      support::endian::write<uint16_t>(CB, 0xC000, llvm::endianness::little);
      support::endian::write<uint16_t>(CB, LdwWord, llvm::endianness::little);
      Fixups.push_back(MCFixup::create(
          0, Sym, (MCFixupKind)S1C33::fixup_s1c33_abs_h, /*PCRel=*/false));
      Fixups.push_back(MCFixup::create(
          2, Sym, (MCFixupKind)S1C33::fixup_s1c33_abs_m, /*PCRel=*/false));
      Fixups.push_back(MCFixup::create(
          4, Sym, (MCFixupKind)S1C33::fixup_s1c33_abs_l, /*PCRel=*/false));
    } else if (LdwSymOp == S1C33::LDW_SYM_EXT1) {
      support::endian::write<uint16_t>(CB, 0xC000, llvm::endianness::little);
      support::endian::write<uint16_t>(CB, LdwWord, llvm::endianness::little);
      Fixups.push_back(MCFixup::create(
          0, Sym, (MCFixupKind)S1C33::fixup_s1c33_abs_m, /*PCRel=*/false));
      Fixups.push_back(MCFixup::create(
          2, Sym, (MCFixupKind)S1C33::fixup_s1c33_abs_l, /*PCRel=*/false));
    } else {
      support::endian::write<uint16_t>(CB, LdwWord, llvm::endianness::little);
      Fixups.push_back(MCFixup::create(
          0, Sym, (MCFixupKind)S1C33::fixup_s1c33_abs_l, /*PCRel=*/false));
    }
    return;
  }

  // 4-byte relaxed forms: ext word + branch/call word with fixup_s1c33_pc_rel_21.
  if (uint16_t BranchWord = getExt1BranchWord(Inst.getOpcode())) {
    // Emit ext word (imm13=0 placeholder) → 0xC000.
    support::endian::write<uint16_t>(CB, 0xC000, llvm::endianness::little);
    // Emit branch word with sign8=0 placeholder.
    support::endian::write<uint16_t>(CB, BranchWord, llvm::endianness::little);
    // Fixup covers both words (offset=0, i.e. from start of ext word).
    // PCRel=true so MCAssembler evaluates as target - ext_word_addr.
    const MCExpr *Expr = Inst.getOperand(0).getExpr();
    Fixups.push_back(MCFixup::create(
        0, Expr, (MCFixupKind)S1C33::fixup_s1c33_pc_rel_21,
        /*PCRel=*/true));
    return;
  }

  uint64_t Binary = getBinaryCodeForInstr(Inst, Fixups, STI);
  // All S1C33 base instructions are 16-bit, little-endian.
  support::endian::write<uint16_t>(CB, Binary, llvm::endianness::little);
}

unsigned S1C33MCCodeEmitter::getMachineOpValue(
    const MCInst &Inst, const MCOperand &MO, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());

  // MCExpr operand: check for @l/@m/@h specifiers (MCSymbolRefExpr with spec)
  // used by the LDW_SYM_EXT* address-materialization sequence, or fall through
  // to PC-relative fixup for branches and calls.
  assert(MO.isExpr() && "Unexpected MCOperand type");
  const MCExpr *Expr = MO.getExpr();

  // Find the MCSymbolRefExpr carrying the @l/@m/@h specifier.
  // It may be the top-level expression (plain "sym@h") or nested inside an
  // MCBinaryExpr when the assembly had "sym+offset@h".  The parser's
  // applySpecifier recurses into binary expressions and attaches the specifier
  // to the MCSymbolRefExpr on the LHS.
  const MCSymbolRefExpr *SRE = dyn_cast<MCSymbolRefExpr>(Expr);
  if (!SRE) {
    if (const auto *BE = dyn_cast<MCBinaryExpr>(Expr))
      SRE = dyn_cast<MCSymbolRefExpr>(BE->getLHS());
  }

  if (SRE) {
    MCFixupKind FK;
    switch (SRE->getSpecifier()) {
    case S1C33::S_ABS_H:
      FK = (MCFixupKind)S1C33::fixup_s1c33_abs_h;
      Fixups.push_back(MCFixup::create(0, Expr, FK, /*PCRel=*/false));
      return 0;
    case S1C33::S_ABS_M:
      FK = (MCFixupKind)S1C33::fixup_s1c33_abs_m;
      Fixups.push_back(MCFixup::create(0, Expr, FK, /*PCRel=*/false));
      return 0;
    case S1C33::S_ABS_L:
      FK = (MCFixupKind)S1C33::fixup_s1c33_abs_l;
      Fixups.push_back(MCFixup::create(0, Expr, FK, /*PCRel=*/false));
      return 0;
    default:
      break; // plain sym ref → PC-relative fixup below
    }
  }

  // Plain symbol ref (no specifier) or other expr: PC-relative fixup.
  // Used by CALL_sym, JP_i, and conditional branch instructions.
  // Fixup byte offset: imm8 occupies bits[7:0] = LE byte 0 of the instruction.
  Fixups.push_back(MCFixup::create(0, Expr,
                                   (MCFixupKind)S1C33::fixup_s1c33_pc_rel_8,
                                   /*PCRel=*/true));
  return 0;
}

// TableGen-generated encoding table.
#include "S1C33GenMCCodeEmitter.inc"

namespace llvm {

MCCodeEmitter *createS1C33MCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx) {
  return new S1C33MCCodeEmitter(MCII, Ctx);
}

} // namespace llvm
