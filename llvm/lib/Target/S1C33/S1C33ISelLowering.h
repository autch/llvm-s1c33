//===-- S1C33ISelLowering.h - S1C33 DAG Lowering Interface ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_S1C33_S1C33ISELLOWERING_H
#define LLVM_LIB_TARGET_S1C33_S1C33ISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class S1C33Subtarget;
class S1C33TargetMachine;

namespace S1C33ISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  // Return from subroutine.
  RET_FLAG,
  // Return from interrupt handler (reti).
  RETI_FLAG,
  // Call — emitted by LowerCall; matched to CALL_sym or CALL_r in isel.
  CALL,
  // Compare: produces a Glue output consumed by BRCOND / SELECT_CC.
  CMP,
  // Conditional branch: (chain) = BRCOND(chain, dest, S1C33CC, Glue).
  BRCOND,
  // Conditional select (expanded via EmitInstrWithCustomInserter):
  // (i32) = SELECT_CC(LHS, RHS, TrueVal, FalseVal, S1C33CC).
  SELECT_CC,
  // Wrapper — wraps TargetGlobalAddress/TargetExternalSymbol so that isel
  // can match them as value-producing nodes (address materialization).
  // Without this, TargetGlobalAddress is a leaf that cannot satisfy CopyToReg.
  Wrapper,
};
} // namespace S1C33ISD

// S1C33-specific condition codes used in BRCOND / SELECT_CC nodes.
// These must match the numeric values in S1C33InstrInfo.td branch patterns.
namespace S1C33CC {
enum CondCode : unsigned {
  EQ  = 0,
  NE  = 1,
  LT  = 2,
  LE  = 3,
  GT  = 4,
  GE  = 5,
  ULT = 6,
  ULE = 7,
  UGT = 8,
  UGE = 9,
};
} // namespace S1C33CC

class S1C33TargetLowering : public TargetLowering {
  const S1C33Subtarget &Subtarget;

public:
  explicit S1C33TargetLowering(const TargetMachine &TM,
                                const S1C33Subtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                                bool IsVarArg,
                                const SmallVectorImpl<ISD::InputArg> &Ins,
                                const SDLoc &DL, SelectionDAG &DAG,
                                SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                       const SmallVectorImpl<ISD::OutputArg> &Outs,
                       const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                       SelectionDAG &DAG) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                     SmallVectorImpl<SDValue> &InVals) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                               MachineBasicBlock *BB) const override;

private:
  MachineBasicBlock *emitVariableShift(MachineInstr &MI,
                                       MachineBasicBlock *BB) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSETCC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSHL_PARTS(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSRL_PARTS(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSRA_PARTS(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVACOPY(SDValue Op, SelectionDAG &DAG) const;

  SDValue combineBRCC(SDNode *N, SelectionDAG &DAG) const;
  SDValue combineSETCC(SDNode *N, SelectionDAG &DAG) const;

public:
  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM,
                              Type *Ty, unsigned AS,
                              Instruction *I = nullptr) const override;
  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                StringRef Constraint, MVT VT) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_S1C33_S1C33ISELLOWERING_H
