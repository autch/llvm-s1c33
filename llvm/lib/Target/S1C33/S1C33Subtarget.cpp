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
#include "llvm/CodeGen/LibcallLoweringInfo.h"
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
      InstrInfo(initializeSubtargetDependencies(CPU, FS)), FrameLowering(),
      TLInfo(TM, *this) {}

S1C33Subtarget &S1C33Subtarget::initializeSubtargetDependencies(StringRef CPU,
                                                                StringRef FS) {
  std::string CPUName = std::string(CPU);
  if (CPUName.empty())
    CPUName =
        "s1c33209"; // Default to S1C33209 (P/ECE SoC with hardware multiplier)
  ParseSubtargetFeatures(CPUName, CPUName, FS);
  return *this;
}

void S1C33Subtarget::initLibcallLoweringInfo(LibcallLoweringInfo &Info) const {
  // The TableGen-driven RTLCI tables do not register plain memcpy/memmove/
  // memset for the "s1c33-none-elf" triple, so PreISelIntrinsicLowering's
  // canEmitMemcpy() would return false and expand any llvm.memcpy larger than
  // the default 64-byte threshold into an inline byte-copy loop — disastrous
  // for -Oz.  Register them explicitly here so that large memcpys lower to a
  // call to memcpy().  (RTLIB::MEMCPY is also set on TargetLowering's libcall
  // table in S1C33TargetLowering's constructor for SelectionDAG's own memcpy
  // lowering path; the two tables are separate.)
  Info.setLibcallImpl(RTLIB::MEMCPY, RTLIB::impl_memcpy);
  Info.setLibcallImpl(RTLIB::MEMMOVE, RTLIB::impl_memmove);
  Info.setLibcallImpl(RTLIB::MEMSET, RTLIB::impl_memset);
}
