//===--- S1C33.cpp - Implement S1C33 target feature support ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

// R0–R15 (ABI: R0–R3 callee-saved, R4–R7 scratch, R8 GP,
//              R10 return, R12–R15 args)
const char *const S1C33TargetInfo::GCCRegNames[] = {
    "r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};

ArrayRef<const char *> S1C33TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void S1C33TargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  Builder.defineMacro("__s1c33__");
  Builder.defineMacro("__S1C33__");
  Builder.defineMacro("__s1c33");
}
