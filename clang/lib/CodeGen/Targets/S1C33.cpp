//===- S1C33.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// S1C33 ABI Implementation
//
// Mostly the default ABI, with one S5U1C33000C (gcc33) quirk for arguments:
// a "single-element struct" that fits in 32 bits is passed in an argument
// register, exactly as its scalar element would be — NOT on the stack like
// every other struct (DESIGN_SPEC §3.5).  An 8- or 16-bit single-element
// struct passed this way has its value packed into the HIGH bits of the
// register (a leftover from gcc33's MIPS big-endian origin); that high-bit
// packing is applied by the S1C33 backend.  Such a coerced argument is
// marked `inreg` so the backend can tell it apart from an ordinary i8/i16
// argument (which never carries `inreg`).  Multi-element structs, unions,
// and anything larger than 32 bits stay entirely on the stack.
//===----------------------------------------------------------------------===//

namespace {

class S1C33ABIInfo : public DefaultABIInfo {
public:
  S1C33ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyArgumentType(QualType Ty) const {
    if (isAggregateTypeForABI(Ty)) {
      // C++ records with non-trivial copy ctors / dtors must be indirect.
      if (CGCXXABI::RecordArgABI RAA = getRecordArgABI(Ty, getCXXABI()))
        return getNaturalAlignIndirect(Ty,
                                       getDataLayout().getAllocaAddrSpace(),
                                       RAA == CGCXXABI::RAA_DirectInMemory);

      // gcc33 quirk: a single-element struct whose element is an integer,
      // pointer, or enum and whose total size is exactly 8, 16, or 32 bits is
      // passed in a register like that scalar.  Coerce it to an integer of
      // the matching width so it flows through the normal argument registers,
      // and mark it `inreg` so the backend can recognise it (and high-bit-pack
      // the 8/16-bit forms) without mistaking an ordinary i8/i16 argument for
      // a coerced struct.
      if (const Type *Elt = isSingleElementStruct(Ty, getContext())) {
        if (Elt->isIntegerType() || Elt->isEnumeralType() ||
            Elt->isPointerType()) {
          uint64_t Size = getContext().getTypeSize(Ty);
          if (Size == 8 || Size == 16 || Size == 32)
            return ABIArgInfo::getDirectInReg(
                llvm::IntegerType::get(getVMContext(), Size));
        }
      }

      // Every other struct / union: passed entirely on the stack.
      return getNaturalAlignIndirect(Ty,
                                     getDataLayout().getAllocaAddrSpace());
    }

    return DefaultABIInfo::classifyArgumentType(Ty);
  }

  // DefaultABIInfo::classify{Return,Argument}Type() are not virtual, so
  // computeInfo must dispatch to our classifyArgumentType explicitly.
  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() =
          DefaultABIInfo::classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override {
    return CGF.EmitLoadOfAnyValue(
        CGF.MakeAddrLValue(
            EmitVAArgInstr(CGF, VAListAddr, Ty, classifyArgumentType(Ty)), Ty),
        Slot);
  }
};

class S1C33TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  S1C33TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<S1C33ABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override {
    if (GV->isDeclaration())
      return;
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    if (!FD)
      return;
    auto *Fn = cast<llvm::Function>(GV);

    // __attribute__((interrupt_handler)) → mark with "interrupt_handler" so
    // S1C33FrameLowering generates pushn %r15 / popn %r15 / reti.
    if (FD->getAttr<S1C33InterruptHandlerAttr>())
      Fn->addFnAttr("interrupt_handler");
  }
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createS1C33TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<S1C33TargetCodeGenInfo>(CGM.getTypes());
}
