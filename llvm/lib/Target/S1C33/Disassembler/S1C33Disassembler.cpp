//===-- S1C33Disassembler.cpp - S1C33 Disassembler ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33 MCDisassembler: decodes 16-bit instruction words into MCInst.
// All S1C33 instructions are 16-bit fixed-length, little-endian.
//
// EXT prefix handling:
//   The S1C33 'ext' instruction extends the immediate field of the next
//   instruction (up to 2 ext prefixes).  This disassembler tracks pending
//   ext values and applies them post-decode to produce correct immediates.
//
//   Single ext (ext N):
//     extended = sign_extend_19((N << width) | raw_field)
//   Double ext (ext N1 then ext N2, then instruction):
//     extended = sign_extend_32((N1 << (13+width)) | (N2 << width) | raw_field)
//
//   'width' is the immediate field width of the following instruction (6, 10).
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "TargetInfo/S1C33TargetInfo.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "s1c33-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {

class S1C33Disassembler : public MCDisassembler {
  // Pending ext values, in order of appearance (oldest = highest bits).
  // At most 2 pending ext instructions can precede a target instruction.
  mutable SmallVector<int64_t, 2> PendingExt;

public:
  S1C33Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;

private:
  // Apply pending ext values to non-PC-relative Inst, if any.
  // Emits a hex comment to CStream for the extended value.
  void applyPendingExt(MCInst &Inst, raw_ostream &CStream) const;

  // Apply pending ext values to a PC-relative branch/call instruction.
  // RawInsn is the 16-bit instruction word; the raw 8-bit displacement is
  // extracted directly from it to avoid dependence on the pre-computed
  // (wrong) operand that decodePCRelSimm8Operand already stored.
  // Emits a hex comment to CStream for the extended displacement.
  void applyPendingExtPCRel(MCInst &Inst, uint16_t RawInsn,
                             raw_ostream &CStream) const;
};

} // end anonymous namespace

static MCDisassembler *createS1C33Disassembler(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                MCContext &Ctx) {
  return new S1C33Disassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeS1C33Disassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheS1C33Target(),
                                         createS1C33Disassembler);
}

//===----------------------------------------------------------------------===//
// Operand decoder functions called by the TableGen-generated table.
//===----------------------------------------------------------------------===//

// R0–R15: HWEncoding == register index, maps directly to GR32.
static const unsigned GR32DecoderTable[] = {
    S1C33::R0,  S1C33::R1,  S1C33::R2,  S1C33::R3,
    S1C33::R4,  S1C33::R5,  S1C33::R6,  S1C33::R7,
    S1C33::R8,  S1C33::R9,  S1C33::R10, S1C33::R11,
    S1C33::R12, S1C33::R13, S1C33::R14, S1C33::R15,
};

static DecodeStatus DecodeGR32RegisterClass(MCInst &Inst, unsigned RegNo,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  if (RegNo >= 16)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GR32DecoderTable[RegNo]));
  return MCDisassembler::Success;
}

// 6-bit signed immediate (ld.w sign6, cmp sign6, not sign6).
// Sign-extends from 6 bits: range -32..31.
static DecodeStatus decodeSimm6Operand(MCInst &Inst, unsigned Val,
                                        uint64_t Address,
                                        const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend32<6>(Val)));
  return MCDisassembler::Success;
}

// 6-bit unsigned immediate (add imm6, sub imm6, sp-relative offset).
// Caller applies sign extension if ext context is present.
static DecodeStatus decodeUimm6Operand(MCInst &Inst, unsigned Val,
                                        uint64_t Address,
                                        const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(Val & 0x3F));
  return MCDisassembler::Success;
}

// 10-bit unsigned immediate (add/sub %sp, imm10).
static DecodeStatus decodeUimm10Operand(MCInst &Inst, unsigned Val,
                                         uint64_t Address,
                                         const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(Val & 0x3FF));
  return MCDisassembler::Success;
}

// 13-bit unsigned immediate (ext imm13).
static DecodeStatus decodeUimm13Operand(MCInst &Inst, unsigned Val,
                                         uint64_t Address,
                                         const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(Val & 0x1FFF));
  return MCDisassembler::Success;
}

