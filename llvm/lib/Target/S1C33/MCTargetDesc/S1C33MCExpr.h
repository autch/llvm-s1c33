//===-- S1C33MCExpr.h - S1C33 symbol specifier IDs ---------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33 address specifier kinds for the @l/@m/@h modifiers.
//
// These are registered in S1C33MCAsmInfo via initializeAtSpecifiers so that the
// MC AsmParser automatically handles "sym@l", "sym@m", "sym@h" and creates
// MCSymbolRefExpr nodes with the corresponding specifier.
//
// The specifiers are used in the LDW_SYM_EXT* address-materialization sequence:
//   ext  sym@h       -> fixup_s1c33_abs_h (bits[31:19] → ext imm13)
//   ext  sym@m       -> fixup_s1c33_abs_m (bits[18:6]  → ext imm13)
//   ld.w %rd, sym@l  -> fixup_s1c33_abs_l (bits[5:0]   → ld.w sign6)
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCEXPR_H
#define LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCEXPR_H

#include <cstdint>

namespace llvm {
namespace S1C33 {

/// Specifier kinds for @l/@m/@h and @ah/@al address modifiers.
/// These are stored in MCSymbolRefExpr::getSpecifier().
/// Values must be >= MCSymbolRefExpr::FirstTargetSpecifier (= 4).
enum Specifier : uint32_t {
  S_ABS_L = 4, ///< sym@l  — bits[5:0],   sign6 field of ld.w
  S_ABS_M = 5, ///< sym@m  — bits[18:6],  imm13 field of ext
  S_ABS_H = 6, ///< sym@h  — bits[31:19], imm13 field of ext
  // 26-bit absolute split, used for `ext sym@ah / ext sym@al / ld.* [%r8]`.
  // Compatible with gcc33's syntax.  'ah/al' is historically named for SRF
  // REL_AH/REL_AL relocations, but the effective semantics is absolute.
  S_ABS_AH = 7, ///< sym@ah — bits[25:13], imm13 field of ext (hi)
  S_ABS_AL = 8, ///< sym@al — bits[12:0],  imm13 field of ext (lo)
};

} // namespace S1C33
} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_MCTARGETDESC_S1C33MCEXPR_H
