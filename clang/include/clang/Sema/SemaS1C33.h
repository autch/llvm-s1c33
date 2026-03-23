//===--- SemaS1C33.h ------- S1C33 target-specific routines ---*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file declares semantic analysis functions specific to S1C33.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMAS1C33_H
#define LLVM_CLANG_SEMA_SEMAS1C33_H

#include "clang/AST/ASTFwd.h"
#include "clang/Sema/SemaBase.h"

namespace clang {
class ParsedAttr;

class SemaS1C33 : public SemaBase {
public:
  SemaS1C33(Sema &S);

  void handleInterruptHandlerAttr(Decl *D, const ParsedAttr &AL);
};

} // namespace clang

#endif // LLVM_CLANG_SEMA_SEMAS1C33_H
