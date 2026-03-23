//===-- S1C33TargetInfo.cpp - S1C33 Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheS1C33Target() {
  static Target TheS1C33Target;
  return TheS1C33Target;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeS1C33TargetInfo() {
  RegisterTarget<Triple::s1c33, /*HasJIT=*/false> X(
      getTheS1C33Target(), "s1c33", "EPSON S1C33000", "S1C33");
}
