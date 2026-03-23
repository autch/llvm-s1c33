//===-- S1C33MCAsmInfo.h - S1C33 asm properties -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCASMINFO_H
#define LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {

class Triple;

class S1C33MCAsmInfo : public MCAsmInfoELF {
  void anchor() override;

public:
  explicit S1C33MCAsmInfo(const Triple &TheTriple,
                          const MCTargetOptions &Options);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCASMINFO_H
