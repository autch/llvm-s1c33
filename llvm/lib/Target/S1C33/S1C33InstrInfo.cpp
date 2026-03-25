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
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLT_W)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_ALR), Rd);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::MULHS_r: {
    // Expand: mlt.w %rs2, %rs1 + ld.w %rd, %ahr
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
    Register Rd  = MI.getOperand(0).getReg();
    Register Rs1 = MI.getOperand(1).getReg();
    Register Rs2 = MI.getOperand(2).getReg();
    BuildMI(MBB, MI, DL, get(S1C33::MLTU_W)).addReg(Rs2).addReg(Rs1);
    BuildMI(MBB, MI, DL, get(S1C33::LDW_from_AHR), Rd);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::MOV_ri32: {
    Register Rd = MI.getOperand(0).getReg();
    int64_t V = MI.getOperand(1).getImm();

    // The ld.w imm6 field holds only bits[5:0] of V, sign-extended to 6 bits.
    // The preceding ext(s) supply the upper bits so that the hardware
    // reconstructs: sign_extend({ext_upper, imm6[5:0]}) == V at run time.
    // Pass the sign-extended 6-bit value so the AsmPrinter emits a value
    // in the simm6 range and the assembler can round-trip the output.
    int64_t imm6 = llvm::SignExtend64<6>(static_cast<uint64_t>(V) & 0x3F);
    if (isInt<6>(V)) {
      // No ext needed: value fits directly in 6-bit signed field.
      BuildMI(MBB, MI, DL, get(S1C33::MOV_ri6), Rd).addImm(V);
    } else if (isInt<19>(V)) {
      // One ext: hardware sees sign_extend_19({ext_imm13, imm6[5:0]}) = V.
      int64_t ext_imm13 = (V >> 6) & 0x1FFF;
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext_imm13);
      BuildMI(MBB, MI, DL, get(S1C33::MOV_ri6), Rd).addImm(imm6);
    } else {
      // Two exts: hardware sees sign_extend_32({ext1, ext2, imm6[5:0]}) = V.
      int64_t ext2_imm13 = (V >> 6)  & 0x1FFF;
      int64_t ext1_imm13 = (V >> 19) & 0x1FFF;
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext1_imm13);
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext2_imm13);
      BuildMI(MBB, MI, DL, get(S1C33::MOV_ri6), Rd).addImm(imm6);
    }
    MI.eraseFromParent();
    return true;
  }

  // ALU_ri32 pseudos: 2-address (op0=rd tied to op1=rs), op2=imm.
  // and/or/xor use sign-extended immediate (simm6); sub uses unsigned (uimm6).
  // Same ext encoding formula as MOV_ri32, but emitting the ALU instruction.
  case S1C33::AND_ri32:
  case S1C33::OR_ri32:
  case S1C33::XOR_ri32:
  case S1C33::SUB_ri32: {
    unsigned Opc = MI.getOpcode();
    Register Rd = MI.getOperand(0).getReg();
    int64_t V   = MI.getOperand(2).getImm();

    // Map pseudo → real ALU_ri instruction.
    unsigned RealOpc;
    switch (Opc) {
    case S1C33::AND_ri32: RealOpc = S1C33::AND_ri; break;
    case S1C33::OR_ri32:  RealOpc = S1C33::OR_ri;  break;
    case S1C33::XOR_ri32: RealOpc = S1C33::XOR_ri; break;
    default:              RealOpc = S1C33::SUB_ri;  break;
    }

    // Lower 6 bits of V as the instruction immediate.
    // For and/or/xor (simm6): sign-extend so AsmPrinter emits a value in -32..31.
    // For sub (uimm6): keep as unsigned 0..63 (V & 0x3F is always in 0..63).
    int64_t imm6 = (RealOpc == S1C33::SUB_ri)
                       ? static_cast<int64_t>(V & 0x3F)
                       : llvm::SignExtend64<6>(static_cast<uint64_t>(V) & 0x3F);

    if (isInt<6>(V)) {
      // No ext needed.
      BuildMI(MBB, MI, DL, get(RealOpc), Rd).addReg(Rd).addImm(V);
    } else if (isInt<19>(V)) {
      int64_t ext_imm13 = (V >> 6) & 0x1FFF;
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext_imm13);
      BuildMI(MBB, MI, DL, get(RealOpc), Rd).addReg(Rd).addImm(imm6);
    } else {
      int64_t ext2_imm13 = (V >> 6)  & 0x1FFF;
      int64_t ext1_imm13 = (V >> 19) & 0x1FFF;
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext1_imm13);
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext2_imm13);
      BuildMI(MBB, MI, DL, get(RealOpc), Rd).addReg(Rd).addImm(imm6);
    }
    MI.eraseFromParent();
    return true;
  }

  // LDxx_ri_off / STxx_ri_off pseudos: expand to ext $off; ld.x [$rb].
  // Offset is a 13-bit signed immediate supplied by the immSExt13 PatLeaf.
  case S1C33::LDB_ri_off:
  case S1C33::LDUB_ri_off:
  case S1C33::LDH_ri_off:
  case S1C33::LDUH_ri_off:
  case S1C33::LDW_ri_off: {
    Register Rd = MI.getOperand(0).getReg();
    Register Rb = MI.getOperand(1).getReg();
    int64_t  Off = MI.getOperand(2).getImm();

    unsigned RealOpc;
    switch (MI.getOpcode()) {
    case S1C33::LDB_ri_off:  RealOpc = S1C33::LDB_ri;  break;
    case S1C33::LDUB_ri_off: RealOpc = S1C33::LDUB_ri; break;
    case S1C33::LDH_ri_off:  RealOpc = S1C33::LDH_ri;  break;
    case S1C33::LDUH_ri_off: RealOpc = S1C33::LDUH_ri; break;
    default:                 RealOpc = S1C33::LDW_ri;  break;
    }
    // Emit ext (if offset != 0) then the base load instruction.
    if (Off != 0) {
      int64_t ext_imm13 = Off & 0x1FFF;
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext_imm13);
    }
    BuildMI(MBB, MI, DL, get(RealOpc), Rd).addReg(Rb);
    MI.eraseFromParent();
    return true;
  }

  case S1C33::STB_ri_off:
  case S1C33::STH_ri_off:
  case S1C33::STW_ri_off: {
    Register Rb  = MI.getOperand(0).getReg();
    int64_t  Off = MI.getOperand(1).getImm();
    Register Rs  = MI.getOperand(2).getReg();

    unsigned RealOpc;
    switch (MI.getOpcode()) {
    case S1C33::STB_ri_off: RealOpc = S1C33::STB_ri; break;
    case S1C33::STH_ri_off: RealOpc = S1C33::STH_ri; break;
    default:                RealOpc = S1C33::STW_ri; break;
    }
    if (Off != 0) {
      int64_t ext_imm13 = Off & 0x1FFF;
      BuildMI(MBB, MI, DL, get(S1C33::EXT)).addImm(ext_imm13);
    }
    BuildMI(MBB, MI, DL, get(RealOpc)).addReg(Rb).addReg(Rs);
    MI.eraseFromParent();
    return true;
  }
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
