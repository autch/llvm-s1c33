//===-- S1C33TargetTransformInfo.cpp - S1C33 specific TTI -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33TargetTransformInfo.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace llvm;

#define DEBUG_TYPE "s1c33tti"

//===----------------------------------------------------------------------===//
// Integer immediate costs
//
// S1C33 materialization of a 32-bit integer constant takes:
//   * 1 instruction for signed 6-bit values                    (ld.w %rd, imm6)
//   * 2 instructions for signed 19-bit values  (ext imm13 + ld.w %rd, sign6)
//   * 3 instructions otherwise                 (ext + ext + ld.w %rd, sign6)
//
// Many ALU instructions can encode an immediate directly, so when the
// constant is used *in* such an instruction no separate materialization is
// needed — the operand is "free" from the instruction's point of view.
// ConstantHoisting uses these per-instruction costs to decide whether to
// extract a shared constant to a dominator: it hoists when the per-use
// inline cost * N exceeds (hoisted materialization + N × 1).  Our cost
// model therefore reports TCC_Free for constants that actually fit inline
// and the full materialization cost when they do not.
//===----------------------------------------------------------------------===//

static InstructionCost getS1C33ImmMatCost(const APInt &Imm) {
  int64_t V = Imm.getSExtValue();
  if (isInt<6>(V))
    return 1; // ld.w %rd, imm6
  if (isInt<19>(V))
    return 2; // ext imm13 + ld.w %rd, sign6
  return 3;   // ext + ext + ld.w %rd, sign6
}

InstructionCost
S1C33TTIImpl::getIntImmCost(const APInt &Imm, Type *Ty,
                            TTI::TargetCostKind CostKind) const {
  assert(Ty->isIntegerTy() &&
         "getIntImmCost can only estimate cost of materialising integers");
  return getS1C33ImmMatCost(Imm);
}

InstructionCost S1C33TTIImpl::getIntImmCostInst(unsigned Opcode, unsigned Idx,
                                                const APInt &Imm, Type *Ty,
                                                TTI::TargetCostKind CostKind,
                                                Instruction *Inst) const {
  assert(Ty->isIntegerTy() &&
         "getIntImmCost can only estimate cost of materialising integers");

  int64_t V = Imm.getSExtValue();
  uint64_t UV = Imm.getZExtValue();

  switch (Opcode) {
  case Instruction::GetElementPtr:
    // CodeGenPrepare already splits large GEP offsets into shapes the
    // backend can fold; hoisting here would duplicate that work and add
    // register pressure.
    return TTI::TCC_Free;

  case Instruction::Add:
  case Instruction::Sub:
    // ADD/SUB take a 6-bit unsigned immediate inline; with a single ext
    // prefix the 3-operand Class 1 form absorbs up to 13-bit unsigned.
    if (isUInt<13>(UV))
      return TTI::TCC_Free;
    return getS1C33ImmMatCost(Imm);

  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
    // Logical ops take a signed 6-bit immediate inline; one ext extends to
    // signed 19-bit.  Beyond that, materialize separately.
    if (isInt<19>(V))
      return TTI::TCC_Free;
    return getS1C33ImmMatCost(Imm);

  case Instruction::ICmp:
    // cmp takes signed 6-bit inline; with one ext, signed 19-bit.
    if (isInt<19>(V))
      return TTI::TCC_Free;
    return getS1C33ImmMatCost(Imm);

  case Instruction::Mul:
    // Multiplication by a power of two lowers to a shift chain (no constant
    // needed).  Anything else requires the constant in a register (either
    // as an mlt.w operand or as an expanded multiplier input).
    if (Imm.isPowerOf2() || Imm.isNegatedPowerOf2())
      return TTI::TCC_Free;
    return getS1C33ImmMatCost(Imm);

  case Instruction::Shl:
  case Instruction::LShr:
  case Instruction::AShr:
    // Shift/rotate instructions take the amount as an imm4 (0..8), never
    // with an ext prefix (CPU Manual forbids ext on shift/rotate).  At
    // the IR level we see the amount as a constant and ISel handles it.
    if (Idx == 1)
      return TTI::TCC_Free;
    return getS1C33ImmMatCost(Imm);

  case Instruction::Load:
    // Constant pointer → materialize the full address.  No immediate
    // addressing mode takes a symbolic value.
    return getS1C33ImmMatCost(Imm);

  case Instruction::Store:
    // Either the address (Idx == 1) or the value (Idx == 0) may be constant;
    // both must be materialized — stores have no constant-operand form.
    return getS1C33ImmMatCost(Imm);

  default:
    // Be conservative for unrecognized opcodes: do not hoist.
    return TTI::TCC_Free;
  }
}

InstructionCost
S1C33TTIImpl::getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx,
                                  const APInt &Imm, Type *Ty,
                                  TTI::TargetCostKind CostKind) const {
  // No intrinsic-specific immediate folding on S1C33 yet — be conservative.
  return TTI::TCC_Free;
}