// PC-relative 8-bit signed offset: target = Address + 2*sign8.
static DecodeStatus decodePCRelSimm8Operand(MCInst &Inst, unsigned Val,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  int64_t Sign8 = SignExtend32<8>(Val);
  int64_t Target = (int64_t)Address + 2 * Sign8;
  if (!Decoder->tryAddingSymbolicOperand(Inst, Target, Address,
                                          /*IsBranch=*/true, 0, 1, 2))
    Inst.addOperand(MCOperand::createImm(Sign8));
  return MCDisassembler::Success;
}

// TableGen-generated decoder table.
#include "S1C33GenDisassemblerTables.inc"

//===----------------------------------------------------------------------===//
// Ext-context application
//===----------------------------------------------------------------------===//

// Return the immediate field bit-width for ext-extendable instructions.
// Returns 0 for instructions that cannot be extended.
// PC-relative branch/call instructions (simm8) are handled separately by
// applyPendingExtPCRel; do NOT list them here.
static unsigned getExtImmWidth(unsigned Opcode) {
  switch (Opcode) {
  // Class 2: SP-relative memory, 6-bit offset field.
  case S1C33::LDB_sp:
  case S1C33::LDUB_sp:
  case S1C33::LDH_sp:
  case S1C33::LDUH_sp:
  case S1C33::LDW_sp:
  case S1C33::STB_sp:
  case S1C33::STH_sp:
  case S1C33::STW_sp:
  // Class 3: immediate ALU, 6-bit immediate field.
  case S1C33::ADD_ri:
  case S1C33::SUB_ri:
  case S1C33::AND_ri:
  case S1C33::OR_ri:
  case S1C33::XOR_ri:
  case S1C33::CMP_ri:
  case S1C33::MOV_ri6:
  case S1C33::NOT_ri:
    return 6;
  // Class 4 SP: 10-bit immediate.
  case S1C33::ADDSP_i:
  case S1C33::SUBSP_i:
    return 10;
  default:
    return 0;
  }
}

// Return true for instructions whose sole immediate operand is a PC-relative
// 8-bit signed displacement (simm8 / decodePCRelSimm8Operand).
// These are handled by applyPendingExtPCRel rather than applyPendingExt.
static bool isPCRelBranch(unsigned Opcode) {
  switch (Opcode) {
  case S1C33::JP_i:
  case S1C33::JP_D_i:
  case S1C33::CALL_i:
  case S1C33::JRGT:   case S1C33::JRGE:   case S1C33::JRLT:   case S1C33::JRLE:
  case S1C33::JRUGT:  case S1C33::JRUGE:  case S1C33::JRULT:  case S1C33::JRULE:
  case S1C33::JREQ:   case S1C33::JRNE:
  case S1C33::JRGT_D: case S1C33::JRGE_D: case S1C33::JRLT_D: case S1C33::JRLE_D:
  case S1C33::JRUGT_D:case S1C33::JRUGE_D:case S1C33::JRULT_D:case S1C33::JRULE_D:
  case S1C33::JREQ_D: case S1C33::JRNE_D:
    return true;
  default:
    return false;
  }
}

// Format Extended as a signed hex comment for CStream.
// Positive: "# 0xNN", negative: "# -0xNN".
static void emitExtComment(int64_t Extended, raw_ostream &CStream) {
  if (Extended < 0)
    CStream << format("# -0x%" PRIx64, (uint64_t)-Extended);
  else
    CStream << format("# 0x%" PRIx64, (uint64_t)Extended);
}

