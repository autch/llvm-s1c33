//===-- S1C33InstPrinter.cpp - S1C33 MCInst to assembly -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33InstPrinter.h"
#include "S1C33MCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "S1C33GenAsmWriter.inc"

void S1C33InstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << '%' << getRegisterName(Reg);
}

void S1C33InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                  StringRef Annot, const MCSubtargetInfo &STI,
                                  raw_ostream &O) {
  unsigned Opc = MI->getOpcode();

  // LDW_SYM_EXT* pseudo-instructions: always emit the full 3-instruction EXT2
  // sequence with @h/@m/@l modifiers so the assembly can be correctly
  // re-assembled with absolute relocations (R_S1C33_ABS_H/M/L) instead of the
  // wrong PC-relative one (R_S1C33_REL8).
  //
  // We always use EXT2 even if the MC relaxation selected EXT0 or EXT1, because
  // MCAsmStreamer does not run relaxation before calling the InstPrinter.
  // The EXT2 form is always correct for any 28-bit address, and the redundant
  // zero ext words are harmless.
  if (Opc == S1C33::LDW_SYM_EXT0 || Opc == S1C33::LDW_SYM_EXT1 ||
      Opc == S1C33::LDW_SYM_EXT2) {
    const MCOperand &Rd  = MI->getOperand(0);
    const MCOperand &Sym = MI->getOperand(1);
    assert(Sym.isExpr() && "LDW_SYM_EXT* operand must be an expression");
    const MCExpr *SymExpr = Sym.getExpr();

    auto printModified = [&](StringRef Mod) {
      MAI.printExpr(O, *SymExpr);
      O << '@' << Mod;
    };

    // Always use the full EXT2 (3-instruction) form for correct round-trip.
    O << "\text\t"; printModified("h");
    O << "\n\text\t"; printModified("m");
    O << "\n\tld.w\t"; printRegName(O, Rd.getReg()); O << ", "; printModified("l");
    printAnnotation(O, Annot);
    return;
  }

  if (!printAliasInstr(MI, Address, O))
    printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void S1C33InstPrinter::printOperand(const MCInst *MI, uint64_t Address,
                                     unsigned OpNo, raw_ostream &O) {
  // For PC-relative operands, Address context is passed but we
  // display the raw immediate; Phase 3 will handle symbol resolution.
  printOperand(MI, OpNo, O);
}

void S1C33InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegName(O, Op.getReg());
  } else if (Op.isImm()) {
    O << Op.getImm();
  } else {
    assert(Op.isExpr() && "Expected an expression");
    // S1C33MCExpr (@l/@m/@h) prints as "sym@modifier" via printImpl.
    MAI.printExpr(O, *Op.getExpr());
  }
}
