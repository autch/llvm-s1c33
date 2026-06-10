//===-- S1C33FrameLowering.cpp - S1C33 Frame Lowering ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S5U1C33000C ABI stack layout (SP grows downward, 4-byte aligned):
//
//   [high address]
//   incoming arguments (if > 4 words, passed on stack by caller)
//   return address (pushed by 'call' instruction to stack as return PC?)
//   -- No: S1C33 'call' saves return address in link register R14? No.
//   -- Actually: S1C33 'call' pushes return address onto stack.
//   callee-saved registers (pushn %r3 saves R0,R1,R2,R3)
//   local variables
//   [low address / current SP]
//
//===----------------------------------------------------------------------===//

#include "S1C33FrameLowering.h"
#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33InstrInfo.h"
#include "S1C33Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

// Map a callee-saved register to its position in R0–R3.
// pushn %rN pushes R0..RN, so we need the highest index used.
static unsigned getCalleeSavedRegIdx(MCPhysReg Reg) {
  if (Reg == S1C33::R0)
    return 0;
  if (Reg == S1C33::R1)
    return 1;
  if (Reg == S1C33::R2)
    return 2;
  if (Reg == S1C33::R3)
    return 3;
  llvm_unreachable("unexpected callee-saved register");
}

static const MCPhysReg CalleeSavedByIdx[] = {S1C33::R0, S1C33::R1, S1C33::R2,
                                             S1C33::R3};

// Adjust SP by WordSize words using one or more sub/add %sp instructions.
// The imm10 field is in word units and — uniquely among S1C33 immediates —
// accepts no ext prefix, so a single instruction tops out at 1023 words
// (4092 bytes).  Larger frames are rare but reachable (e.g. a big local
// array, or the outgoing-argument area for a struct passed by value), and
// truncating the immediate would corrupt the stack silently; split the
// adjustment into 1023-word chunks instead.
static void emitSPAdjustment(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             const DebugLoc &DL, const S1C33InstrInfo &TII,
                             unsigned Opcode, uint64_t WordSize) {
  while (WordSize > 0) {
    uint64_t Chunk = std::min<uint64_t>(WordSize, 1023);
    BuildMI(MBB, MBBI, DL, TII.get(Opcode)).addImm(Chunk);
    WordSize -= Chunk;
  }
}

// Expand ADJCALLSTACKDOWN/UP pseudo instructions.
// When hasReservedCallFrame() is true the prologue already includes space for
// the maximum outgoing call frame, so we just erase the pseudo.  Otherwise
// (e.g. with a frame pointer or variable-length arrays) we emit dynamic
// sub/add %sp.
MachineBasicBlock::iterator S1C33FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  if (!hasReservedCallFrame(MF)) {
    const S1C33Subtarget &STI = MF.getSubtarget<S1C33Subtarget>();
    const S1C33InstrInfo &TII =
        *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
    DebugLoc DL = MI->getDebugLoc();
    unsigned Opcode = MI->getOpcode();
    int64_t Amount = MI->getOperand(0).getImm();

    // Currently unreachable: dynamic_stackalloc is not selectable, so
    // hasVarSizedObjects() is never true and the reserved-call-frame path
    // above always applies.  Kept correct anyway: Amount is in bytes and
    // sub/add %sp take word-unit immediates capped at 1023.
    if (Amount != 0) {
      uint64_t WordSize = alignTo(Amount, 4) / 4;
      unsigned SPOpc =
          Opcode == S1C33::ADJCALLSTACKDOWN ? S1C33::SUBSP_i : S1C33::ADDSP_i;
      emitSPAdjustment(MBB, MI, DL, TII, SPOpc, WordSize);
    }
  }
  return MBB.erase(MI);
}

