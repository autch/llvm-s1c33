//===-- S1C33TargetMachine.h - Define TargetMachine for S1C33 ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33TARGETMACHINE_H
#define LLVM_LIB_TARGET_S1C33_S1C33TARGETMACHINE_H

#include "S1C33Subtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include <optional>

namespace llvm {

class S1C33TargetMachine : public CodeGenTargetMachineImpl {
  S1C33Subtarget Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  S1C33TargetMachine(const Target &TheTarget, const Triple &TargetTriple,
                      StringRef Cpu, StringRef FeatureString,
                      const TargetOptions &Options,
                      std::optional<Reloc::Model> RM,
                      std::optional<CodeModel::Model> CodeModel,
                      CodeGenOptLevel OptLevel, bool JIT);

  const S1C33Subtarget *
  getSubtargetImpl(const Function & /*F*/) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                             const TargetSubtargetInfo *STI) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33TARGETMACHINE_H
