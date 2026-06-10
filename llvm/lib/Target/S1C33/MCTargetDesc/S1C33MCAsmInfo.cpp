//===-- S1C33MCAsmInfo.cpp - S1C33 asm properties -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33MCAsmInfo.h"
#include "S1C33MCExpr.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void S1C33MCAsmInfo::anchor() {}

S1C33MCAsmInfo::S1C33MCAsmInfo(const Triple & /*TheTriple*/,
                               const MCTargetOptions &Options) {
  IsLittleEndian = true;
  PrivateGlobalPrefix = ".L";
  WeakRefDirective = "\t.weak\t";
  ExceptionsType = ExceptionHandling::DwarfCFI;
  UsesELFSectionDirectiveForBSS = true;
  // ';' is the comment character, compatible with pp33/ext33 assembly.
  CommentString = ";";
  SupportsDebugInformation = true;
  // Instructions are 16-bit (2 bytes) aligned
  MinInstAlignment = 2;

  // Register @l/@m/@h address specifiers for absolute address decomposition.
  // The AsmParser automatically parses "sym@l", "sym@m", "sym@h" and creates
  // MCSymbolRefExpr nodes with these specifier IDs.
  // S1C33MCCodeEmitter::getMachineOpValue maps these to fixup_s1c33_abs_l/m/h.
  static const AtSpecifier AtSpecifiers[] = {
      {S1C33::S_ABS_L, "l"},
      {S1C33::S_ABS_M, "m"},
      {S1C33::S_ABS_H, "h"},
      // 26-bit absolute split for the `ext sym@ah / ext sym@al / ld.* [%r8]`
      // pattern.  Names follow gcc33/as33 convention (historical SRF naming).
      {S1C33::S_ABS_AH, "ah"},
      {S1C33::S_ABS_AL, "al"},
  };
  initializeAtSpecifiers(AtSpecifiers);
}
