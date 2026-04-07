//===-- S1C33AsmPrinter.cpp - S1C33 LLVM Assembly Printer -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33.h"
#include "S1C33TargetMachine.h"
#include "MCTargetDesc/S1C33InstPrinter.h"
#include "TargetInfo/S1C33TargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {

class S1C33AsmPrinter : public AsmPrinter {
public:
  static char ID;

  explicit S1C33AsmPrinter(TargetMachine &TM,
                             std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "S1C33 Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;

  // Print a single MachineOperand to OS in S1C33 asm syntax.
  void printOperand(const MachineInstr *MI, unsigned OpNo, raw_ostream &O);

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
};

} // end anonymous namespace

char S1C33AsmPrinter::ID = 0;

INITIALIZE_PASS(S1C33AsmPrinter, "s1c33-asm-printer",
                "S1C33 Assembly Printer", false, false)

// Lower a MachineOperand to an MCOperand for the asm text streamer.
static MCOperand lowerOperand(const MachineOperand &MO, AsmPrinter &AP) {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    return MCOperand::createReg(MO.getReg());

  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());

  case MachineOperand::MO_MachineBasicBlock: {
    // Branch target: emit a reference to the MBB's label symbol.
    MCSymbol *Sym = MO.getMBB()->getSymbol();
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(Sym, AP.OutContext));
  }

  case MachineOperand::MO_GlobalAddress: {
    const GlobalValue *GV = MO.getGlobal();
    MCSymbol *Sym = AP.getSymbol(GV);
    const MCExpr *Expr = MCSymbolRefExpr::create(Sym, AP.OutContext);
    if (MO.getOffset() != 0)
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), AP.OutContext),
          AP.OutContext);
    return MCOperand::createExpr(Expr);
  }

  case MachineOperand::MO_ExternalSymbol: {
    MCSymbol *Sym = AP.GetExternalSymbolSymbol(MO.getSymbolName());
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(Sym, AP.OutContext));
  }

  case MachineOperand::MO_ConstantPoolIndex: {
    MCSymbol *Sym = AP.GetCPISymbol(MO.getIndex());
    const MCExpr *Expr = MCSymbolRefExpr::create(Sym, AP.OutContext);
    if (MO.getOffset() != 0)
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), AP.OutContext),
          AP.OutContext);
    return MCOperand::createExpr(Expr);
  }

  default:
    // Fallback for unhandled operand types (register masks, etc.).
    return MCOperand::createImm(0);
  }
}

static MCInst lowerToMCInst(const MachineInstr *MI, AsmPrinter &AP) {
  MCInst TmpInst;
  TmpInst.setOpcode(MI->getOpcode());
  unsigned NumExplicit = MI->getDesc().getNumOperands();
  for (unsigned i = 0; i < NumExplicit; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    if (MO.getType() == MachineOperand::MO_RegisterMask)
      continue;
    TmpInst.addOperand(lowerOperand(MO, AP));
  }
  return TmpInst;
}

void S1C33AsmPrinter::printOperand(const MachineInstr *MI, unsigned OpNo,
                                    raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNo);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << '%' << S1C33InstPrinter::getRegisterName(MO.getReg());
    return;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return;
  default:
    llvm_unreachable("not implemented for inline asm");
  }
}

bool S1C33AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode, raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  printOperand(MI, OpNo, O);
  return false;
}

void S1C33AsmPrinter::emitInstruction(const MachineInstr *MI) {
  // Walk the instruction and any InsideBundle successors (delay slot members).
  auto I = MI->getIterator();
  auto E = MI->getParent()->instr_end();
  do {
    MCInst TmpInst = lowerToMCInst(&*I, *this);
    OutStreamer->emitInstruction(TmpInst, getSubtargetInfo());
  } while (++I != E && I->isInsideBundle());
}

// Force static initialization.
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeS1C33AsmPrinter() {
  RegisterAsmPrinter<S1C33AsmPrinter> X(getTheS1C33Target());
}
