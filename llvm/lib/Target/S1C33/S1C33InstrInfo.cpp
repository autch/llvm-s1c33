//===-- S1C33InstrInfo.cpp - S1C33 Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33InstrInfo.h"
#include "S1C33.h"
#include "S1C33Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "S1C33GenInstrInfo.inc"

using namespace llvm;

S1C33InstrInfo::S1C33InstrInfo(const S1C33Subtarget &STI)
    : S1C33GenInstrInfo(STI, RI,
                        /*CFSetupOpcode=*/S1C33::ADJCALLSTACKDOWN,
                        /*CFDestroyOpcode=*/S1C33::ADJCALLSTACKUP) {}

void S1C33InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const DebugLoc &DL, Register DestReg,
                                  Register SrcReg, bool KillSrc,
                                  bool RenamableDest,
                                  bool RenamableSrc) const {
  // SP is a special register — use CLASS 5 transfers instead of CLASS 1 MOV.
  if (SrcReg == S1C33::SP) {
    // ld.w %rd, %sp — CLASS 5 special register read
    BuildMI(MBB, I, DL, get(S1C33::LDW_from_SP), DestReg);
    return;
  }
  if (DestReg == S1C33::SP) {
    // ld.w %sp, %rs — CLASS 5 special register write
    BuildMI(MBB, I, DL, get(S1C33::STW_to_SP))
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  // ld.w %dest, %src — CLASS 1 register-to-register copy (GPR only).
  BuildMI(MBB, I, DL, get(S1C33::MOV_rr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void S1C33InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();
  // Emit ld.w [%sp+imm6], %src.
  // The FrameIndex is later resolved by eliminateFrameIndex to an imm6 offset.
  BuildMI(MBB, MBBI, DL, get(S1C33::STW_sp))
      .addFrameIndex(FrameIndex)
      .addReg(SrcReg, getKillRegState(isKill))
      .setMIFlags(Flags);
}

bool S1C33InstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
  default:
    return false;

  case S1C33::MUL_r: {
    // Expand: mlt.w %rs2, %rs1 + ld.w %rd, %alr
    // S1C33209 hardware multiplier: the pipeline interlocks on ALR/AHR reads,
    // stalling until the result is ready.  gcc33 also reads ALR immediately
    // after mlt.w with no NOPs.  Inserting explicit NOPs is harmful: the gap
    // creates a window where interrupts (e.g. DMA for sound playback) can
    // fire and clobber ALR/AHR, producing garbage multiply results.
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLT_W)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_ALR), Rd);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::MUL16U_r: {
    // Expand: mltu.h %rs2, %rs1 + ld.w %rd, %alr
    // Pipeline interlocks on ALR read; no NOP needed (see MUL_r comment).
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLTU_H)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_ALR), Rd);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::MUL16S_r: {
    // Expand: mlt.h %rs2, %rs1 + ld.w %rd, %alr
    // Pipeline interlocks on ALR read; no NOP needed (see MUL_r comment).
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLT_H)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_ALR), Rd);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::MULHS_r: {
    // Expand: mlt.w %rs2, %rs1 + ld.w %rd, %ahr
    // Pipeline interlocks on AHR read; no NOP needed (see MUL_r comment).
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLT_W)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_AHR), Rd);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::MULHU_r: {
    // Expand: mltu.w %rs2, %rs1 + ld.w %rd, %ahr
    // Pipeline interlocks on AHR read; no NOP needed (see MUL_r comment).
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLTU_W)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_AHR), Rd);
    MI.eraseFromParent();
    return true;
  }

  // MOV_ri32, ALU_ri32, LDx_ri_off, STx_ri_off are expanded in
  // S1C33ExpandExtPseudos (addPreEmitPass) so that ext+target pairs
  // are not split by the post-RA scheduler.
  }
}

void S1C33InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    unsigned SubReg, MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();
  // Emit ld.w %dest, [%sp+imm6].
  BuildMI(MBB, MBBI, DL, get(S1C33::LDW_sp), DestReg)
      .addFrameIndex(FrameIndex)
      .setMIFlags(Flags);
}

// ---------------------------------------------------------------------------
// Branch analysis helpers
// ---------------------------------------------------------------------------

static bool isCondBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case S1C33::JREQ:      case S1C33::JRNE:
  case S1C33::JRLT:      case S1C33::JRLE:
  case S1C33::JRGT:      case S1C33::JRGE:
  case S1C33::JRULT:     case S1C33::JRULE:
  case S1C33::JRUGT:     case S1C33::JRUGE:
  case S1C33::JREQ_EXT1: case S1C33::JRNE_EXT1:
  case S1C33::JRLT_EXT1: case S1C33::JRLE_EXT1:
  case S1C33::JRGT_EXT1: case S1C33::JRGE_EXT1:
  case S1C33::JRULT_EXT1: case S1C33::JRULE_EXT1:
  case S1C33::JRUGT_EXT1: case S1C33::JRUGE_EXT1:
    return true;
  default:
    return false;
  }
}

static bool isUncondBranchOpcode(unsigned Opc) {
  return Opc == S1C33::JP_i || Opc == S1C33::JP_EXT1 ||
         Opc == S1C33::JP_D_i || Opc == S1C33::JP_D_EXT1;
}

