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
      return ELF::R_S1C33_REL8;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_21)
      return ELF::R_S1C33_REL21;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_h)
      return ELF::R_S1C33_REL_H;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_m)
      return ELF::R_S1C33_REL_M;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_l)
      return ELF::R_S1C33_REL_L;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_h)
      return ELF::R_S1C33_ABS_H;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_m)
      return ELF::R_S1C33_ABS_M;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_l)
      return ELF::R_S1C33_ABS_L;
    // 26-bit absolute split (for ext/ext/ld.*[%r8] pattern).  Reuses the
    // existing REL_AH/REL_AL numbers (10/11) — the 'REL_' prefix is historical
    // from gcc33 SRF naming; the effective semantics is absolute.  See the
    // provenance note in llvm/BinaryFormat/ELFRelocs/S1C33.def.
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_ah)
      return ELF::R_S1C33_REL_AH;
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_al)
      return ELF::R_S1C33_REL_AL;

    // Standard LLVM fixup kinds: used by eh_frame / debug info data.
    switch (Kind) {
    case FK_Data_4:
      return ELF::R_S1C33_32;
    case FK_Data_1:
    case FK_Data_2:
    case FK_Data_8:
      reportError(Fixup.getLoc(), "unsupported data relocation size");
      return ELF::R_S1C33_NONE;
    default:
      reportError(Fixup.getLoc(), "unsupported S1C33 relocation type");
      return ELF::R_S1C33_NONE;
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
