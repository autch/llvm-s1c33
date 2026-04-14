//===-- S1C33RegisterInfo.cpp - S1C33 Register Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33RegisterInfo.h"
#include "S1C33.h"
#include "S1C33FrameLowering.h"
#include "S1C33Subtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_REGINFO_TARGET_DESC
#include "S1C33GenRegisterInfo.inc"

using namespace llvm;

S1C33RegisterInfo::S1C33RegisterInfo()
    : S1C33GenRegisterInfo(S1C33::PC) {}

const MCPhysReg *
S1C33RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  // ABI (S5U1C33000C): R0–R3 are callee-saved.
  static const MCPhysReg CalleeSavedRegs[] = {
      S1C33::R0, S1C33::R1, S1C33::R2, S1C33::R3, 0};
  return CalleeSavedRegs;
}

const uint32_t *
S1C33RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                         CallingConv::ID CC) const {
  return CSR_S1C33_RegMask;
}

BitVector
S1C33RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // R8: kernel table base pointer, always 0x0 per P/ECE calling convention.
  // The kernel sets R8 = 0x0 before calling any app callback; pceapi stubs
  // read kernel function pointers via "ext N / ld.w %r9, [%r8]".
  // User-compiled code must never modify R8.  Reserved to enforce this.
  Reserved.set(S1C33::R8);
  // R9: scratch (caller-saved), same as R4-R7.
  // Safe because all P/ECE kernel interrupt handlers use pushn %r15 / popn %r15
  // (INT_BEGIN/INT_END macros), which saves/restores R0-R15 including R9.
  // pceapi stubs use R9 as caller-saved scratch (ext33 ABI convention), so
  // treating R9 as caller-saved is ABI-compatible with SDK libraries.
  // Special registers are not allocatable.
  Reserved.set(S1C33::SP);
  Reserved.set(S1C33::PC);
  Reserved.set(S1C33::PSR);
  Reserved.set(S1C33::ALR);
  Reserved.set(S1C33::AHR);
  return Reserved;
}

