//===- S1C33.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// S1C33 ABI Implementation
//===----------------------------------------------------------------------===//

namespace {

class S1C33TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  S1C33TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override {
    if (GV->isDeclaration())
      return;
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    if (!FD)
      return;
    auto *Fn = cast<llvm::Function>(GV);

    // __attribute__((interrupt_handler)) → mark with "interrupt_handler" so
    // S1C33FrameLowering generates pushn %r15 / popn %r15 / reti.
    if (FD->getAttr<S1C33InterruptHandlerAttr>())
      Fn->addFnAttr("interrupt_handler");
  }
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createS1C33TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<S1C33TargetCodeGenInfo>(CGM.getTypes());
}
