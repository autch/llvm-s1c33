//===-- S1C33Subtarget.h - Define Subtarget for S1C33 ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33SUBTARGET_H
#define LLVM_LIB_TARGET_S1C33_S1C33SUBTARGET_H

#include "S1C33FrameLowering.h"
#include "S1C33ISelLowering.h"
#include "S1C33InstrInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "S1C33GenSubtargetInfo.inc"

namespace llvm {

class S1C33Subtarget : public S1C33GenSubtargetInfo {
  // SubtargetFeature: S1C33209 optional hardware multiplier.
  bool HasHWMul = false;

  // SubtargetFeature: enable R8-based absolute addressing for single-use
  // globals.  Default true.  Toggle with -mattr=+/-r8-abs.
  bool HasR8AbsGlobal = false;

  SelectionDAGTargetInfo TSInfo;
  S1C33InstrInfo InstrInfo;
  S1C33FrameLowering FrameLowering;
  S1C33TargetLowering TLInfo;

public:
  S1C33Subtarget(const Triple &TT, StringRef CPU, StringRef FS,
                  const TargetMachine &TM, const TargetOptions &Options,
                  CodeModel::Model CM, CodeGenOptLevel OL);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  S1C33Subtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS);

  bool hasHWMul() const { return HasHWMul; }

  bool hasR8AbsGlobal() const { return HasR8AbsGlobal; }

  // Enable post-RA list scheduler to use the SchedMachineModel latencies
  // for reordering instructions (e.g., hiding load-use penalties).
  bool enablePostRAScheduler() const override { return true; }

  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }

  const S1C33InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const S1C33RegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const S1C33TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }

  // Register runtime libcall implementations at module analysis level so that
  // PreISelIntrinsicLowering's canEmitMemcpy/canEmitLibcall checks succeed and
  // large llvm.memcpy/memmove/memset intrinsics lower to libcalls instead of
  // being expanded into byte-copy loops (triggered when size exceeds
  // TTI::getMaxMemIntrinsicInlineSizeThreshold, default 64).
  void initLibcallLoweringInfo(LibcallLoweringInfo &Info) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33SUBTARGET_H
