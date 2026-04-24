//===-- S1C33AsmBackend.cpp - S1C33 Assembler Backend ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33FixupKinds.h"
#include "S1C33MCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class S1C33AsmBackend : public MCAsmBackend {
public:
  S1C33AsmBackend() : MCAsmBackend(llvm::endianness::little) {}

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    // Table indexed by (Kind - FirstTargetFixupKind).
    static const MCFixupKindInfo Infos[S1C33::NumTargetFixupKinds] = {
        // fixup_s1c33_pc_rel_8:
        //   Field: bits[7:0] of the 16-bit instruction (LE byte 0).
        {"fixup_s1c33_pc_rel_8", 0, 8, 0},
        // fixup_s1c33_pc_rel_21:
        //   Covers all 32 bits of the 4-byte ext+branch sequence.
        //   applyFixup patches ext imm13 (Data[0..1]) and branch sign8 (Data[2]).
        {"fixup_s1c33_pc_rel_21", 0, 32, 0},
        // fixup_s1c33_pc_rel_h/m/l:
        //   Split 32-bit PC-relative ext+ext+branch/call relocation pieces.
        {"fixup_s1c33_pc_rel_h", 0, 13, 0},
        {"fixup_s1c33_pc_rel_m", 0, 13, 0},
        {"fixup_s1c33_pc_rel_l", 0, 8, 0},
        // fixup_s1c33_abs_h: bits[31:19] of absolute addr → ext_h imm13 field.
        {"fixup_s1c33_abs_h", 0, 13, 0},
        // fixup_s1c33_abs_m: bits[18:6] of absolute addr → ext_m imm13 field.
        {"fixup_s1c33_abs_m", 0, 13, 0},
        // fixup_s1c33_abs_l: bits[5:0] of absolute addr → ld.w imm6 field.
        {"fixup_s1c33_abs_l", 4, 6, 0},
        // fixup_s1c33_abs_ah: bits[25:13] of 26-bit absolute addr → ext_hi imm13.
        {"fixup_s1c33_abs_ah", 0, 13, 0},
        // fixup_s1c33_abs_al: bits[12:0] of 26-bit absolute addr → ext_lo imm13.
        {"fixup_s1c33_abs_al", 0, 13, 0},
    };
    static_assert(std::size(Infos) == S1C33::NumTargetFixupKinds,
                  "Fixup kinds table size mismatch");

    if (mc::isRelocation(Kind))
      return {};

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);

    assert(unsigned(Kind - FirstTargetFixupKind) < S1C33::NumTargetFixupKinds &&
           "Invalid fixup kind");
    return Infos[Kind - FirstTargetFixupKind];
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    MCFixupKind Kind = Fixup.getKind();

    // For unresolved fixups, record an ELF relocation so the linker patches
    // the field.  With SHT_RELA the addend is stored in the relocation entry
    // (r_addend), so we don't need to write it into the section data.
    if (!IsResolved) {
      Asm->getWriter().recordRelocation(F, Fixup, Target, Value);
      return;
    }

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_8) {
      // S1C33 PC-relative branch encoding:
      //   target = instr_addr + 2 * sign8
      //   sign8  = Value / 2  where Value = target - instr_addr
      int64_t Offset = (int64_t)Value;
      assert(Offset % 2 == 0 && "PC-relative branch to misaligned target");
      int64_t Sign8 = Offset / 2;
      if (Sign8 < -128 || Sign8 > 127)
        Asm->getContext().reportError(Fixup.getLoc(),
                                      "branch target out of range");
      // Data[0] is the sign8 byte (bits[7:0] of the 16-bit instruction).
      // The Data pointer is already offset to the start of this fixup's field.
      Data[0] = static_cast<uint8_t>(Sign8);
      return;
    }

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_21) {
      // 4-byte ext+branch sequence.  Fixup is at offset 0 (start of ext word).
      // S1C33 branch formula (with ext): target = call_addr + 2 * extended_imm
      //   call_addr = ext_addr + 2, so:
      //   extended_imm = (target - call_addr) / 2 = (Value - 2) / 2
      //   where Value = target - ext_addr (PCRel=true fixup)
      //   imm13    = (extended_imm >> 8) & 0x1FFF  → ext word bits[12:0]
      //   sign8_raw = extended_imm & 0xFF           → branch word bits[7:0]
      int64_t SValue = (int64_t)Value;
      int64_t Offset = SValue - 2;
      assert(Offset % 2 == 0 && "PC-relative branch to misaligned target");
      int64_t ExtImm = Offset / 2;
      if (ExtImm < -(1 << 20) || ExtImm > ((1 << 20) - 1))
        Asm->getContext().reportError(Fixup.getLoc(),
                                      "branch target out of ext1 range");
      uint32_t Raw21  = static_cast<uint32_t>(ExtImm & 0x1FFFFF);
      uint32_t Imm13  = (Raw21 >> 8) & 0x1FFF;
      uint8_t  Sign8  = static_cast<uint8_t>(Raw21 & 0xFF);
      // Data is already offset to the start of the ext word.
      // Patch ext word (Data[0..1]):
      //   ext = 0xC000 | imm13  →  LE: [imm13[7:0], 0xC0|(imm13[12:8])]
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      // Patch branch sign8 field (Data[2] = LE byte 0 of branch word).
      Data[2] = Sign8;
      return;
    }

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_h) {
      int64_t ByteOff = static_cast<int64_t>(Value) - 4;
      uint32_t Imm13 = static_cast<uint32_t>((ByteOff >> 22) & 0x1FFF);
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      return;
    }
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_m) {
      int64_t ByteOff = static_cast<int64_t>(Value) - 2;
      uint32_t Imm13 = static_cast<uint32_t>((ByteOff >> 9) & 0x1FFF);
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      return;
    }
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_l) {
      int64_t ByteOff = static_cast<int64_t>(Value);
      assert((ByteOff & 1) == 0 && "PC-relative branch to misaligned target");
      Data[0] = static_cast<uint8_t>((ByteOff >> 1) & 0xFF);
      return;
    }

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_h) {
      uint32_t Imm13 = ((uint32_t)Value >> 19) & 0x1FFF;
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      return;
    }
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_m) {
      uint32_t Imm13 = ((uint32_t)Value >> 6) & 0x1FFF;
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      return;
    }
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_l) {
      // sign6 occupies bits[9:4] of the ld.w word: high nibble of byte 0, low 2 bits of byte 1.
      uint8_t Sign6Raw = static_cast<uint8_t>(Value & 0x3F);
      Data[0] |= static_cast<uint8_t>(Sign6Raw << 4);
      Data[1] |= static_cast<uint8_t>((Sign6Raw >> 4) & 0x03);
      return;
    }

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_ah) {
      // 26-bit absolute address: bits[25:13] into the hi ext's imm13 field.
      // P/ECE memory map fits in 26 bits; anything beyond is a linker error.
      if (static_cast<uint64_t>(Value) >> 26)
        Asm->getContext().reportError(
            Fixup.getLoc(),
            "absolute address does not fit in 26 bits for [%r8] addressing");
      uint32_t Imm13 = ((uint32_t)Value >> 13) & 0x1FFF;
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      return;
    }
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_al) {
      uint32_t Imm13 = (uint32_t)Value & 0x1FFF;
      Data[0] = static_cast<uint8_t>(Imm13 & 0xFF);
      Data[1] = static_cast<uint8_t>(0xC0 | ((Imm13 >> 8) & 0x1F));
      return;
    }

    // FK_Data_4 → R_S1C33_32: fully-resolved 32-bit absolute value.
    // Occurs e.g. for .word expressions that resolve within the same section.
    if (Kind == FK_Data_4) {
      Data[0] = static_cast<uint8_t>(Value);
      Data[1] = static_cast<uint8_t>(Value >> 8);
      Data[2] = static_cast<uint8_t>(Value >> 16);
      Data[3] = static_cast<uint8_t>(Value >> 24);
      return;
    }
  }

  // Return the relaxed opcode for a branch/call opcode, or the same
  // opcode if not relaxable.
  //
  // Single-step relaxation rule: any 2-byte form that can *only* reach its
  // target via an EXT2 sequence must relax directly to EXT2 (skipping EXT1).
  //
  // Background: the MCAssembler's relaxOnce inner loop has a MaxIter budget
  // equal to `Tail->getLayoutOrder() + 1`.  For a section with few fragments
  // (produced by -ffunction-sections or LTO's per-function .text.<name>
  // sections), MaxIter can be as low as 2.  A two-step chain
  // (e.g. CALL_i → CALL_EXT1 → CALL_EXT2) uses both iterations for relaxation
  // and leaves the final layoutSection call un-executed, so fragment offsets
  // within the section stay stale and the writeSectionData size assertion
  // fires at ELF emission time.
  //
  // Therefore CALL_i / CALL_D_i / CALL_sym / CALL_D_sym / LDW_SYM_EXT0 all
  // skip the EXT1 rung when relaxation is actually needed.  This costs 2
  // bytes on calls whose target happens to fit in sign21 but not sign8 —
  // acceptable compared to the alternative.
  //
  // CALL_EXT1 / LDW_SYM_EXT1 remain as single-step rungs for hand-written
  // assembly that starts at the 4-byte form (`ext N; call ...`).
  static unsigned getRelaxedOpcode(unsigned Op) {
    switch (Op) {
    case S1C33::JRGT:     return S1C33::JRGT_EXT1;
    case S1C33::JRGE:     return S1C33::JRGE_EXT1;
    case S1C33::JRLT:     return S1C33::JRLT_EXT1;
    case S1C33::JRLE:     return S1C33::JRLE_EXT1;
    case S1C33::JRUGT:    return S1C33::JRUGT_EXT1;
    case S1C33::JRUGE:    return S1C33::JRUGE_EXT1;
    case S1C33::JRULT:    return S1C33::JRULT_EXT1;
    case S1C33::JRULE:    return S1C33::JRULE_EXT1;
    case S1C33::JREQ:     return S1C33::JREQ_EXT1;
    case S1C33::JRNE:     return S1C33::JRNE_EXT1;
    case S1C33::JP_i:     return S1C33::JP_EXT1;
    case S1C33::JP_D_i:   return S1C33::JP_D_EXT1;
    // CALL relaxation skips EXT1 (see comment above).
    case S1C33::CALL_i:       return S1C33::CALL_EXT2;
    case S1C33::CALL_D_i:     return S1C33::CALL_D_EXT2;
    case S1C33::CALL_sym:     return S1C33::CALL_EXT2;
    case S1C33::CALL_D_sym:   return S1C33::CALL_D_EXT2;
    case S1C33::CALL_EXT1:    return S1C33::CALL_EXT2;
    case S1C33::CALL_D_EXT1:  return S1C33::CALL_D_EXT2;
    case S1C33::LDW_SYM_EXT0: return S1C33::LDW_SYM_EXT2; // direct: 2→6 bytes
    case S1C33::LDW_SYM_EXT1: return S1C33::LDW_SYM_EXT2;
    default:                  return Op;
    }
  }

  bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                         const MCSubtargetInfo &STI) const override {
    return getRelaxedOpcode(Opcode) != Opcode;
  }

  bool fixupNeedsRelaxationAdvanced(const MCFragment &F, const MCFixup &Fixup,
                                    const MCValue &Target, uint64_t Value,
                                    bool Resolved) const override {
    MCFixupKind Kind = Fixup.getKind();

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_8) {
      if (!Resolved) return true;
      int64_t Sign8 = (int64_t)Value / 2;
      return Sign8 < -128 || Sign8 > 127;
    }

    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_pc_rel_21) {
      if (F.getOpcode() != S1C33::CALL_EXT1 &&
          F.getOpcode() != S1C33::CALL_D_EXT1)
        return false;
      if (!Resolved)
        return true;
      int64_t Offset = static_cast<int64_t>(Value) - 2;
      return !isInt<21>(Offset / 2);
    }

    // abs_l controls EXT0 → EXT2 relaxation (direct, skipping EXT1).
    // EXT0 always relaxes to EXT2 in one step to avoid a two-pass chain
    // that breaks the MCAssembler relaxation loop for small sections.
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_l) {
      if (F.getOpcode() != S1C33::LDW_SYM_EXT0)
        return false;  // EXT2: no further relaxation
      const MCSymbol *Sym = Target.getAddSym();
      // Any non-absolute symbol's final address is determined by the linker.
      // The section-relative Value here is not the final address.  Always relax
      // so that the linker gets the full H/M/L triple to work with.
      if (Sym && !Sym->isAbsolute())
        return true;
      return !isInt<6>((int64_t)Value);
    }

    // abs_m controls EXT1 → EXT2 relaxation (for EXT1 emitted from assembly).
    if (Kind == (MCFixupKind)S1C33::fixup_s1c33_abs_m) {
      if (F.getOpcode() != S1C33::LDW_SYM_EXT1)
        return false;  // EXT2: no further relaxation
      const MCSymbol *Sym = Target.getAddSym();
      if (Sym && !Sym->isAbsolute())
        return true;   // Non-absolute: final address not known, use full form
      return !isInt<19>((int64_t)Value);
    }

    return false;
  }

  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override {
    Inst.setOpcode(getRelaxedOpcode(Inst.getOpcode()));
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createS1C33ELFObjectWriter(0);
  }

  // NOP = 0x0000 (all-zero is a no-operation on S1C33).
  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    if (Count % 2 != 0)
      return false;
    for (uint64_t I = 0; I < Count; I += 2)
      OS.write("\x00\x00", 2);
    return true;
  }
};

} // end anonymous namespace

namespace llvm {

MCAsmBackend *createS1C33AsmBackend(const Target &T,
                                     const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options) {
  return new S1C33AsmBackend();
}

} // namespace llvm
