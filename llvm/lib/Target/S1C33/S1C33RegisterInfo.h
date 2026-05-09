//===-- S1C33RegisterInfo.h - S1C33 Register Information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33REGISTERINFO_H
#define LLVM_LIB_TARGET_S1C33_S1C33REGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "S1C33GenRegisterInfo.inc"

namespace llvm {

class S1C33RegisterInfo : public S1C33GenRegisterInfo {
public:
  S1C33RegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                        CallingConv::ID CC) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                            unsigned FIOperandNum,
                            RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;

  bool getRegAllocationHints(Register VirtReg, ArrayRef<MCPhysReg> Order,
                              SmallVectorImpl<MCPhysReg> &Hints,
                              const MachineFunction &MF,
                              const VirtRegMap *VRM = nullptr,
                              const LiveRegMatrix *Matrix = nullptr) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33REGISTERINFO_H
