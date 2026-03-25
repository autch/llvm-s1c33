//===-- S1C33ELFObjectWriter.cpp - S1C33 ELF writer -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33FixupKinds.h"
#include "S1C33MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

// S1C33 ELF relocation types (project-local definitions, DESIGN_SPEC §7).
// Stored in r_type field of ELF relocation entries.
enum {
  R_S1C33_NONE  = 0, // No relocation
  R_S1C33_REL8  = 1, // 8-bit PC-relative (call/branch sign8 field)
  R_S1C33_32    = 2, // 32-bit absolute (used for eh_frame / data)
  R_S1C33_ABS_H = 3, // bits[31:19] → ext_h imm13
  R_S1C33_ABS_M = 4, // bits[18:6]  → ext_m imm13
  R_S1C33_ABS_L = 5, // bits[5:0]   → ld.w/etc. imm6
  R_S1C33_REL21 = 6, // 21-bit PC-relative (ext+branch/call)
  R_S1C33_REL_H = 7, // bits[31:22] → ext_h for PC-rel (<<3)
  R_S1C33_REL_M = 8, // bits[21:9]  → ext_m for PC-rel
  R_S1C33_REL_L = 9, // bits[8:1]   → branch sign8 field
};

namespace {

class S1C33ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  S1C33ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/false, OSABI, ELF::EM_SE_C33,
                                /*HasRelAddend=*/true) {}

  ~S1C33ELFObjectWriter() override = default;

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    MCFixupKind Kind = Fixup.getKind();

    // Raw relocation kinds (from .reloc directives) pass through as-is.
    if (mc::isRelocation(Kind))
      return Kind;

    // S1C33-specific fixup kinds.
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_8)
      return R_S1C33_REL8;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_21)
      return R_S1C33_REL21;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_h)
      return R_S1C33_ABS_H;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_m)
      return R_S1C33_ABS_M;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_l)
      return R_S1C33_ABS_L;

    // Standard LLVM fixup kinds: used by eh_frame / debug info data.
    switch (Kind) {
    case FK_Data_4:
      return R_S1C33_32;
    case FK_Data_1:
    case FK_Data_2:
    case FK_Data_8:
      reportError(Fixup.getLoc(), "unsupported data relocation size");
      return R_S1C33_NONE;
    default:
      reportError(Fixup.getLoc(), "unsupported S1C33 relocation type");
      return R_S1C33_NONE;
    }
  }
};

} // end anonymous namespace

namespace llvm {

std::unique_ptr<MCObjectTargetWriter>
createS1C33ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<S1C33ELFObjectWriter>(OSABI);
}

} // namespace llvm
