//===--- PIECE.h - Aquaplus P/ECE ToolChain ---------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PIECE_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PIECE_H

#include "BareMetal.h"

namespace clang {
namespace driver {

namespace toolchains {

/// Toolchain for the Aquaplus P/ECE handheld (s1c33-*-piece): an S1C33209
/// running the P/ECE kernel.  Differs from generic bare-metal s1c33 in that
/// applications are kernel callbacks linked against the P/ECE SDK runtime
/// (crt0.o/crti.o, libpceapi, the piece.ld linker script), and in that the
/// kernel guarantees R8 == 0 (see FeatureR8AbsGlobal in the backend).
class LLVM_LIBRARY_VISIBILITY PIECEToolChain : public BareMetal {
public:
  PIECEToolChain(const Driver &D, const llvm::Triple &Triple,
                 const llvm::opt::ArgList &Args)
      : BareMetal(D, Triple, Args) {}

protected:
  Tool *buildLinker() const override;
};

} // namespace toolchains

namespace tools {
namespace piece {

class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("piece::Linker", "ld.lld", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // namespace piece
} // namespace tools

} // namespace driver
} // namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PIECE_H
