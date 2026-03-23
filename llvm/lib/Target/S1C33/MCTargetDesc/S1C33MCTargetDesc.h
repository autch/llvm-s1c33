//===-- S1C33MCTargetDesc.h - S1C33 Target Descriptions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCTARGETDESC_H
#define LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCTARGETDESC_H

#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/DataTypes.h"

namespace llvm {

class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;

MCCodeEmitter *createS1C33MCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

MCAsmBackend *createS1C33AsmBackend(const Target &T,
                                     const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createS1C33ELFObjectWriter(uint8_t OSABI);

} // namespace llvm

// Defines symbolic names for S1C33 registers.
#define GET_REGINFO_ENUM
#include "S1C33GenRegisterInfo.inc"

// Defines symbolic names for S1C33 instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "S1C33GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "S1C33GenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCTARGETDESC_H
