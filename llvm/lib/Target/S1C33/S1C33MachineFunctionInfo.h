//===-- S1C33MachineFunctionInfo.h - S1C33 Machine Function Info -*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_S1C33_S1C33MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// S1C33MachineFunctionInfo - per-function target-specific information.
class S1C33MachineFunctionInfo : public MachineFunctionInfo {
  /// Frame index for the first variadic argument.
  /// Valid only in variadic functions; set in LowerFormalArguments.
  int VarArgsFrameIndex = 0;

public:
  S1C33MachineFunctionInfo() = default;
  explicit S1C33MachineFunctionInfo(const Function &F,
                                    const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<S1C33MachineFunctionInfo>(*this);
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33MACHINEFUNCTIONINFO_H