// Normalize EXT1 variants to their base opcode for Cond[] storage.
static unsigned normalizeCondBrOpc(unsigned Opc) {
  switch (Opc) {
  case S1C33::JREQ_EXT1:  return S1C33::JREQ;
  case S1C33::JRNE_EXT1:  return S1C33::JRNE;
  case S1C33::JRLT_EXT1:  return S1C33::JRLT;
  case S1C33::JRLE_EXT1:  return S1C33::JRLE;
  case S1C33::JRGT_EXT1:  return S1C33::JRGT;
  case S1C33::JRGE_EXT1:  return S1C33::JRGE;
  case S1C33::JRULT_EXT1: return S1C33::JRULT;
  case S1C33::JRULE_EXT1: return S1C33::JRULE;
  case S1C33::JRUGT_EXT1: return S1C33::JRUGT;
  case S1C33::JRUGE_EXT1: return S1C33::JRUGE;
  default:                 return Opc;
  }
}

// Return true if this is an EXT1 pseudo (4 bytes) rather than a plain
// 2-byte instruction, for BytesRemoved/BytesAdded accounting.
static bool isBranchExt1(unsigned Opc) {
  switch (Opc) {
  case S1C33::JP_EXT1:
  case S1C33::JP_D_EXT1:
  case S1C33::JREQ_EXT1: case S1C33::JRNE_EXT1:
  case S1C33::JRLT_EXT1: case S1C33::JRLE_EXT1:
  case S1C33::JRGT_EXT1: case S1C33::JRGE_EXT1:
  case S1C33::JRULT_EXT1: case S1C33::JRULE_EXT1:
  case S1C33::JRUGT_EXT1: case S1C33::JRUGE_EXT1:
    return true;
  default:
    return false;
  }
}

bool S1C33InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  MachineBasicBlock::iterator I = MBB.end();
  if (I == MBB.begin())
    return false; // Empty block — fallthrough.

  // Find last non-debug instruction.
  do {
    --I;
  } while (I != MBB.begin() && I->isDebugInstr());

  if (!I->isTerminator())
    return false; // No branch — fallthrough.

  // Delayed branches: delay slot is already filled; don't reorganize.
  if (I->hasDelaySlot())
    return true;

  // Case 1: Last terminator is an unconditional branch.
  if (isUncondBranchOpcode(I->getOpcode())) {
    TBB = I->getOperand(0).getMBB();
    if (AllowModify) {
      // Remove dead code after an unconditional branch.
      while (std::next(I) != MBB.end())
        std::next(I)->eraseFromParent();
    }
    // Look for a conditional branch before the unconditional branch.
    if (I == MBB.begin())
      return false;
    MachineBasicBlock::iterator J = I;
    do {
      --J;
    } while (J != MBB.begin() && J->isDebugInstr());
    if (!J->isTerminator())
      return false;
    if (J->hasDelaySlot())
      return true;
    if (!isCondBranchOpcode(J->getOpcode()))
      return true; // Unknown pattern.
    // Pattern: JRcc target / JP fallthrough
    FBB = TBB;
    TBB = J->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(normalizeCondBrOpc(J->getOpcode())));
    return false;
  }

  // Case 2: Last terminator is a conditional branch (fallthrough is implicit).
  if (isCondBranchOpcode(I->getOpcode())) {
    TBB = I->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(normalizeCondBrOpc(I->getOpcode())));
    return false;
  }

  return true; // Unknown terminator.
}

unsigned S1C33InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  MachineBasicBlock::iterator I = MBB.end();
  unsigned Count = 0;
  if (BytesRemoved)
    *BytesRemoved = 0;

  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!I->isTerminator())
      break;
    if (I->hasDelaySlot())
      break;
    if (BytesRemoved)
      *BytesRemoved += isBranchExt1(I->getOpcode()) ? 4 : 2;
    I->eraseFromParent();
    ++Count;
    I = MBB.end();
  }
  return Count;
}

unsigned S1C33InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                       MachineBasicBlock *TBB,
                                       MachineBasicBlock *FBB,
                                       ArrayRef<MachineOperand> Cond,
                                       const DebugLoc &DL,
                                       int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert(Cond.size() <= 1 && "S1C33 branch conditions have at most 1 operand");

  if (BytesAdded)
    *BytesAdded = 0;

  if (Cond.empty()) {
    // Unconditional branch.
    BuildMI(&MBB, DL, get(S1C33::JP_i)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += 2;
    return 1;
  }

  // Conditional branch.
  unsigned Opc = Cond[0].getImm();
  BuildMI(&MBB, DL, get(Opc)).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += 2;

  if (FBB) {
    // Explicit fallthrough target: add an unconditional branch.
    BuildMI(&MBB, DL, get(S1C33::JP_i)).addMBB(FBB);
    if (BytesAdded)
      *BytesAdded += 2;
    return 2;
  }
  return 1;
}

bool S1C33InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Expected exactly one condition operand");
  unsigned Opc = Cond[0].getImm();
  unsigned NewOpc;
  switch (Opc) {
  case S1C33::JREQ:  NewOpc = S1C33::JRNE;  break;
  case S1C33::JRNE:  NewOpc = S1C33::JREQ;  break;
  case S1C33::JRLT:  NewOpc = S1C33::JRGE;  break;
  case S1C33::JRGE:  NewOpc = S1C33::JRLT;  break;
  case S1C33::JRLE:  NewOpc = S1C33::JRGT;  break;
  case S1C33::JRGT:  NewOpc = S1C33::JRLE;  break;
  case S1C33::JRULT: NewOpc = S1C33::JRUGE; break;
  case S1C33::JRUGE: NewOpc = S1C33::JRULT; break;
  case S1C33::JRULE: NewOpc = S1C33::JRUGT; break;
  case S1C33::JRUGT: NewOpc = S1C33::JRULE; break;
  default:
    return true; // Cannot reverse.
  }
  Cond[0].setImm(NewOpc);
  return false;
}
