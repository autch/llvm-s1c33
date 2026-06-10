//===-- S1C33ExpandExtPseudos.cpp - Expand ext-producing pseudos ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass expands pseudo instructions that produce ext prefix sequences
// (MOV_ri32, ALU_ri32, LDx_ri_off, STx_ri_off) into their real instruction
// equivalents.
//
// This expansion runs as a pre-emit pass (after the post-RA scheduler) so
// that ext+target instruction pairs are not split by instruction scheduling.
// The standard expandPostRAPseudo runs BEFORE the post-RA scheduler, which
// would allow the scheduler to reorder ext and its target instruction.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33.h"
#include "S1C33InstrInfo.h"
#include "S1C33Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "s1c33-expand-ext-pseudos"

namespace {

class S1C33ExpandExtPseudos : public MachineFunctionPass {
public:
  static char ID;
  S1C33ExpandExtPseudos() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "S1C33 Expand Ext Pseudos"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                const S1C33InstrInfo &TII);
};

} // namespace

char S1C33ExpandExtPseudos::ID = 0;

/// Emit ext prefix(es) for an immediate value, returning the 6-bit tail.
/// If V fits in 6 bits, emits nothing. If 19 bits, emits 1 ext. If wider, 2.
static int64_t emitExtForImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator InsertPt,
                             const DebugLoc &DL, const S1C33InstrInfo &TII,
                             int64_t V, bool SignedImm6) {
  int64_t imm6 = SignedImm6
                     ? llvm::SignExtend64<6>(static_cast<uint64_t>(V) & 0x3F)
                     : static_cast<int64_t>(V & 0x3F);

  if (isInt<6>(V)) {
    // No ext needed.
  } else if (isInt<19>(V)) {
    int64_t ext_imm13 = (V >> 6) & 0x1FFF;
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm(ext_imm13);
  } else {
    int64_t ext2_imm13 = (V >> 6) & 0x1FFF;
    int64_t ext1_imm13 = (V >> 19) & 0x1FFF;
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm(ext1_imm13);
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm(ext2_imm13);
  }
  return imm6;
}

/// Emit ext prefix(es) for a 3-operand ALU immediate (Class 1 + ext form).
/// The ext provides the ENTIRE immediate (no imm6 tail from target instr).
/// 1 ext: 13-bit unsigned.  2 exts: 26-bit unsigned.
static void emitExtForC1Imm(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator InsertPt,
                            const DebugLoc &DL, const S1C33InstrInfo &TII,
                            uint64_t V) {
  assert(isUInt<26>(V) && "3-operand ALU immediate must fit in 26 bits");
  if (isUInt<13>(V)) {
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm(V);
  } else {
    // 2 exts: first ext has high 13 bits, second has low 13 bits.
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm((V >> 13) & 0x1FFF);
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm(V & 0x1FFF);
  }
}

/// Emit ext prefix(es) for an SP-relative byte displacement that exceeds
/// the scaled imm6 encoding (Class 2 + ext form).  With EXT present the
/// combined displacement is an UNSIGNED raw byte offset: ext13:imm6
/// (19 bits) or ext13:ext13:imm6 (32 bits).  The caller places the low
/// 6 bits in the instruction's imm6 field.
static void emitExtForSpOffset(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator InsertPt,
                               const DebugLoc &DL, const S1C33InstrInfo &TII,
                               int64_t Off) {
  assert(Off >= 64 && "SP-offset pseudo used for an imm6-range offset");
  if (isUInt<19>(Off)) {
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm((Off >> 6) & 0x1FFF);
  } else {
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT))
        .addImm((Off >> 19) & 0x1FFF);
    BuildMI(MBB, InsertPt, DL, TII.get(S1C33::EXT)).addImm((Off >> 6) & 0x1FFF);
  }
}

