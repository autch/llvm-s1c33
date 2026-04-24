//===-- S1C33FixupKinds.h - S1C33 Fixup Kinds -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33FIXUPKINDS_H
#define LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace S1C33 {

enum Fixups {
  // 8-bit PC-relative offset for call/branch instructions (Class 0 Rel format).
  // S1C33 encoding: target = PC + 2*sign8  (PC = instruction's own address)
  //   sign8 = (target - fixup_addr) / 2
  // Field: bits[7:0] of the 16-bit instruction (LE byte 0).
  fixup_s1c33_pc_rel_8 = FirstTargetFixupKind,

  // 21-bit PC-relative fixup for the 4-byte relaxed ext+branch/call sequence.
  // Covers both the ext imm13 field and the following branch sign8 field.
  // S1C33 encoding (ext at fixup_addr, branch at fixup_addr+2):
  //   target = (fixup_addr + 2) + 2 * extended_imm
  //   extended_imm = sign_extend_21((imm13 << 8) | sign8_raw)
  //   extended_imm = (target - fixup_addr - 2) / 2
  //   imm13    = (extended_imm >> 8) & 0x1FFF  → ext word bits[12:0]
  //   sign8_raw = extended_imm & 0xFF           → branch word bits[7:0]
  fixup_s1c33_pc_rel_21,

  // Split 32-bit PC-relative fixups for ext+ext+branch/call.
  // The three relocations are all PC-relative to the instruction they patch:
  //   fixup_s1c33_pc_rel_h: ext_h at P_H, encodes bits[31:22] of target-call
  //   fixup_s1c33_pc_rel_m: ext_m at P_M=P_H+2, encodes bits[21:9]
  //   fixup_s1c33_pc_rel_l: call/jp at P_L=P_H+4, encodes bits[8:1]
  // where call_addr = P_H + 4.
  fixup_s1c33_pc_rel_h,
  fixup_s1c33_pc_rel_m,
  fixup_s1c33_pc_rel_l,

  // Absolute address fixups for the 3-level LDW_SYM_EXT0/EXT1/EXT2 sequence.
  // Each targets the corresponding ext or ld.w instruction word.
  fixup_s1c33_abs_h, // bits[31:19] of absolute addr → ext_h imm13 field
  fixup_s1c33_abs_m, // bits[18:6] of absolute addr → ext_m imm13 field
  fixup_s1c33_abs_l, // bits[5:0] of absolute addr → ld.w imm6 field

  // 26-bit absolute address fixups for the ext+ext+ld.*[%r8] pattern used to
  // access globals through the kernel's known-zero R8.  The address is split
  // into two 13-bit halves; there is no sign6 tail since Class 1 memory ops
  // have no immediate field.  Compatible with gcc33's @ah/@al syntax and the
  // R_S1C33_REL_AH/REL_AL ELF relocations (name is historical; semantics are
  // absolute).
  fixup_s1c33_abs_ah, // bits[25:13] of absolute addr → ext_hi imm13 field
  fixup_s1c33_abs_al, // bits[12:0] of absolute addr → ext_lo imm13 field

  // Marker.
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace S1C33
} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33FIXUPKINDS_H
