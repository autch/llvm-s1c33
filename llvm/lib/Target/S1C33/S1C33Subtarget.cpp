//===-- S1C33Subtarget.cpp - S1C33 Subtarget Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33Subtarget.h"
#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "s1c33-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "S1C33GenSubtargetInfo.inc"

S1C33Subtarget::S1C33Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                                  const TargetMachine &TM,
                                  const TargetOptions &Options,
                                  CodeModel::Model CM, CodeGenOptLevel OL)
    : S1C33GenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      InstrInfo(initializeSubtargetDependencies(CPU, FS)),
      FrameLowering(),
      TLInfo(TM, *this) {
}

S1C33Subtarget &
S1C33Subtarget::initializeSubtargetDependencies(StringRef CPU, StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName = "s1c33209"; // Default to S1C33209 (P/ECE SoC with hardware multiplier)
  ParseSubtargetFeatures(CPUName, CPUName, FS);
  return *this;
}