// Emit the function prologue:
//   1. pushn %rN  — save callee-saved registers R0..RN (ABI: R0–R3)
//   2. sub %sp, FrameSize — allocate local variable space
void S1C33FrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const S1C33Subtarget &STI = MF.getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  bool IsISR = MF.getFunction().hasFnAttribute("interrupt_handler");

  if (IsISR) {
    // Interrupt handler: save all registers R0–R15 with pushn %r15.
    // The interrupt entry has already pushed PSR and PC onto the stack;
    // pushn %r15 saves the remaining general-purpose registers.
    BuildMI(MBB, MBBI, DL, TII.get(S1C33::PUSHN))
        .addReg(S1C33::R15, RegState::Kill);
  } else {
    // Emit pushn %rN for callee-saved registers.
    // pushn %rN pushes R0..RN; find the highest callee-saved register in use.
    const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
    if (!CSI.empty()) {
      unsigned MaxIdx = 0;
      for (const CalleeSavedInfo &CS : CSI) {
        unsigned Idx = getCalleeSavedRegIdx(CS.getReg());
        if (Idx > MaxIdx)
          MaxIdx = Idx;
      }
      MCPhysReg HighestReg = CalleeSavedByIdx[MaxIdx];
      BuildMI(MBB, MBBI, DL, TII.get(S1C33::PUSHN))
          .addReg(HighestReg, RegState::Kill);
    }
  }

  // Allocate local variable space with sub %sp, FrameSize.
  // StackSize does NOT include callee-saved registers
  // (assignCalleeSavedSpillSlots returns true, preventing frame slot allocation
  // for callee-saved regs).
  //
  // S1C33 hardware interprets the imm10 field in word units (×4).
  // gcc33 encodes "sub %sp, 0x8" for 32 bytes (8 words × 4).
  //
  // Round StackSize up to a multiple of 4.  TargetFrameLowering's
  // StackAlignment=Align(4) covers the top of the frame, but individual
  // object alignment plus odd-size objects (e.g. i8 spills) can leave the
  // total un-aligned.  Pad here and write the adjusted size back so
  // eliminateFrameIndex sees the same value.
  uint64_t StackSize = alignTo(MFI.getStackSize(), 4);
  MFI.setStackSize(StackSize);
  emitSPAdjustment(MBB, MBBI, DL, TII, S1C33::SUBSP_i, StackSize / 4);
}

// Emit the function epilogue:
//   1. add %sp, FrameSize — free local variable space
//   2. popn %rN  — restore callee-saved registers RN..R0 (ABI: R0–R3)
void S1C33FrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const S1C33Subtarget &STI = MF.getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL = MBBI->getDebugLoc();

  bool IsISR = MF.getFunction().hasFnAttribute("interrupt_handler");

  // Free local variable space (insert before MBBI = ret/reti).
  // S1C33 hardware interprets the imm10 field in word units (×4).
  // emitPrologue rounded MFI's stack size up to a multiple of 4 already.
  uint64_t StackSize = MFI.getStackSize();
  assert((StackSize & 3) == 0 && "Stack size not aligned by emitPrologue");
  emitSPAdjustment(MBB, MBBI, DL, TII, S1C33::ADDSP_i, StackSize / 4);

  if (IsISR) {
    // Interrupt handler: restore all registers R15–R0 with popn %r15.
    // reti is already emitted by isel (RETI_FLAG → RETI); insert popn before
    // it.
    BuildMI(MBB, MBBI, DL, TII.get(S1C33::POPN), S1C33::R15);
  } else {
    // Emit popn %rN for callee-saved registers.
    // popn %rN pops RN..R0; find the highest callee-saved register in use.
    const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
    if (!CSI.empty()) {
      unsigned MaxIdx = 0;
      for (const CalleeSavedInfo &CS : CSI)
        MaxIdx = std::max(MaxIdx, getCalleeSavedRegIdx(CS.getReg()));
      MCPhysReg HighestReg = CalleeSavedByIdx[MaxIdx];
      BuildMI(MBB, MBBI, DL, TII.get(S1C33::POPN), HighestReg);
    }
  }
}

// Returning true prevents LLVM from allocating frame slots for callee-saved
// registers.  Without this, getStackSize() would include callee-saved space,
// causing double-counting when pushn/popn also adjust SP.
bool S1C33FrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  return true;
}

// Callee-saved spilling is handled in emitPrologue via pushn.
bool S1C33FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  return true;
}

// Callee-saved restoring is handled in emitEpilogue via popn.
bool S1C33FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  return true;
}
