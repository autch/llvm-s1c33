//===--- S1C33.h - Declare S1C33 target feature support ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares S1C33 TargetInfo objects.
// Target: EPSON S1C33000 32-bit RISC (as used in P/ECE: S1C33209).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_S1C33_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_S1C33_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY S1C33TargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];

public:
  S1C33TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // S1C33 ABI (S5U1C33000C):
    //   char=8, short=16, int=32, long=32, long long=64
    //   float=32, double=64
    //   pointer=32 (hardware address space is 28-bit, but LLVM uses 32-bit)
    //   little-endian, no hardware divide, optional hardware multiply
    //
    // DataLayout must match S1C33TargetMachine (Triple::computeDataLayout):
    //   "e-m:e-p:32:32-i1:8-i8:8-i16:16-i32:32-i64:32-f32:32-f64:32-n32-S32"
    TLSSupported = false;
    LongWidth = LongAlign = 32;
    LongLongWidth = 64;
    LongLongAlign = 32;    // 64-bit values only need 32-bit alignment on S1C33
    PointerWidth = PointerAlign = 32;
    SuitableAlign = 32;
    SizeType = UnsignedInt;
    PtrDiffType = SignedInt;
    IntPtrType = SignedInt;
    IntMaxType = SignedLongLong;
    Int64Type = SignedLongLong;
    SigAtomicType = SignedInt;
    DoubleAlign = LongDoubleAlign = 32; // double is 32-bit aligned on S1C33
    LongDoubleWidth = 64;
    LongDoubleFormat = &llvm::APFloat::IEEEdouble();
    resetDataLayout("e-m:e-p:32:32-i1:8-i8:8-i16:16-i32:32-i64:32"
                    "-f32:32-f64:32-n32-S32");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "s1c33";
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    // No register aliases needed beyond the canonical r0-r15 names.
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    return false;
  }

  std::string_view getClobbers() const override { return ""; }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }

  bool hasBitIntType() const override { return true; }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_S1C33_H
