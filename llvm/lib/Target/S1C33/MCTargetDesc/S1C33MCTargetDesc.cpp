//===-- S1C33MCTargetDesc.cpp - S1C33 Target Descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33MCTargetDesc.h"
#include "S1C33InstPrinter.h"
#include "S1C33MCAsmInfo.h"
#include "TargetInfo/S1C33TargetInfo.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "S1C33GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "S1C33GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "S1C33GenRegisterInfo.inc"

using namespace llvm;

//===----------------------------------------------------------------------===//
// MCInstrAnalysis
//===----------------------------------------------------------------------===//

namespace {
class S1C33MCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit S1C33MCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  bool isBranch(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    // Unconditional PC-relative
    case S1C33::JP_i:
    case S1C33::JP_D_i:
    // Unconditional register-indirect
    case S1C33::JP_r:
    // Conditional PC-relative (signed, unsigned, equal, not-equal)
    case S1C33::JRGT:   case S1C33::JRGE:   case S1C33::JRLT:   case S1C33::JRLE:
    case S1C33::JRUGT:  case S1C33::JRUGE:  case S1C33::JRULT:  case S1C33::JRULE:
    case S1C33::JREQ:   case S1C33::JRNE:
    case S1C33::JRGT_D: case S1C33::JRGE_D: case S1C33::JRLT_D: case S1C33::JRLE_D:
    case S1C33::JRUGT_D:case S1C33::JRUGE_D:case S1C33::JRULT_D:case S1C33::JRULE_D:
    case S1C33::JREQ_D: case S1C33::JRNE_D:
      return true;
    default:
      return false;
    }
  }

  bool isUnconditionalBranch(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    case S1C33::JP_i:
    case S1C33::JP_D_i:
    case S1C33::JP_r:
      return true;
    default:
      return false;
    }
  }

  bool isConditionalBranch(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    case S1C33::JRGT:   case S1C33::JRGE:   case S1C33::JRLT:   case S1C33::JRLE:
    case S1C33::JRUGT:  case S1C33::JRUGE:  case S1C33::JRULT:  case S1C33::JRULE:
    case S1C33::JREQ:   case S1C33::JRNE:
    case S1C33::JRGT_D: case S1C33::JRGE_D: case S1C33::JRLT_D: case S1C33::JRLE_D:
    case S1C33::JRUGT_D:case S1C33::JRUGE_D:case S1C33::JRULT_D:case S1C33::JRULE_D:
    case S1C33::JREQ_D: case S1C33::JRNE_D:
      return true;
    default:
      return false;
    }
  }

  bool isCall(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    case S1C33::CALL_i:
    case S1C33::CALL_D_i:
    case S1C33::CALL_EXT1:
    case S1C33::CALL_D_EXT1:
    case S1C33::CALL_EXT2:
    case S1C33::CALL_D_EXT2:
    case S1C33::CALL_r:
    case S1C33::CALL_r_D:
      return true;
    default:
      return false;
    }
  }

  bool isReturn(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    case S1C33::RET:
    case S1C33::RET_D:
    case S1C33::RETI:
    case S1C33::RETD:
      return true;
    default:
      return false;
    }
  }

  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    // Only handle actual branch/call instructions.
    if (!isBranch(Inst) && !isCall(Inst))
      return false;

    // Only PC-relative instructions carry an immediate operand.
    // Register-indirect branches (JP_r, CALL_r, CALL_r_D) have only a
    // register operand and cannot be statically resolved.
    for (unsigned I = 0; I < Inst.getNumOperands(); ++I) {
      if (Inst.getOperand(I).isImm()) {
        // Operand is a halfword-unit signed displacement (Sign8, or the
        // extended Sign21/Sign34 written back by applyPendingExtPCRel).
        // Byte offset = Imm * 2.  S1C33 has a 28-bit address space.
        int64_t Imm = Inst.getOperand(I).getImm();
        Target = (uint64_t)((int64_t)Addr + Imm * 2) & 0x0FFFFFFF;
        return true;
      }
    }
    return false;
  }
};
} // namespace

static MCInstrAnalysis *createS1C33MCInstrAnalysis(const MCInstrInfo *Info) {
  return new S1C33MCInstrAnalysis(Info);
}

static MCInstrInfo *createS1C33MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitS1C33MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createS1C33MCRegisterInfo(const Triple & /*TT*/) {
  MCRegisterInfo *X = new MCRegisterInfo();
  // SP is the stack pointer; PC is the program counter.
  InitS1C33MCRegisterInfo(X, S1C33::PC, 0, 0, S1C33::PC);
  return X;
}

static MCSubtargetInfo *
createS1C33MCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "s1c33209"; // Default to S1C33209 (P/ECE SoC with hardware multiplier)
  return createS1C33MCSubtargetInfoImpl(TT, CPUName, /*TuneCPU=*/CPUName, FS);
}

static MCStreamer *createMCStreamer(const Triple &T, MCContext &Context,
                                    std::unique_ptr<MCAsmBackend> &&MAB,
                                    std::unique_ptr<MCObjectWriter> &&OW,
                                    std::unique_ptr<MCCodeEmitter> &&Emitter) {
  if (!T.isOSBinFormatELF())
    llvm_unreachable("OS not supported");
  return createELFStreamer(Context, std::move(MAB), std::move(OW),
                           std::move(Emitter));
}

static MCInstPrinter *createS1C33MCInstPrinter(const Triple & /*T*/,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new S1C33InstPrinter(MAI, MII, MRI);
  return nullptr;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeS1C33TargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfo<S1C33MCAsmInfo> X(getTheS1C33Target());

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(getTheS1C33Target(),
                                      createS1C33MCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(getTheS1C33Target(),
                                    createS1C33MCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(getTheS1C33Target(),
                                          createS1C33MCSubtargetInfo);

  // Register the MC code emitter.
  TargetRegistry::RegisterMCCodeEmitter(getTheS1C33Target(),
                                        createS1C33MCCodeEmitter);

  // Register the ASM backend.
  TargetRegistry::RegisterMCAsmBackend(getTheS1C33Target(),
                                       createS1C33AsmBackend);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(getTheS1C33Target(),
                                        createS1C33MCInstPrinter);

  // Register the ELF streamer.
  TargetRegistry::RegisterELFStreamer(getTheS1C33Target(), createMCStreamer);

  // Register the MCInstrAnalysis for branch target resolution (objdump labels).
  TargetRegistry::RegisterMCInstrAnalysis(getTheS1C33Target(),
                                          createS1C33MCInstrAnalysis);
}