bool S1C33ExpandExtPseudos::expandMI(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MI,
                                     const S1C33InstrInfo &TII) {
  DebugLoc DL = MI->getDebugLoc();

  switch (MI->getOpcode()) {
  default:
    return false;

  case S1C33::MOV_ri19:
  case S1C33::MOV_ri32: {
    // Both pseudos share the same expansion.  MOV_ri19 only carries values
    // in the sign_ext_19-but-not-sign6 range (exactly one ext prefix);
    // MOV_ri32 carries anything.  emitExtForImm picks the right ext count
    // from the immediate's value so a single path handles both.
    Register Rd = MI->getOperand(0).getReg();
    int64_t V = MI->getOperand(1).getImm();

    int64_t imm6 = emitExtForImm(MBB, MI, DL, TII, V, /*SignedImm6=*/true);
    if (isInt<6>(V))
      BuildMI(MBB, MI, DL, TII.get(S1C33::MOV_ri6), Rd).addImm(V);
    else
      BuildMI(MBB, MI, DL, TII.get(S1C33::MOV_ri6), Rd).addImm(imm6);
    MI->eraseFromParent();
    return true;
  }

  case S1C33::AND_ri32:
  case S1C33::OR_ri32:
  case S1C33::XOR_ri32:
  case S1C33::SUB_ri32: {
    unsigned Opc = MI->getOpcode();
    Register Rd = MI->getOperand(0).getReg();
    int64_t V = MI->getOperand(2).getImm();

    unsigned RealOpc;
    switch (Opc) {
    case S1C33::AND_ri32:
      RealOpc = S1C33::AND_ri;
      break;
    case S1C33::OR_ri32:
      RealOpc = S1C33::OR_ri;
      break;
    case S1C33::XOR_ri32:
      RealOpc = S1C33::XOR_ri;
      break;
    default:
      RealOpc = S1C33::SUB_ri;
      break;
    }

    bool Signed = (RealOpc != S1C33::SUB_ri);
    int64_t imm6 = emitExtForImm(MBB, MI, DL, TII, V, Signed);
    if (isInt<6>(V))
      BuildMI(MBB, MI, DL, TII.get(RealOpc), Rd).addReg(Rd).addImm(V);
    else
      BuildMI(MBB, MI, DL, TII.get(RealOpc), Rd).addReg(Rd).addImm(imm6);
    MI->eraseFromParent();
    return true;
  }

  case S1C33::CMP_ri32: {
    Register Rd = MI->getOperand(0).getReg();
    int64_t V = MI->getOperand(1).getImm();
    int64_t imm6 = emitExtForImm(MBB, MI, DL, TII, V, /*SignedImm6=*/true);
    if (isInt<6>(V))
      BuildMI(MBB, MI, DL, TII.get(S1C33::CMP_ri)).addReg(Rd).addImm(V);
    else
      BuildMI(MBB, MI, DL, TII.get(S1C33::CMP_ri)).addReg(Rd).addImm(imm6);
    MI->eraseFromParent();
    return true;
  }

  // 3-operand ALU: ext imm / op %rd, %rs → rd = rs <op> zero_ext(imm)
  case S1C33::ADD_rri:
  case S1C33::SUB_rri:
  case S1C33::AND_rri:
  case S1C33::OR_rri:
  case S1C33::XOR_rri: {
    Register Rd = MI->getOperand(0).getReg();
    Register Rs = MI->getOperand(1).getReg();
    uint64_t V = MI->getOperand(2).getImm();

    unsigned RealOpc;
    switch (MI->getOpcode()) {
    case S1C33::ADD_rri:
      RealOpc = S1C33::ADD_rr_ext;
      break;
    case S1C33::SUB_rri:
      RealOpc = S1C33::SUB_rr_ext;
      break;
    case S1C33::AND_rri:
      RealOpc = S1C33::AND_rr_ext;
      break;
    case S1C33::OR_rri:
      RealOpc = S1C33::OR_rr_ext;
      break;
    default:
      RealOpc = S1C33::XOR_rr_ext;
      break;
    }

    emitExtForC1Imm(MBB, MI, DL, TII, V);
    BuildMI(MBB, MI, DL, TII.get(RealOpc), Rd).addReg(Rs);
    MI->eraseFromParent();
    return true;
  }

  case S1C33::LDB_ri_off:
  case S1C33::LDUB_ri_off:
  case S1C33::LDH_ri_off:
  case S1C33::LDUH_ri_off:
  case S1C33::LDW_ri_off: {
    Register Rd = MI->getOperand(0).getReg();
    Register Rb = MI->getOperand(1).getReg();
    int64_t Off = MI->getOperand(2).getImm();

    unsigned RealOpc;
    switch (MI->getOpcode()) {
    case S1C33::LDB_ri_off:
      RealOpc = S1C33::LDB_ri;
      break;
    case S1C33::LDUB_ri_off:
      RealOpc = S1C33::LDUB_ri;
      break;
    case S1C33::LDH_ri_off:
      RealOpc = S1C33::LDH_ri;
      break;
    case S1C33::LDUH_ri_off:
      RealOpc = S1C33::LDUH_ri;
      break;
    default:
      RealOpc = S1C33::LDW_ri;
      break;
    }
    if (Off != 0) {
      int64_t ext_imm13 = Off & 0x1FFF;
      BuildMI(MBB, MI, DL, TII.get(S1C33::EXT)).addImm(ext_imm13);
    }
    BuildMI(MBB, MI, DL, TII.get(RealOpc), Rd).addReg(Rb);
    MI->eraseFromParent();
    return true;
  }

  case S1C33::STB_ri_off:
  case S1C33::STH_ri_off:
  case S1C33::STW_ri_off: {
    Register Rb = MI->getOperand(0).getReg();
    int64_t Off = MI->getOperand(1).getImm();
    Register Rs = MI->getOperand(2).getReg();

    unsigned RealOpc;
    switch (MI->getOpcode()) {
    case S1C33::STB_ri_off:
      RealOpc = S1C33::STB_ri;
      break;
    case S1C33::STH_ri_off:
      RealOpc = S1C33::STH_ri;
      break;
    default:
      RealOpc = S1C33::STW_ri;
      break;
    }
    if (Off != 0) {
      int64_t ext_imm13 = Off & 0x1FFF;
      BuildMI(MBB, MI, DL, TII.get(S1C33::EXT)).addImm(ext_imm13);
    }
    BuildMI(MBB, MI, DL, TII.get(RealOpc)).addReg(Rb).addReg(Rs);
    MI->eraseFromParent();
    return true;
  }

  case S1C33::LDB_sp_off:
  case S1C33::LDUB_sp_off:
  case S1C33::LDH_sp_off:
  case S1C33::LDUH_sp_off:
  case S1C33::LDW_sp_off: {
    Register Rd = MI->getOperand(0).getReg();
    int64_t Off = MI->getOperand(1).getImm();

    unsigned RealOpc;
    switch (MI->getOpcode()) {
    case S1C33::LDB_sp_off:
      RealOpc = S1C33::LDB_sp;
      break;
    case S1C33::LDUB_sp_off:
      RealOpc = S1C33::LDUB_sp;
      break;
    case S1C33::LDH_sp_off:
      RealOpc = S1C33::LDH_sp;
      break;
    case S1C33::LDUH_sp_off:
      RealOpc = S1C33::LDUH_sp;
      break;
    default:
      RealOpc = S1C33::LDW_sp;
      break;
    }
    emitExtForSpOffset(MBB, MI, DL, TII, Off);
    // With EXT present the imm6 field holds the low 6 bits of the raw byte
    // displacement (not scaled units).
    BuildMI(MBB, MI, DL, TII.get(RealOpc), Rd).addImm(Off & 0x3F);
    MI->eraseFromParent();
    return true;
  }

  case S1C33::STB_sp_off:
  case S1C33::STH_sp_off:
  case S1C33::STW_sp_off: {
    int64_t Off = MI->getOperand(0).getImm();

    unsigned RealOpc;
    switch (MI->getOpcode()) {
    case S1C33::STB_sp_off:
      RealOpc = S1C33::STB_sp;
      break;
    case S1C33::STH_sp_off:
      RealOpc = S1C33::STH_sp;
      break;
    default:
      RealOpc = S1C33::STW_sp;
      break;
    }
    emitExtForSpOffset(MBB, MI, DL, TII, Off);
    BuildMI(MBB, MI, DL, TII.get(RealOpc))
        .addImm(Off & 0x3F)
        .add(MI->getOperand(1)); // $rs — preserves kill/undef flags
    MI->eraseFromParent();
    return true;
  }

  case S1C33::BTST_ri_off:
  case S1C33::BCLR_ri_off:
  case S1C33::BSET_ri_off:
  case S1C33::BNOT_ri_off: {
    Register Rb = MI->getOperand(0).getReg();
    int64_t Off = MI->getOperand(1).getImm();
    int64_t Imm3 = MI->getOperand(2).getImm();

    unsigned RealOpc;
    switch (MI->getOpcode()) {
    case S1C33::BTST_ri_off:
      RealOpc = S1C33::BTST;
      break;
    case S1C33::BCLR_ri_off:
      RealOpc = S1C33::BCLR;
      break;
    case S1C33::BSET_ri_off:
      RealOpc = S1C33::BSET;
      break;
    default:
      RealOpc = S1C33::BNOT;
      break;
    }
    if (Off != 0) {
      assert(isUInt<13>(Off) && "Bit-op ext displacement must fit in 13 bits");
      BuildMI(MBB, MI, DL, TII.get(S1C33::EXT)).addImm(Off & 0x1FFF);
    }
    BuildMI(MBB, MI, DL, TII.get(RealOpc)).addReg(Rb).addImm(Imm3);
    MI->eraseFromParent();
    return true;
  }
  }
}

bool S1C33ExpandExtPseudos::runOnMachineFunction(MachineFunction &MF) {
  const S1C33Subtarget &ST = MF.getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII = *ST.getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto MI = MBB.begin(), E = MBB.end(); MI != E;) {
      auto Next = std::next(MI);
      Changed |= expandMI(MBB, MI, TII);
      MI = Next;
    }
  }
  return Changed;
}

FunctionPass *llvm::createS1C33ExpandExtPseudosPass() {
  return new S1C33ExpandExtPseudos();
}
