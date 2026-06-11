//===------ SemaS1C33.cpp ------ S1C33 target-specific routines -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis functions specific to S1C33.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaS1C33.h"
#include "clang/AST/DeclBase.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Sema/Attr.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"

namespace clang {

SemaS1C33::SemaS1C33(Sema &S) : SemaBase(S) {}

void SemaS1C33::handleInterruptHandlerAttr(Decl *D, const ParsedAttr &AL) {
  if (!isFuncOrMethodForAttrSubject(D)) {
    Diag(D->getLocation(), diag::warn_attribute_wrong_decl_type)
        << AL << AL.isRegularKeywordAttribute() << ExpectedFunction;
    return;
  }

  if (!AL.checkExactlyNumArgs(SemaRef, 0))
    return;

  // Interrupt handlers are entered from the trap table, not by `call`:
  // there are no arguments in R12-R15 and no way to return a value, so the
  // function must take no parameters and return void.
  if (hasFunctionProto(D) && getFunctionOrMethodNumParams(D) != 0) {
    Diag(D->getLocation(), diag::warn_interrupt_signal_attribute_invalid)
        << /*S1C33*/ 4 << /*interrupt_handler*/ 2 << 0;
    return;
  }
  if (!getFunctionOrMethodResultType(D)->isVoidType()) {
    Diag(D->getLocation(), diag::warn_interrupt_signal_attribute_invalid)
        << /*S1C33*/ 4 << /*interrupt_handler*/ 2 << 1;
    return;
  }

  handleSimpleAttribute<S1C33InterruptHandlerAttr>(*this, D, AL);
}

} // namespace clang
