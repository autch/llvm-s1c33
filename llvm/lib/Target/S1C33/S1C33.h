//===-- S1C33.h - Top-level interface for S1C33 backend ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33_H
#define LLVM_LIB_TARGET_S1C33_S1C33_H

#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class S1C33TargetMachine;
class FunctionPass;
class PassRegistry;

FunctionPass *createS1C33ISelDag(S1C33TargetMachine &TM);
FunctionPass *createS1C33ExpandExtPseudosPass();
FunctionPass *createS1C33DelaySlotFillerPass();
FunctionPass *createS1C33HoistImmInLoopPass();

void initializeS1C33AsmPrinterPass(PassRegistry &);
void initializeS1C33DAGToDAGISelLegacyPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33_H
