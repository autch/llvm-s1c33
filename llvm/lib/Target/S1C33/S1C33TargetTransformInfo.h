//===- S1C33TargetTransformInfo.h - S1C33 specific TTI ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file defines a TargetTransformInfoImplBase conforming object specific
/// to the S1C33 target machine.  It lets target-independent passes
/// (ConstantHoisting, inliner cost model, loop optimizations, ...) query
/// accurate materialization costs for integer immediates on S1C33, where a
/// constant requires 1, 2, or 3 instructions depending on its value.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_S1C33_S1C33TARGETTRANSFORMINFO_H

#include "S1C33Subtarget.h"
#include "S1C33TargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/Function.h"

namespace llvm {

class S1C33TTIImpl final : public BasicTTIImplBase<S1C33TTIImpl> {
  using BaseT = BasicTTIImplBase<S1C33TTIImpl>;
  using TTI = TargetTransformInfo;

  friend BaseT;

  const S1C33Subtarget *ST;
  const S1C33TargetLowering *TLI;

  const S1C33Subtarget *getST() const { return ST; }
  const S1C33TargetLowering *getTLI() const { return TLI; }

public:
  explicit S1C33TTIImpl(const S1C33TargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind) const override;

  InstructionCost getIntImmCostInst(unsigned Opcode, unsigned Idx,
                                    const APInt &Imm, Type *Ty,
                                    TTI::TargetCostKind CostKind,
                                    Instruction *Inst = nullptr) const override;

  InstructionCost
  getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx, const APInt &Imm,
                      Type *Ty, TTI::TargetCostKind CostKind) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33TARGETTRANSFORMINFO_H
