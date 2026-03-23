//===-- S1C33FrameLowering.h - S1C33 Frame Lowering -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33FRAMELOWERING_H
#define LLVM_LIB_TARGET_S1C33_S1C33FRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class S1C33FrameLowering : public TargetFrameLowering {
public:
  explicit S1C33FrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                            /*StackAlignment=*/Align(4),
                            /*LocalAreaOffset=*/0) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  // Expand ADJCALLSTACKDOWN/UP to sub/add %sp when stack-passed args exist.
  // For register-only calls the size is 0 and the pseudos are just removed.
  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF,
                                MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;

  // S1C33 does not use a frame pointer — SP-based addressing only.
  bool hasFPImpl(const MachineFunction &MF) const override { return false; }

  // pushn/popn handle callee-saved registers; tell LLVM not to allocate
  // frame slots for them (which would cause double-counting with pushn).
  bool assignCalleeSavedSpillSlots(MachineFunction &MF,
                                   const TargetRegisterInfo *TRI,
                                   std::vector<CalleeSavedInfo> &CSI) const override;

  // Callee-saved spilling/restoring is done in emitPrologue/emitEpilogue via
  // pushn/popn; these overrides suppress the default individual-store mechanism.
  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;

  bool restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI,
                                   MutableArrayRef<CalleeSavedInfo> CSI,
                                   const TargetRegisterInfo *TRI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33FRAMELOWERING_H