bool S1C33RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                             int SPAdj, unsigned FIOperandNum,
                                             RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MI.getMF();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const S1C33Subtarget &STI = MF.getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
  DebugLoc DL = MI.getDebugLoc();

  // ADJFI: materialize frame slot address into a register.
  // Expand to: ld.w %dst, %sp; (add %dst, offset if offset != 0)
  if (MI.getOpcode() == S1C33::ADJFI) {
    Register Dst = MI.getOperand(0).getReg();
    int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
    int64_t Offset = MFI.getObjectOffset(FrameIndex) + MFI.getStackSize() + SPAdj;
    // Fixed frame objects (incoming stack args, FrameIndex < 0) sit above the
    // frame.  Apply the same adjustments as the standard eliminateFrameIndex
    // path: +4 for the return address pushed by 'call', plus the callee-saved
    // area (so that the materialized address is correct after the prologue).
    if (FrameIndex < 0) {
      Offset += 4; // return address
      if (MF.getFunction().hasFnAttribute("interrupt_handler")) {
        Offset += 64; // pushn %r15 saves R0–R15 = 16 × 4 bytes
      } else {
        const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
        if (!CSI.empty()) {
          unsigned MaxIdx = 0;
          for (const auto &CS : CSI) {
            MCPhysReg Reg = CS.getReg();
            if (Reg == S1C33::R0)      MaxIdx = std::max(MaxIdx, 0u);
            else if (Reg == S1C33::R1) MaxIdx = std::max(MaxIdx, 1u);
            else if (Reg == S1C33::R2) MaxIdx = std::max(MaxIdx, 2u);
            else if (Reg == S1C33::R3) MaxIdx = std::max(MaxIdx, 3u);
          }
          Offset += (MaxIdx + 1) * 4;
        }
      }
    }
    assert(Offset >= 0 && "Negative SP-relative frame offset for ADJFI");

    // Emit: ld.w Dst, %sp — CLASS 5 special register read (NOT MOV_rr)
    BuildMI(MBB, II, DL, TII.get(S1C33::LDW_from_SP), Dst);
    if (Offset != 0) {
      if (isUInt<6>(Offset)) {
        BuildMI(MBB, II, DL, TII.get(S1C33::ADD_ri), Dst)
            .addReg(Dst)
            .addImm(Offset);
      } else {
        // Use the late-expanded 3-operand pseudo so ext+add stays adjacent even
        // with the post-RA scheduler enabled.
        BuildMI(MBB, II, DL, TII.get(S1C33::ADD_rri), Dst)
            .addReg(Dst)
            .addImm(Offset);
      }
    }
    MI.eraseFromParent();
    return false;
  }

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(FrameIndex)
                   + MFI.getStackSize()
                   + SPAdj;

  // Fixed objects (FrameIndex < 0) are incoming stack arguments sitting above
  // the frame in the caller's space.  The formula above only adds LocalSize;
  // we also need to skip:
  //   • the 4-byte return address pushed by 'call', and
  //   • the callee-saved area emitted by pushn %rN in the prologue.
  if (FrameIndex < 0) {
    // Return address pushed by 'call'.
    Offset += 4;
    // Callee-saved area: pushn %rN pushes R0..RN = (N+1) × 4 bytes.
    if (MF.getFunction().hasFnAttribute("interrupt_handler")) {
      // ISR uses pushn %r15 → saves R0–R15 = 16 × 4 = 64 bytes.
      Offset += 64;
    } else {
      const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
      if (!CSI.empty()) {
        unsigned MaxIdx = 0;
        for (const auto &CS : CSI) {
          MCPhysReg Reg = CS.getReg();
          if (Reg == S1C33::R0)      MaxIdx = std::max(MaxIdx, 0u);
          else if (Reg == S1C33::R1) MaxIdx = std::max(MaxIdx, 1u);
          else if (Reg == S1C33::R2) MaxIdx = std::max(MaxIdx, 2u);
          else if (Reg == S1C33::R3) MaxIdx = std::max(MaxIdx, 3u);
        }
        Offset += (MaxIdx + 1) * 4;
      }
    }
  }

  // ADD_ri with a frame index in the source register slot (FIOperandNum == 1)
  // arises when the selector lowers ADD(FrameIndex, imm) as a plain ADD_ri.
  // eliminateFrameIndex's default path would call ChangeToImmediate() on the
  // frame-index register operand, producing a broken "add %rd, imm" where the
  // "base" is undefined.  Expand it here instead: emit ld.w Dst, SP (to
  // materialise the frame base address) followed by add Dst, TotalOffset.
  //
  // Use the already-computed Offset (which includes fixed-object adjustments
  // for the return address and callee-saved area) as the SP-relative base.
  if (MI.getOpcode() == S1C33::ADD_ri && FIOperandNum == 1) {
    Register Dst = MI.getOperand(0).getReg();
    int64_t ExtraImm = MI.getOperand(2).getImm();
    int64_t TotalOffset = Offset + ExtraImm;
    assert(TotalOffset >= 0 && "Negative SP-relative offset in ADD_ri FI expansion");

    BuildMI(MBB, II, DL, TII.get(S1C33::LDW_from_SP), Dst);
    if (TotalOffset != 0) {
      if (isUInt<6>(TotalOffset)) {
        BuildMI(MBB, II, DL, TII.get(S1C33::ADD_ri), Dst)
            .addReg(Dst)
            .addImm(TotalOffset);
      } else {
        BuildMI(MBB, II, DL, TII.get(S1C33::ADD_rri), Dst)
            .addReg(Dst)
            .addImm(TotalOffset);
      }
    }
    MI.eraseFromParent();
    return false;
  }

  // If the frame index landed in a register-indirect memory instruction instead
  // of the intended SP-relative form, switch to the SP-relative opcode here.
  // This happens when the instruction selector picks e.g. LDUB_ri with the
  // frame index in the %rb register slot.  Without this correction,
  // ChangeToImmediate(Offset) below would produce "ld.ub %rd, [43]" — a raw
  // integer in brackets — which the assembler rejects.
  static const std::pair<unsigned, unsigned> RiToSp[] = {
      {S1C33::LDB_ri,  S1C33::LDB_sp},
      {S1C33::LDUB_ri, S1C33::LDUB_sp},
      {S1C33::LDH_ri,  S1C33::LDH_sp},
      {S1C33::LDUH_ri, S1C33::LDUH_sp},
      {S1C33::LDW_ri,  S1C33::LDW_sp},
      {S1C33::STB_ri,  S1C33::STB_sp},
      {S1C33::STH_ri,  S1C33::STH_sp},
      {S1C33::STW_ri,  S1C33::STW_sp},
  };
  for (auto [Ri, Sp] : RiToSp) {
    if (MI.getOpcode() == Ri) {
      MI.setDesc(TII.get(Sp));
      break;
    }
  }

  // Handle _ri_off variants: the instruction selector may generate e.g.
  // STH_ri_off %stack.N, explicit_off, %rs  when the address is
  // (frameindex + constant).  We combine the FI-resolved SP offset with
  // the explicit operand offset, convert to the _sp opcode, and remove
  // the now-redundant explicit offset operand.
  //
  // Operand layout:
  //   Stores  (FI at FIOperandNum=0): rb[FI], off[1], rs[2] → imm[0], rs[1]
  //   Loads   (FI at FIOperandNum=1): rd[0], rb[FI=1], off[2] → rd[0], imm[1]
  // In both cases the explicit offset operand is at FIOperandNum+1.
  static const std::pair<unsigned, unsigned> RiOffToSp[] = {
      {S1C33::LDB_ri_off,  S1C33::LDB_sp},
      {S1C33::LDUB_ri_off, S1C33::LDUB_sp},
      {S1C33::LDH_ri_off,  S1C33::LDH_sp},
      {S1C33::LDUH_ri_off, S1C33::LDUH_sp},
      {S1C33::LDW_ri_off,  S1C33::LDW_sp},
      {S1C33::STB_ri_off,  S1C33::STB_sp},
      {S1C33::STH_ri_off,  S1C33::STH_sp},
      {S1C33::STW_ri_off,  S1C33::STW_sp},
  };
  for (auto [RiOff, Sp] : RiOffToSp) {
    if (MI.getOpcode() == RiOff) {
      unsigned OffOperandNum = FIOperandNum + 1;
      Offset += MI.getOperand(OffOperandNum).getImm();
      MI.removeOperand(OffOperandNum);
      MI.setDesc(TII.get(Sp));
      break;
    }
  }

  // S1C33 SP-relative class-2 instructions have two offset encodings:
  //   • no EXT:  imm6 is scaled by access size (word ×4, halfword ×2, byte ×1)
  //   • with EXT: ext+imm6 forms a byte displacement directly
  // For example, byte offset 52 without EXT is encoded as ld.w [%sp+0xd], but
  // byte offset 380 with EXT is encoded as ext 5 / ld.w [%sp+60].
  assert(Offset >= 0 && "Negative SP-relative frame offset");

  // Determine scale factor from instruction opcode.
  unsigned Scale = 4; // default: word
  switch (MI.getOpcode()) {
  case S1C33::LDB_sp: case S1C33::LDUB_sp:
  case S1C33::STB_sp:
    Scale = 1;
    break;
  case S1C33::LDH_sp: case S1C33::LDUH_sp:
  case S1C33::STH_sp:
    Scale = 2;
    break;
  default: // LDW_sp, STW_sp, ADDSP_i, SUBSP_i
    Scale = 4;
    break;
  }
  assert((Offset % Scale) == 0 &&
         "SP-relative offset not aligned to access size");
  int64_t ScaledOffset = Offset / Scale;
  int64_t EncodedOffset = ScaledOffset;

  // Without EXT the 6-bit field is scaled. Once EXT is present, the combined
  // displacement is a raw byte offset whose low 6 bits live in the instruction.
  if (!isUInt<6>(ScaledOffset)) {
    EncodedOffset = Offset;
    if (isUInt<19>(EncodedOffset)) {
      int64_t ext_imm13 = (EncodedOffset >> 6) & 0x1FFF;
      BuildMI(MBB, II, DL, TII.get(S1C33::EXT)).addImm(ext_imm13);
    } else {
      int64_t ext2_imm13 = (EncodedOffset >> 6)  & 0x1FFF;
      int64_t ext1_imm13 = (EncodedOffset >> 19) & 0x1FFF;
      BuildMI(MBB, II, DL, TII.get(S1C33::EXT)).addImm(ext1_imm13);
      BuildMI(MBB, II, DL, TII.get(S1C33::EXT)).addImm(ext2_imm13);
    }
  }

  // Use scaled units for plain imm6, but raw byte displacement once EXT is
  // present. The low 6 bits must satisfy the access-size alignment.
  MI.getOperand(FIOperandNum).ChangeToImmediate(EncodedOffset & 0x3F);
  return false;
}

Register
S1C33RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return S1C33::SP;
}
