//===-- S1C33TargetMachine.cpp - Define TargetMachine for S1C33 -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33TargetMachine.h"
#include "S1C33MachineFunctionInfo.h"
#include "S1C33.h"
#include "TargetInfo/S1C33TargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeS1C33Target() {
  RegisterTargetMachine<S1C33TargetMachine> X(getTheS1C33Target());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeS1C33AsmPrinterPass(PR);
  initializeS1C33DAGToDAGISelLegacyPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  // Default to static relocation for embedded targets.
  return RM.value_or(Reloc::Static);
}

S1C33TargetMachine::S1C33TargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, TT.computeDataLayout(), TT, CPU, FS, Options,
          getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CM, CodeModel::Small), OL),
      Subtarget(TT, CPU, FS, *this, Options, getCodeModel(), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) {
  initAsmInfo();
}

namespace {

class S1C33PassConfig : public TargetPassConfig {
public:
  S1C33PassConfig(S1C33TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  S1C33TargetMachine &getS1C33TargetMachine() const {
    return getTM<S1C33TargetMachine>();
  }

  bool addInstSelector() override;
  void addPreEmitPass() override;
};

} // namespace

TargetPassConfig *
S1C33TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new S1C33PassConfig(*this, PM);
}

bool S1C33PassConfig::addInstSelector() {
  addPass(createS1C33ISelDag(getS1C33TargetMachine()));
  return false;
}

MachineFunctionInfo *S1C33TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return S1C33MachineFunctionInfo::create<S1C33MachineFunctionInfo>(Allocator,
                                                                     F, STI);
}

void S1C33PassConfig::addPreEmitPass() {
  // Expand ext-producing pseudos (MOV_ri32, ALU_ri32, offset loads/stores)
  // AFTER the post-RA scheduler so that ext+target pairs aren't split.
  addPass(createS1C33ExpandExtPseudosPass());
  // Run the delay slot filler after ext expansion.
  addPass(createS1C33DelaySlotFillerPass());
}