void S1C33Disassembler::applyPendingExt(MCInst &Inst,
                                         raw_ostream &CStream) const {
  if (PendingExt.empty())
    return;

  unsigned Width = getExtImmWidth(Inst.getOpcode());
  if (Width == 0) {
    // This instruction is not extendable; the ext values are lost.
    // (Should not happen in well-formed code.)
    PendingExt.clear();
    return;
  }

  // Find the single immediate operand in the instruction.
  int ImmIdx = -1;
  for (unsigned I = 0; I < Inst.getNumOperands(); ++I) {
    if (Inst.getOperand(I).isImm()) {
      ImmIdx = (int)I;
      break;
    }
  }
  if (ImmIdx < 0) {
    PendingExt.clear();
    return;
  }

  // Recover the raw immediate field bits (undo any sign extension).
  uint64_t RawField = (uint64_t)Inst.getOperand(ImmIdx).getImm() &
                      ((1ULL << Width) - 1);

  int64_t Extended;
  if (PendingExt.size() == 1) {
    // Single ext: sign_extend_19((ext_val << Width) | raw)
    uint64_t Combined = ((uint64_t)PendingExt[0] << Width) | RawField;
    Extended = SignExtend64(Combined, 13 + Width);
  } else {
    // Double ext: (ext0 << (13+Width)) | (ext1 << Width) | raw
    // ext0 = PendingExt[0] (first/older = provides highest bits)
    // ext1 = PendingExt[1] (second/newer = provides middle bits)
    unsigned TotalBits = 13 + 13 + Width;
    uint64_t Combined = ((uint64_t)PendingExt[0] << (13 + Width)) |
                        ((uint64_t)PendingExt[1] << Width) | RawField;
    Extended = SignExtend64(Combined, TotalBits);
  }

  // Leave the operand as decoded (raw sign-extended field value).
  // The extended result is shown only in the comment.
  PendingExt.clear();
  emitExtComment(Extended, CStream);
}

void S1C33Disassembler::applyPendingExtPCRel(MCInst &Inst, uint16_t RawInsn,
                                              raw_ostream &CStream) const {
  // The PC-relative displacement lives in bits[7:0] of the instruction word.
  // decodePCRelSimm8Operand already ran with the unextended raw value and may
  // have stored a wrong symbol expression or wrong signed displacement; we
  // discard it and recompute from the raw bits.
  uint8_t RawDisp = RawInsn & 0xFF;

  int64_t Extended;
  if (PendingExt.size() == 1) {
    // Single ext: sign_extend_21((ext_val << 8) | raw)
    uint64_t Combined = ((uint64_t)PendingExt[0] << 8) | RawDisp;
    Extended = SignExtend64(Combined, 13 + 8);
  } else {
    // Double ext: sign_extend_34((ext0 << 21) | (ext1 << 8) | raw)
    uint64_t Combined = ((uint64_t)PendingExt[0] << (13 + 8)) |
                        ((uint64_t)PendingExt[1] << 8) | RawDisp;
    Extended = SignExtend64(Combined, 13 + 13 + 8);
  }
  PendingExt.clear();

  // Update the MCInst's immediate operand with the full extended displacement
  // (halfword-unit signed offset) so that MCInstrAnalysis::evaluateBranch()
  // can compute the correct target for ext-prefixed branches.
  // decodePCRelSimm8Operand may have stored Sign8 or a symbolic operand.
  // We only replace when an immediate is present; if a symbolizer was active
  // and stored a symbolic operand instead, there is nothing to update here.
  for (unsigned I = 0; I < Inst.getNumOperands(); ++I) {
    if (Inst.getOperand(I).isImm()) {
      Inst.getOperand(I).setImm(Extended);
      break;
    }
  }

  emitExtComment(Extended, CStream);
}

//===----------------------------------------------------------------------===//
// Main decode entry point
//===----------------------------------------------------------------------===//

DecodeStatus S1C33Disassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CStream) const {
  if (Bytes.size() < 2) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  // Read 16-bit little-endian instruction word.
  uint16_t Insn = (uint16_t)Bytes[0] | ((uint16_t)Bytes[1] << 8);

  DecodeStatus Result = decodeInstruction(DecoderTable16, Instr,
                                           Insn, Address, this, STI);
  if (Result == MCDisassembler::Fail) {
    PendingExt.clear();
    Size = 2;
    return MCDisassembler::Fail;
  }

  Size = 2;

  // If this is an EXT instruction, accumulate the ext value for the next
  // instruction.  Up to 2 ext prefixes are supported.
  if (Instr.getOpcode() == S1C33::EXT) {
    if (PendingExt.size() < 2)
      PendingExt.push_back(Instr.getOperand(0).getImm());
    // Return the EXT instruction as-is for display.
    return Result;
  }

  // Apply any pending ext values to extendable immediates.
  if (!PendingExt.empty() && isPCRelBranch(Instr.getOpcode()))
    applyPendingExtPCRel(Instr, Insn, CStream);
  else
    applyPendingExt(Instr, CStream);
  return Result;
}
