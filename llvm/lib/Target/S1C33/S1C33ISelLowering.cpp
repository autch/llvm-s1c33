//===-- S1C33ISelLowering.cpp - S1C33 DAG Lowering ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "S1C33ISelLowering.h"
#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33InstrInfo.h"
#include "S1C33MachineFunctionInfo.h"
#include "S1C33Subtarget.h"
#include "S1C33TargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/IR/RuntimeLibcalls.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

// CC_S1C33_AssignDoubleHalf — custom handler for the lo half of an i64/f64 pair.
//
// gcc33 rule: both halves of a 64-bit argument must go into two consecutive
// argument registers (R12–R15), or both must go to stack.  Splitting between
// one register and one stack slot is NOT allowed.
//
// This function is invoked by CCIfSplit in CC_S1C33 (only for the lo half,
// i.e., ArgFlags.isSplit() == true).  Return convention for CCCustom:
//   true  → assignment made here (stop processing this value)
//   false → not handled; fall through to the next CC rule
//
// When ≥2 argument registers remain: return false so CCAssignToReg below
// allocates the lo half normally; the hi half will then get the next register.
//
// When <2 argument registers remain: exhaust all remaining argument registers
// (so the hi half also misses CCAssignToReg and falls to CCAssignToStack),
// assign the lo half to stack, and return true.
static bool CC_S1C33_AssignDoubleHalf(unsigned ValNo, MVT ValVT, MVT LocVT,
                                       CCValAssign::LocInfo LocInfo,
                                       ISD::ArgFlagsTy ArgFlags,
                                       CCState &State) {
  static const MCPhysReg ArgRegs[] = {S1C33::R12, S1C33::R13,
                                       S1C33::R14, S1C33::R15};
  // Index of the first unallocated register (4 = all allocated).
  unsigned First = State.getFirstUnallocated(ArgRegs);
  // Need at least two consecutive regs (First and First+1).
  if (First + 1 < std::size(ArgRegs)) {
    // Pair available — let the normal CCAssignToReg rule handle lo.
    return false; // not handled, fall through
  }
  // Fewer than 2 registers remain.  Exhaust any leftover register so that
  // the hi half also ends up on the stack via CCAssignToStack.
  for (MCPhysReg R : ArgRegs)
    if (!State.isAllocated(R))
      State.AllocateReg(R);
  // Assign lo half to stack.
  unsigned Offset = State.AllocateStack(4, Align(4));
  State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
  return true; // handled
}

// Include the TableGen-generated calling convention tables.
#include "S1C33GenCallingConv.inc"

S1C33TargetLowering::S1C33TargetLowering(const TargetMachine &TM,
                                           const S1C33Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  addRegisterClass(MVT::i32, &S1C33::GR32RegClass);

  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(S1C33::SP);

  // GlobalAddress and ExternalSymbol must be lowered to target nodes so that
  // isel patterns (tglobaladdr, texternalsym) can match them.
  setOperationAction(ISD::GlobalAddress,  MVT::i32, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i32, Custom);
  // ConstantPool addresses (e.g. De Bruijn tables from CTTZ expansion) must be
  // lowered like GlobalAddress — materialize the address via ext+ld.w sequences.
  setOperationAction(ISD::ConstantPool,   MVT::i32, Custom);

  // S1C33 is not in the generated RuntimeLibcallsImpl target table
  // (setTargetRuntimeLibcallSets has no S1C33 entry), so AvailableLibcallImpls
  // remains empty and getLibcallName() returns nullptr for every RTLIB entry.
  // We must explicitly register every libcall implementation we want to use.

  // Memory operations — implemented in P/ECE SDK string.lib.
  setLibcallImpl(RTLIB::MEMCPY,  RTLIB::impl_memcpy);
  setLibcallImpl(RTLIB::MEMMOVE, RTLIB::impl_memmove);
  setLibcallImpl(RTLIB::MEMSET,  RTLIB::impl_memset);

  // Integer division/remainder — 32-bit: P/ECE SDK idiv.lib.
  setLibcallImpl(RTLIB::SDIV_I32,  RTLIB::impl___divsi3);
  setLibcallImpl(RTLIB::UDIV_I32,  RTLIB::impl___udivsi3);
  setLibcallImpl(RTLIB::SREM_I32,  RTLIB::impl___modsi3);
  setLibcallImpl(RTLIB::UREM_I32,  RTLIB::impl___umodsi3);
  // 64-bit integer arithmetic — compiler-rt builtins.
  setLibcallImpl(RTLIB::MUL_I64,  RTLIB::impl___muldi3);
  setLibcallImpl(RTLIB::SDIV_I64, RTLIB::impl___divdi3);
  setLibcallImpl(RTLIB::UDIV_I64, RTLIB::impl___udivdi3);
  setLibcallImpl(RTLIB::SREM_I64, RTLIB::impl___moddi3);
  setLibcallImpl(RTLIB::UREM_I64, RTLIB::impl___umoddi3);
  setLibcallImpl(RTLIB::SHL_I64,  RTLIB::impl___ashldi3);
  setLibcallImpl(RTLIB::SRL_I64,  RTLIB::impl___lshrdi3);
  setLibcallImpl(RTLIB::SRA_I64,  RTLIB::impl___ashrdi3);

  // Floating-point arithmetic — implemented in P/ECE SDK fp.lib.
  setLibcallImpl(RTLIB::ADD_F32,  RTLIB::impl___addsf3);
  setLibcallImpl(RTLIB::SUB_F32,  RTLIB::impl___subsf3);
  setLibcallImpl(RTLIB::MUL_F32,  RTLIB::impl___mulsf3);
  setLibcallImpl(RTLIB::DIV_F32,  RTLIB::impl___divsf3);
  setLibcallImpl(RTLIB::ADD_F64,  RTLIB::impl___adddf3);
  setLibcallImpl(RTLIB::SUB_F64,  RTLIB::impl___subdf3);
  setLibcallImpl(RTLIB::MUL_F64,  RTLIB::impl___muldf3);
  setLibcallImpl(RTLIB::DIV_F64,  RTLIB::impl___divdf3);

  // Floating-point conversions — fp.lib.
  setLibcallImpl(RTLIB::FPEXT_F32_F64,    RTLIB::impl___extendsfdf2);
  setLibcallImpl(RTLIB::FPROUND_F64_F32,  RTLIB::impl___truncdfsf2);
  setLibcallImpl(RTLIB::FPTOSINT_F32_I32, RTLIB::impl___fixsfsi);
  setLibcallImpl(RTLIB::FPTOSINT_F64_I32, RTLIB::impl___fixdfsi);
  setLibcallImpl(RTLIB::FPTOUINT_F32_I32, RTLIB::impl___fixunssfsi);
  setLibcallImpl(RTLIB::FPTOUINT_F64_I32, RTLIB::impl___fixunsdfsi);
  setLibcallImpl(RTLIB::SINTTOFP_I32_F32, RTLIB::impl___floatsisf);
  setLibcallImpl(RTLIB::SINTTOFP_I32_F64, RTLIB::impl___floatsidf);
  setLibcallImpl(RTLIB::UINTTOFP_I32_F32, RTLIB::impl___floatunsisf);
  setLibcallImpl(RTLIB::UINTTOFP_I32_F64, RTLIB::impl___floatunsidf);

  // Floating-point comparisons — fp.lib (__fcmps/__fcmpd wrappers).
  // LLVM uses __eqsf2 / __unorddf2 etc. as the canonical comparison libcalls.
  setLibcallImpl(RTLIB::OEQ_F32, RTLIB::impl___eqsf2);
  setLibcallImpl(RTLIB::UNE_F32, RTLIB::impl___nesf2);
  setLibcallImpl(RTLIB::OLT_F32, RTLIB::impl___ltsf2);
  setLibcallImpl(RTLIB::OLE_F32, RTLIB::impl___lesf2);
  setLibcallImpl(RTLIB::OGT_F32, RTLIB::impl___gtsf2);
  setLibcallImpl(RTLIB::OGE_F32, RTLIB::impl___gesf2);
  setLibcallImpl(RTLIB::UO_F32,  RTLIB::impl___unordsf2);
  setLibcallImpl(RTLIB::OEQ_F64, RTLIB::impl___eqdf2);
  setLibcallImpl(RTLIB::UNE_F64, RTLIB::impl___nedf2);
  setLibcallImpl(RTLIB::OLT_F64, RTLIB::impl___ltdf2);
  setLibcallImpl(RTLIB::OLE_F64, RTLIB::impl___ledf2);
  setLibcallImpl(RTLIB::OGT_F64, RTLIB::impl___gtdf2);
  setLibcallImpl(RTLIB::OGE_F64, RTLIB::impl___gedf2);
  setLibcallImpl(RTLIB::UO_F64,  RTLIB::impl___unorddf2);
  setOperationAction(ISD::SDIV,    MVT::i32, Expand);
  setOperationAction(ISD::UDIV,    MVT::i32, Expand);
  setOperationAction(ISD::SREM,    MVT::i32, Expand);
  setOperationAction(ISD::UREM,    MVT::i32, Expand);
  // Prevent the DAG combiner from forming combined SDIVREM/UDIVREM nodes;
  // split them back into SDIV+SREM pairs so each expands to its own libcall.
  setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

  // Integer multiplication:
  //   With HWMul (+hwmul): ISD::MUL/MULHS/MULHU are Legal (MUL_r/MULHS_r/MULHU_r Pat<>)
  //   Without HWMul:       ISD::MUL expands to __mulsi3 libcall; MULHS/MULHU expand
  // SMUL_LOHI/UMUL_LOHI are always Expand: we don't want the DAGCombiner to
  // produce them for plain i32 multiply — our mlt.w pattern handles MUL directly.
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
  if (!STI.hasHWMul()) {
    setLibcallImpl(RTLIB::MUL_I32, RTLIB::impl___mulsi3);
    setOperationAction(ISD::MUL,   MVT::i32, Expand);
    setOperationAction(ISD::MULHS, MVT::i32, Expand);
    setOperationAction(ISD::MULHU, MVT::i32, Expand);
  }

  // S1C33 has ld.b (sign-extend byte) and ld.h (sign-extend halfword)
  // register-to-register instructions.  Mark Legal so ISel matches the
  // LDB_rr / LDH_rr patterns (1 instruction) instead of expanding to
  // shift pairs (6 instructions for i8, 4 for i16).
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1,  Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8,  Legal);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Legal);

  // S1C33 has no byte-swap instruction.  SWAP swaps halfwords (rotate-16),
  // not a full byte reversal.  Expand to shift/mask sequence.
  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  // MIRROR instruction does full 32-bit bit reversal → Legal.
  setOperationAction(ISD::BITREVERSE, MVT::i32, Legal);

  // S1C33 has no efficient jump table support (no scaled-index addressing).
  // Disable jump tables entirely so switch statements become if-else chains.
  setMinimumJumpTableEntries(UINT_MAX);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);

  // Conditional branches: lower BR_CC to S1C33ISD::CMP + S1C33ISD::BRCOND.
  setOperationAction(ISD::BR_CC,    MVT::i32,   Custom);
  // SELECT: no conditional move on S1C33; lower to SELECT_CC(cond, 0, T, F, NE)
  setOperationAction(ISD::SELECT,    MVT::i32,   Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i32,   Custom);
  // BRCOND requires a boolean condition — expand to BR_CC instead.
  setOperationAction(ISD::BRCOND,   MVT::Other, Expand);
  // SETCC produces a boolean i32 (0 or 1) from a comparison.
  // Lower to SELECT_CC(lhs, rhs, 1, 0, cc) which expands via our SELECT pseudo.
  setOperationAction(ISD::SETCC,    MVT::i32,   Custom);

  // Large constant shifts (> 8) are expanded in ISelDAGToDAG::Select()
  // into chains of machine shift instructions (each ≤ 8), bypassing the
  // generic DAG combiner which would re-merge them.

  // Variadic function support.
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG,   MVT::Other, Expand);
  setOperationAction(ISD::VACOPY,  MVT::Other, Custom);
  setOperationAction(ISD::VAEND,   MVT::Other, Expand);

  // S1C33 has no count-leading/trailing zeros or popcount instructions.
  // Expand to software sequences.
  setOperationAction(ISD::CTTZ,  MVT::i32, Expand);
  setOperationAction(ISD::CTLZ,  MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i32, Expand);

  // 64-bit shift parts: lower SHL_PARTS/SRL_PARTS/SRA_PARTS to sequences of
  // 32-bit operations plus a SELECT_CC to handle the >= 32 case.
  setOperationAction(ISD::SHL_PARTS, MVT::i32, Custom);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Custom);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Custom);

  // Register DAG combines:
  // - BR_CC, SETCC: sign-bit test optimization (sext_inreg+cmp → and+cmp)
  // - SRL: signed-division-by-power-of-2 bias (shift chain → select_cc)
  setTargetDAGCombine({ISD::BR_CC, ISD::SETCC, ISD::SRL, ISD::AND});
}

const char *S1C33TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case S1C33ISD::RET_FLAG:  return "S1C33ISD::RET_FLAG";
  case S1C33ISD::RETI_FLAG: return "S1C33ISD::RETI_FLAG";
  case S1C33ISD::CALL:      return "S1C33ISD::CALL";
  case S1C33ISD::CMP:       return "S1C33ISD::CMP";
  case S1C33ISD::BRCOND:    return "S1C33ISD::BRCOND";
  case S1C33ISD::SELECT_CC: return "S1C33ISD::SELECT_CC";
  case S1C33ISD::Wrapper:   return "S1C33ISD::Wrapper";
  default:                   return nullptr;
  }
}

//===----------------------------------------------------------------------===//
// Helper: ISD::CondCode → S1C33CC
//===----------------------------------------------------------------------===//

static S1C33CC::CondCode convertCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:  return S1C33CC::EQ;
  case ISD::SETNE:  return S1C33CC::NE;
  case ISD::SETLT:  return S1C33CC::LT;
  case ISD::SETLE:  return S1C33CC::LE;
  case ISD::SETGT:  return S1C33CC::GT;
  case ISD::SETGE:  return S1C33CC::GE;
  case ISD::SETULT: return S1C33CC::ULT;
  case ISD::SETULE: return S1C33CC::ULE;
  case ISD::SETUGT: return S1C33CC::UGT;
  case ISD::SETUGE: return S1C33CC::UGE;
  default:
    llvm_unreachable("Unsupported condition code for S1C33");
  }
}

static unsigned getBranchOpcode(S1C33CC::CondCode CC) {
  switch (CC) {
  case S1C33CC::EQ:  return S1C33::JREQ;
  case S1C33CC::NE:  return S1C33::JRNE;
  case S1C33CC::LT:  return S1C33::JRLT;
  case S1C33CC::LE:  return S1C33::JRLE;
  case S1C33CC::GT:  return S1C33::JRGT;
  case S1C33CC::GE:  return S1C33::JRGE;
  case S1C33CC::ULT: return S1C33::JRULT;
  case S1C33CC::ULE: return S1C33::JRULE;
  case S1C33CC::UGT: return S1C33::JRUGT;
  case S1C33CC::UGE: return S1C33::JRUGE;
  }
  llvm_unreachable("Unknown S1C33CC");
}

//===----------------------------------------------------------------------===//
// Custom node lowering
//===----------------------------------------------------------------------===//

SDValue S1C33TargetLowering::LowerOperation(SDValue Op,
                                              SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    llvm_unreachable("Unimplemented custom lowering");

  case ISD::BR_CC:    return LowerBR_CC(Op, DAG);
  case ISD::SELECT:   return LowerSELECT(Op, DAG);
  case ISD::SELECT_CC: return LowerSELECT_CC(Op, DAG);
  case ISD::SETCC:    return LowerSETCC(Op, DAG);
  case ISD::SHL_PARTS: return LowerSHL_PARTS(Op, DAG);
  case ISD::SRL_PARTS: return LowerSRL_PARTS(Op, DAG);
  case ISD::SRA_PARTS: return LowerSRA_PARTS(Op, DAG);
  case ISD::VASTART:  return LowerVASTART(Op, DAG);
  case ISD::VACOPY:   return LowerVACOPY(Op, DAG);

  case ISD::GlobalAddress: {
    // Wrap TargetGlobalAddress in S1C33ISD::Wrapper so that the isel pattern
    //   (S1C33Wrapper tglobaladdr:$sym) → LDW_SYM_EXT0
    // can match it as a value-producing node (address materialization).
    // Without the wrapper, TargetGlobalAddress is a leaf that cannot satisfy
    // a CopyToReg when used as a data pointer argument.
    auto *N = cast<GlobalAddressSDNode>(Op);
    SDValue TGA = DAG.getTargetGlobalAddress(N->getGlobal(), SDLoc(Op),
                                             MVT::i32, N->getOffset());
    return DAG.getNode(S1C33ISD::Wrapper, SDLoc(Op), MVT::i32, TGA);
  }

  case ISD::ExternalSymbol: {
    auto *N = cast<ExternalSymbolSDNode>(Op);
    SDValue TES = DAG.getTargetExternalSymbol(N->getSymbol(), MVT::i32);
    return DAG.getNode(S1C33ISD::Wrapper, SDLoc(Op), MVT::i32, TES);
  }

  case ISD::ConstantPool: {
    // Materialize the address of a constant pool entry (e.g. De Bruijn lookup
    // tables from CTTZ expansion) using the same ext+ld.w path as GlobalAddress.
    auto *N = cast<ConstantPoolSDNode>(Op);
    SDValue TCP;
    if (N->isMachineConstantPoolEntry())
      TCP = DAG.getTargetConstantPool(N->getMachineCPVal(), MVT::i32,
                                      N->getAlign(), N->getOffset());
    else
      TCP = DAG.getTargetConstantPool(N->getConstVal(), MVT::i32,
                                      N->getAlign(), N->getOffset());
    return DAG.getNode(S1C33ISD::Wrapper, SDLoc(Op), MVT::i32, TCP);
  }
  }
}

//===----------------------------------------------------------------------===//
// Formal Arguments
//===----------------------------------------------------------------------===//

// Lower incoming function arguments using the S5U1C33000C ABI:
//   R12→R13→R14→R15 for the first four i32 args; overflow to stack.
SDValue S1C33TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  S1C33MachineFunctionInfo *FuncInfo = MF.getInfo<S1C33MachineFunctionInfo>();

  // gcc33 varargs ABI: all arguments (fixed + variadic) are on the stack.
  // Use the stack-only CC so that fixed args are also stack-allocated,
  // making VarArgsFrameIndex trivially point just past the last fixed arg.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  if (IsVarArg)
    CCInfo.AnalyzeFormalArguments(Ins, CC_S1C33_VarArg);
  else
    CCInfo.AnalyzeFormalArguments(Ins, CC_S1C33);

  for (const CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      // Argument passed in a register — create a virtual register for it.
      const TargetRegisterClass *RC = &S1C33::GR32RegClass;
      Register VReg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgVal = DAG.getCopyFromReg(Chain, DL, VReg, MVT::i32);
      InVals.push_back(ArgVal);
    } else {
      // Argument on the stack.  Create a fixed frame object so that
      // eliminateFrameIndex can compute the correct SP-relative offset after
      // the prologue (accounting for the return address + callee-saved area).
      assert(VA.isMemLoc());
      int FI = MF.getFrameInfo().CreateFixedObject(
          VA.getLocVT().getStoreSize(), VA.getLocMemOffset(), /*IsImmutable=*/true);
      SDValue FIPtr = DAG.getFrameIndex(FI, MVT::i32);
      SDValue Load = DAG.getLoad(VA.getValVT(), DL, Chain, FIPtr,
                                 MachinePointerInfo::getFixedStack(MF, FI));
      InVals.push_back(Load);
    }
  }

  // For variadic functions, record the stack offset of the first variadic arg.
  // All fixed args consumed CCInfo.getStackSize() bytes; variadic args follow.
  if (IsVarArg) {
    int64_t VarArgsOffset = CCInfo.getStackSize();
    int VarArgFI = MF.getFrameInfo().CreateFixedObject(
        4, VarArgsOffset, /*IsImmutable=*/true);
    FuncInfo->setVarArgsFrameIndex(VarArgFI);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Return value
//===----------------------------------------------------------------------===//

// Lower the return instruction using the S5U1C33000C ABI:
//   R10 for 32-bit return, R10+R11 for 64-bit.
SDValue S1C33TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  // Determine return-value register assignments.
  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_S1C33);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy return values into return registers.
  for (const CCValAssign &VA : RVLocs) {
    assert(VA.isRegLoc() && "Return value must be in a register");
    SDValue Val = OutVals[VA.getValNo()];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  // Interrupt handlers must return with reti (restores PSR and PC from stack).
  unsigned RetOpc = MF.getFunction().hasFnAttribute("interrupt_handler")
                        ? S1C33ISD::RETI_FLAG
                        : S1C33ISD::RET_FLAG;
  return DAG.getNode(RetOpc, DL, MVT::Other, RetOps);
}

//===----------------------------------------------------------------------===//
// Call lowering
//===----------------------------------------------------------------------===//

SDValue
S1C33TargetLowering::LowerCall(CallLoweringInfo &CLI,
                                 SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  MachineFunction &MF = DAG.getMachineFunction();

  // Tail calls not yet supported — the tail-call path requires the callee to
  // reuse the caller's frame, which needs frame lowering changes.  Until then,
  // disable tail calls so that LowerCallTo does not assert when InVals is
  // non-empty on a tail call.
  CLI.IsTailCall = false;

  // Convert callee to a form that isel can match:
  //   GlobalAddressSDNode  → TargetGlobalAddress  → CALL_sym (direct)
  //   ExternalSymbolSDNode → TargetExternalSymbol  → CALL_sym (direct)
  //   anything else        → S1C33ISD::Wrapper     → LDW_SYM_EXT0 + CALL_r
  //
  // The third case arises when:
  //   (a) a libcall is generated for an unregistered RTLIB entry whose
  //       getLibcallName() returns nullptr (would crash in getTargetExternalSymbol),
  //   (b) the callee is a function pointer (GR32 register value), or
  //   (c) legalization already wrapped a GlobalAddress before LowerCall ran.
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32,
                                        G->getOffset());
  } else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    assert(E->getSymbol() && "ExternalSymbol with null name in LowerCall");
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);
  } else {
    // Indirect call (function pointer, already-legalized target, etc.).
    // If the callee is already in target form (legalized before LowerCall),
    // use it as-is for CALL_sym.  Otherwise it is a register value (e.g. a
    // load result) — pass it through unchanged so that the isel pattern
    //   Pat<(S1C33Call GR32:$target), (CALL_r GR32:$target)>
    // can match it directly.  Do NOT wrap register values in S1C33ISD::Wrapper;
    // Wrapper is only for symbol address materialization.
    (void)Callee; // used as-is in all sub-cases below
  }

  // Pre-pass: collect byval struct info.
  // The S5U1C33000C ABI (gcc33) passes struct-by-value arguments entirely on
  // the stack — register slots are NOT used.  Non-byval scalar arguments still
  // use R12–R15 as usual.  We handle byval args separately from CC analysis:
  // first run CC on the non-byval args, then append byval words to the stack
  // area after the CC-assigned stack args.
  SmallVector<ISD::OutputArg, 16> ScalarOuts;
  SmallVector<SDValue, 16> ScalarOutVals;
  struct ByValInfo {
    unsigned OrigIdx;   // index in Outs[]
    unsigned Size;      // byte size of struct
    unsigned StackOff;  // assigned later
  };
  SmallVector<ByValInfo, 4> ByValArgs;

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    if (Outs[i].Flags.isByVal()) {
      ByValArgs.push_back({i, Outs[i].Flags.getByValSize(), 0});
    } else {
      ScalarOuts.push_back(Outs[i]);
      ScalarOutVals.push_back(OutVals[i]);
    }
  }

  // Analyze only non-byval (scalar) outgoing arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  if (CLI.IsVarArg)
    CCInfo.AnalyzeCallOperands(ScalarOuts, CC_S1C33_VarArg);
  else
    CCInfo.AnalyzeCallOperands(ScalarOuts, CC_S1C33);

  // Byval structs go on the stack after the CC-assigned area.
  unsigned ByValBase = CCInfo.getStackSize();
  for (auto &BV : ByValArgs) {
    BV.StackOff = ByValBase;
    ByValBase += llvm::alignTo(BV.Size, 4);
  }
  unsigned ArgsSize = ByValBase;

  // ADJCALLSTACKDOWN — reserve stack for any stack-passed arguments.
  Chain = DAG.getCALLSEQ_START(Chain, ArgsSize, 0, DL);

  // Copy arguments to registers or the stack.
  SmallVector<std::pair<Register, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  // Copy SP to a virtual register so that ADD_ri (2-address, tied) can operate
  // on a virtual register. Using a physical register directly here would cause
  // TwoAddressInstructionPass to fail (regB.isVirtual() assertion).
  SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, S1C33::SP, MVT::i32);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    const CCValAssign &VA = ArgLocs[i];
    SDValue Arg = ScalarOutVals[i];

    // If the argument value is a raw FrameIndex (e.g. sret pointer to a local
    // alloca), it cannot be used directly as the source of a CopyToReg because
    // TargetFrameIndex is a leaf node with no VR.  Convert it to an ADJFI
    // pseudo instruction that eliminateFrameIndex expands to:
    //   ld.w %rd, %sp; add %rd, offset
    if (Arg.getOpcode() == ISD::FrameIndex) {
      int FI = cast<FrameIndexSDNode>(Arg)->getIndex();
      SDValue TFI = DAG.getTargetFrameIndex(FI, MVT::i32);
      Arg = SDValue(DAG.getMachineNode(S1C33::ADJFI, DL, MVT::i32, TFI), 0);
    }

    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
    } else {
      // Stack-passed argument: store to [SP + offset] in the area reserved by
      // ADJCALLSTACKDOWN.  The callee sees it at [SP_callee + 4 + offset]
      // (after 'call' pushes the 4-byte return address).
      assert(VA.isMemLoc());
      SDValue Addr =
          DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr,
                      DAG.getConstant(VA.getLocMemOffset(), DL, MVT::i32));
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, Arg, Addr,
                       MachinePointerInfo::getStack(MF, VA.getLocMemOffset())));
    }
  }

  // Copy byval struct words to the stack (entirely on stack, no registers).
  for (const auto &BV : ByValArgs) {
    SDValue Src = OutVals[BV.OrigIdx]; // pointer to source struct
    unsigned NumWords = (BV.Size + 3) / 4;
    for (unsigned w = 0; w < NumWords; w++) {
      SDValue WordAddr = Src;
      if (w > 0)
        WordAddr = DAG.getNode(ISD::ADD, DL, MVT::i32, Src,
                               DAG.getConstant(w * 4, DL, MVT::i32));
      SDValue Word = DAG.getLoad(MVT::i32, DL, Chain, WordAddr,
                                 MachinePointerInfo());
      Chain = Word.getValue(1);

      unsigned Off = BV.StackOff + w * 4;
      SDValue DstAddr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr,
                                    DAG.getConstant(Off, DL, MVT::i32));
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, Word, DstAddr,
                       MachinePointerInfo::getStack(MF, Off)));
    }
  }

  // Merge all memory stores into one chain before the CopyToReg nodes.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // Emit CopyToReg nodes to put args in the right registers, chaining glue.
  SDValue InGlue;
  for (auto &[Reg, Val] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Val, InGlue);
    InGlue = Chain.getValue(1);
  }

  // Build the S1C33ISD::CALL node.
  // Operands: chain, callee, [arg registers...], register mask, [glue].
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (auto &[Reg, Val] : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg, MVT::i32));

  // Register mask — indicates which physical registers are preserved across call.
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CLI.CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InGlue.getNode())
    Ops.push_back(InGlue);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(S1C33ISD::CALL, DL, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  // ADJCALLSTACKUP — release the stack reservation.
  Chain = DAG.getCALLSEQ_END(Chain, ArgsSize, 0, InGlue, DL);
  InGlue = Chain.getValue(1);

  // Copy return values from their physical registers.
  SmallVector<CCValAssign, 4> RVLocs;
  CCState RetCCInfo(CLI.CallConv, CLI.IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_S1C33);

  for (const CCValAssign &VA : RVLocs) {
    assert(VA.isRegLoc() && "Return value must be in a register");
    SDValue Val = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(),
                                     VA.getValVT(), InGlue);
    InVals.push_back(Val.getValue(0));
    Chain   = Val.getValue(1);
    InGlue  = Val.getValue(2);
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// Conditional branch lowering
//===----------------------------------------------------------------------===//

// Lower ISD::BR_CC to S1C33ISD::CMP (produces Glue) + S1C33ISD::BRCOND.
SDValue S1C33TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  S1C33CC::CondCode S1C33CC = convertCondCode(CC);
  SDValue Cmp = DAG.getNode(S1C33ISD::CMP, DL, MVT::Glue, LHS, RHS);
  return DAG.getNode(S1C33ISD::BRCOND, DL, MVT::Other,
                     Chain, Dest, DAG.getConstant(S1C33CC, DL, MVT::i32), Cmp);
}

// Lower ISD::SETCC to SELECT_CC(lhs, rhs, 1, 0, cc).
// S1C33 has no native "compare and set boolean" instruction; we reuse the
// SELECT_CC → SELECT pseudo path which expands to CMP + conditional branch + PHI.
SDValue S1C33TargetLowering::LowerSETCC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  S1C33CC::CondCode S1C33CC = convertCondCode(CC);
  SDValue True  = DAG.getConstant(1, DL, MVT::i32);
  SDValue False = DAG.getConstant(0, DL, MVT::i32);
  return DAG.getNode(S1C33ISD::SELECT_CC, DL, MVT::i32,
                     LHS, RHS, True, False,
                     DAG.getConstant(S1C33CC, DL, MVT::i32));
}

//===----------------------------------------------------------------------===//
// Variadic function support
//===----------------------------------------------------------------------===//

// Lower ISD::VASTART — initialize a va_list with the address of the first
// variadic argument.  gcc33 ABI: all args (including fixed) are on the stack
// when the callee is variadic, so va_list is just a pointer (char *).
//
// VASTART(ptr) — store the address of the first variadic arg into *ptr.
SDValue S1C33TargetLowering::LowerVASTART(SDValue Op,
                                            SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  S1C33MachineFunctionInfo *FuncInfo = MF.getInfo<S1C33MachineFunctionInfo>();

  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue VaListPtr = Op.getOperand(1);
  MachinePointerInfo PtrInfo =
      cast<SrcValueSDNode>(Op.getOperand(2))->getValue()
          ? MachinePointerInfo(cast<SrcValueSDNode>(Op.getOperand(2))->getValue())
          : MachinePointerInfo();

  // Materialize the runtime address of the first variadic argument.
  // ADJFI expands to: ld.w %rd, %sp; add %rd, offset
  // so that %rd holds the absolute address (SP + offset) at runtime.
  SDValue TFI = DAG.getTargetFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                         MVT::i32);
  SDValue VarArgsAddr =
      SDValue(DAG.getMachineNode(S1C33::ADJFI, DL, MVT::i32, TFI), 0);

  // Store the address into the va_list pointer.
  return DAG.getStore(Chain, DL, VarArgsAddr, VaListPtr, PtrInfo);
}

// Lower ISD::VACOPY — copy a va_list (just a pointer copy).
// va_list on S1C33 is a single i32 pointer; copying it is a plain 4-byte store.
SDValue S1C33TargetLowering::LowerVACOPY(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain   = Op.getOperand(0);
  SDValue DstPtr  = Op.getOperand(1);  // destination va_list address
  SDValue SrcPtr  = Op.getOperand(2);  // source va_list address
  const Value *DstSV = cast<SrcValueSDNode>(Op.getOperand(3))->getValue();
  const Value *SrcSV = cast<SrcValueSDNode>(Op.getOperand(4))->getValue();

  // Load the pointer from the source va_list, then store to the destination.
  SDValue Ptr = DAG.getLoad(MVT::i32, DL, Chain, SrcPtr,
                             MachinePointerInfo(SrcSV));
  return DAG.getStore(Ptr.getValue(1), DL, Ptr, DstPtr,
                      MachinePointerInfo(DstSV));
}

// Lower ISD::SELECT (boolean condition) to S1C33ISD::SELECT_CC.
// The S1C33 has no conditional-move instruction; convert:
//   select cond, TrueVal, FalseVal
// to:
//   S1C33ISD::SELECT_CC(cond, 0, TrueVal, FalseVal, NE)
// which expands via EmitInstrWithCustomInserter using a CMP + conditional branch.
SDValue S1C33TargetLowering::LowerSELECT(SDValue Op,
                                          SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Cond = Op.getOperand(0);
  SDValue TrueVal = Op.getOperand(1);
  SDValue FalseVal = Op.getOperand(2);

  // Ensure condition is i32 (it may be i1 after LTO IPO).
  if (Cond.getValueType() != MVT::i32)
    Cond = DAG.getZExtOrTrunc(Cond, DL, MVT::i32);

  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  return DAG.getNode(S1C33ISD::SELECT_CC, DL, Op.getValueType(),
                     Cond, Zero, TrueVal, FalseVal,
                     DAG.getConstant(S1C33CC::NE, DL, MVT::i32));
}

// Lower ISD::SELECT_CC to S1C33ISD::SELECT_CC, which becomes the SELECT pseudo
// instruction and is expanded via EmitInstrWithCustomInserter.
SDValue S1C33TargetLowering::LowerSELECT_CC(SDValue Op,
                                              SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueVal = Op.getOperand(2);
  SDValue FalseVal = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();

  S1C33CC::CondCode S1C33CC = convertCondCode(CC);
  return DAG.getNode(S1C33ISD::SELECT_CC, DL, Op.getValueType(),
                     LHS, RHS, TrueVal, FalseVal,
                     DAG.getConstant(S1C33CC, DL, MVT::i32));
}

//===----------------------------------------------------------------------===//
// SELECT pseudo expansion (EmitInstrWithCustomInserter)
//===----------------------------------------------------------------------===//

// Variable shift expansion.
// S1C33 shift instructions (both immediate and register-register forms) only
// support shift amounts 0-8.  For a variable shift where the amount may
// exceed 8, we expand into a loop:
//
//   BB:
//     cmp %amt, 8
//     jrule DoneMBB           ← amt <= 8, skip loop
//   LoopMBB:                  ← shift by 8 repeatedly
//     %lp_val = PHI [%val, BB], [%shifted, LoopMBB]
//     %lp_amt = PHI [%amt, BB], [%new_amt, LoopMBB]
//     %shifted = srl %lp_val, 8
//     %new_amt = sub %lp_amt, 8
//     cmp %new_amt, 8
//     jrugt LoopMBB
//   DoneMBB:
//     %d_val = PHI [%val, BB], [%shifted, LoopMBB]
//     %d_amt = PHI [%amt, BB], [%new_amt, LoopMBB]
//     %dst   = srl %d_val, %d_amt
//
MachineBasicBlock *
S1C33TargetLowering::emitVariableShift(MachineInstr &MI,
                                       MachineBasicBlock *BB) const {
  unsigned Opc = MI.getOpcode();
  unsigned ShiftImmOpc, ShiftRegOpc;
  switch (Opc) {
  case S1C33::VSRL: ShiftImmOpc = S1C33::SRL_ri; ShiftRegOpc = S1C33::SRL_rr; break;
  case S1C33::VSLL: ShiftImmOpc = S1C33::SLL_ri; ShiftRegOpc = S1C33::SLL_rr; break;
  case S1C33::VSRA: ShiftImmOpc = S1C33::SRA_ri; ShiftRegOpc = S1C33::SRA_rr; break;
  default: llvm_unreachable("unexpected variable shift opcode");
  }

  const S1C33Subtarget &STI = BB->getParent()->getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  DebugLoc DL = MI.getDebugLoc();
  const TargetRegisterClass *RC = &S1C33::GR32RegClass;

  Register Dst = MI.getOperand(0).getReg();
  Register Val = MI.getOperand(1).getReg();
  Register Amt = MI.getOperand(2).getReg();

  // Create LoopMBB and DoneMBB.
  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *DoneMBB = MF->CreateMachineBasicBlock();

  MachineFunction::iterator It = ++BB->getIterator();
  MF->insert(It, LoopMBB);
  MF->insert(It, DoneMBB);

  // Move the tail of BB into DoneMBB.
  DoneMBB->splice(DoneMBB->begin(), BB,
                  std::next(MachineBasicBlock::iterator(MI)), BB->end());
  DoneMBB->transferSuccessorsAndUpdatePHIs(BB);

  // BB → LoopMBB (fall-through if amt > 8) and BB → DoneMBB (branch if amt <= 8).
  BB->addSuccessor(LoopMBB);
  BB->addSuccessor(DoneMBB);
  // LoopMBB → LoopMBB (loop back) and LoopMBB → DoneMBB (exit).
  LoopMBB->addSuccessor(LoopMBB);
  LoopMBB->addSuccessor(DoneMBB);

  // BB: cmp %amt, 8; jrule DoneMBB
  BuildMI(BB, DL, TII.get(S1C33::CMP_ri)).addReg(Amt).addImm(8);
  BuildMI(BB, DL, TII.get(S1C33::JRULE)).addMBB(DoneMBB);

  // LoopMBB: PHIs, shift-by-8, sub-8, cmp, branch-back.
  Register LoopVal = MRI.createVirtualRegister(RC);
  Register LoopAmt = MRI.createVirtualRegister(RC);
  Register ShiftedVal = MRI.createVirtualRegister(RC);
  Register NewAmt = MRI.createVirtualRegister(RC);

  BuildMI(LoopMBB, DL, TII.get(TargetOpcode::PHI), LoopVal)
      .addReg(Val).addMBB(BB)
      .addReg(ShiftedVal).addMBB(LoopMBB);
  BuildMI(LoopMBB, DL, TII.get(TargetOpcode::PHI), LoopAmt)
      .addReg(Amt).addMBB(BB)
      .addReg(NewAmt).addMBB(LoopMBB);
  BuildMI(LoopMBB, DL, TII.get(ShiftImmOpc), ShiftedVal)
      .addReg(LoopVal).addImm(8);
  BuildMI(LoopMBB, DL, TII.get(S1C33::SUB_ri), NewAmt)
      .addReg(LoopAmt).addImm(8);
  BuildMI(LoopMBB, DL, TII.get(S1C33::CMP_ri)).addReg(NewAmt).addImm(8);
  BuildMI(LoopMBB, DL, TII.get(S1C33::JRUGT)).addMBB(LoopMBB);

  // DoneMBB: PHIs for value/amount, then final register-register shift.
  Register DoneVal = MRI.createVirtualRegister(RC);
  Register DoneAmt = MRI.createVirtualRegister(RC);

  BuildMI(*DoneMBB, DoneMBB->begin(), DL, TII.get(TargetOpcode::PHI), DoneAmt)
      .addReg(Amt).addMBB(BB)
      .addReg(NewAmt).addMBB(LoopMBB);
  BuildMI(*DoneMBB, DoneMBB->begin(), DL, TII.get(TargetOpcode::PHI), DoneVal)
      .addReg(Val).addMBB(BB)
      .addReg(ShiftedVal).addMBB(LoopMBB);
  // Insert the final shift after the PHIs.
  auto InsertPt = DoneMBB->begin();
  while (InsertPt != DoneMBB->end() && InsertPt->isPHI())
    ++InsertPt;
  BuildMI(*DoneMBB, InsertPt, DL, TII.get(ShiftRegOpc), Dst)
      .addReg(DoneVal).addReg(DoneAmt);

  MI.eraseFromParent();
  return DoneMBB;
}
//
//   BB:
//     cmp %lhs, %rhs
//     jrXX MergeBB        ← branch-taken = TRUE path (skips false copy)
//   FalseBB:              ← fall-through = FALSE path
//     ld.w %dst, %falseval
//   MergeBB:
//     %dst = phi [%dst from FalseBB, %trueval from BB]
//
// Layout: BB, FalseBB, MergeBB (so BB falls through to FalseBB).
MachineBasicBlock *
S1C33TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                  MachineBasicBlock *BB) const {
  unsigned Opc = MI.getOpcode();
  if (Opc == S1C33::VSRL || Opc == S1C33::VSLL || Opc == S1C33::VSRA)
    return emitVariableShift(MI, BB);

  assert(Opc == S1C33::SELECT && "Unexpected pseudo opcode");

  const S1C33Subtarget &STI = BB->getParent()->getSubtarget<S1C33Subtarget>();
  const S1C33InstrInfo &TII =
      *static_cast<const S1C33InstrInfo *>(STI.getInstrInfo());
  DebugLoc DL = MI.getDebugLoc();

  Register Dst      = MI.getOperand(0).getReg();
  Register LHS      = MI.getOperand(1).getReg();
  Register RHS      = MI.getOperand(2).getReg();
  Register TrueVal  = MI.getOperand(3).getReg();
  Register FalseVal = MI.getOperand(4).getReg();
  unsigned CC       = MI.getOperand(5).getImm();

  MachineFunction *MF = BB->getParent();
  MachineBasicBlock *FalseMBB = MF->CreateMachineBasicBlock();
  MachineBasicBlock *MergeMBB = MF->CreateMachineBasicBlock();

  // Insert FalseMBB after BB, MergeMBB after FalseMBB.
  // It points to the block that was originally after BB; both inserts go before It.
  MachineFunction::iterator It = ++BB->getIterator();
  MF->insert(It, FalseMBB);  // BB, FalseMBB, [original next...]
  MF->insert(It, MergeMBB); // BB, FalseMBB, MergeMBB, [original next...]

  // Transfer the tail of BB (after SELECT) and its successors into MergeMBB.
  MergeMBB->splice(MergeMBB->begin(), BB,
                   std::next(MachineBasicBlock::iterator(MI)), BB->end());
  MergeMBB->transferSuccessorsAndUpdatePHIs(BB);

  // BB: fall-through → FalseMBB, branch-taken → MergeMBB.
  BB->addSuccessor(FalseMBB);
  BB->addSuccessor(MergeMBB);
  FalseMBB->addSuccessor(MergeMBB);

  // Emit CMP + conditional branch (taken = true condition → MergeMBB).
  BuildMI(BB, DL, TII.get(S1C33::CMP_rr)).addReg(LHS).addReg(RHS);
  unsigned BrOpc = getBranchOpcode((S1C33CC::CondCode)CC);
  BuildMI(BB, DL, TII.get(BrOpc)).addMBB(MergeMBB);

  // FalseBB: copy FalseVal into a fresh vreg (then fall through to MergeMBB).
  MachineRegisterInfo &MRI = MF->getRegInfo();
  Register FalseCopy = MRI.createVirtualRegister(MRI.getRegClass(Dst));
  BuildMI(*FalseMBB, FalseMBB->begin(), DL, TII.get(S1C33::MOV_rr), FalseCopy)
      .addReg(FalseVal);

  // MergeBB: PHI Dst = [FalseCopy from FalseMBB, TrueVal from BB].
  BuildMI(*MergeMBB, MergeMBB->begin(), DL, TII.get(TargetOpcode::PHI), Dst)
      .addReg(FalseCopy).addMBB(FalseMBB)
      .addReg(TrueVal).addMBB(BB);

  MI.eraseFromParent();
  return MergeMBB;
}

// Allow base-register + constant-offset addressing: [%rb + off] where the
// offset fits in a single ext instruction (13-bit signed, [-4096, 4095]).
// Without this override, CodeGenPrepare would not sink GEPs into loads, but
// our isel patterns handle (load (add %rb, imm13)) regardless.
bool S1C33TargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                                 const AddrMode &AM, Type *Ty,
                                                 unsigned AS,
                                                 Instruction *I) const {
  // No scaled-register addressing.
  if (AM.Scale != 0)
    return false;
  // No global-variable base.
  if (AM.BaseGV)
    return false;
  // Pure base-register (no offset).
  if (AM.BaseOffs == 0)
    return AM.HasBaseReg;
  // Base-register + constant offset within single-ext reach.
  return AM.HasBaseReg && isInt<13>(AM.BaseOffs);
}

//===----------------------------------------------------------------------===//
// DAG Combines
//===----------------------------------------------------------------------===//

// Helper: try to fold BR_CC or SETCC when LHS is SEXT_INREG and RHS is 0.
//
// InstCombine converts  (x & 0x80) != 0  →  (int8_t)x < 0
// i.e. SETLT(SEXT_INREG(x, i8), 0).
//
// On S1C33, SEXT_INREG(x, i8) expands to 3×sll + 3×sra (6 instructions)
// because each shift is limited to 8 bits.  Comparing against 0 for sign
// only needs  ext 2; and x, 0  (=  and x, 0x80),  so we fold early:
//   SEXT_INREG(x, i8)  <s 0  →  (x & 0x80) != 0
//   SEXT_INREG(x, i8)  >=  0  →  (x & 0x80) == 0
// Same for i16 (signbit = 0x8000).
static bool foldSextInregSignBit(SDValue LHS, SDValue RHS, ISD::CondCode CC,
                                  SDLoc DL, SelectionDAG &DAG,
                                  SDValue &OutLHS, SDValue &OutRHS,
                                  ISD::CondCode &OutCC) {
  // LHS must be SEXT_INREG of i8 or i16.
  if (LHS.getOpcode() != ISD::SIGN_EXTEND_INREG)
    return false;
  EVT InVT = cast<VTSDNode>(LHS.getOperand(1))->getVT();
  unsigned SignBit;
  if (InVT == MVT::i8)
    SignBit = 0x80;
  else if (InVT == MVT::i16)
    SignBit = 0x8000;
  else
    return false;

  // Match the four sign-bit test patterns:
  //   <s 0   (SETLT,  RHS=0)   sign bit set
  //   >=s 0  (SETGE,  RHS=0)   sign bit clear
  //   >s -1  (SETGT,  RHS=-1)  sign bit clear
  //   <=s -1 (SETLE,  RHS=-1)  sign bit set
  auto *RHSC = dyn_cast<ConstantSDNode>(RHS);
  if (!RHSC)
    return false;
  int64_t RHSVal = RHSC->getSExtValue();

  ISD::CondCode NewCC;
  if (RHSVal == 0 && CC == ISD::SETLT)
    NewCC = ISD::SETNE;
  else if (RHSVal == 0 && CC == ISD::SETGE)
    NewCC = ISD::SETEQ;
  else if (RHSVal == -1 && CC == ISD::SETGT)
    NewCC = ISD::SETEQ;
  else if (RHSVal == -1 && CC == ISD::SETLE)
    NewCC = ISD::SETNE;
  else
    return false;

  OutLHS = DAG.getNode(ISD::AND, DL, MVT::i32, LHS.getOperand(0),
                       DAG.getConstant(SignBit, DL, MVT::i32));
  OutRHS = DAG.getConstant(0, DL, MVT::i32);
  OutCC  = NewCC;
  return true;
}

SDValue S1C33TargetLowering::combineBRCC(SDNode *N,
                                          SelectionDAG &DAG) const {
  SDLoc DL(N);
  ISD::CondCode CC  = cast<CondCodeSDNode>(N->getOperand(1))->get();
  SDValue LHS       = N->getOperand(2);
  SDValue RHS       = N->getOperand(3);

  SDValue NewLHS, NewRHS;
  ISD::CondCode NewCC;
  if (!foldSextInregSignBit(LHS, RHS, CC, DL, DAG, NewLHS, NewRHS, NewCC))
    return SDValue();

  return DAG.getNode(ISD::BR_CC, DL, MVT::Other,
                     N->getOperand(0), DAG.getCondCode(NewCC),
                     NewLHS, NewRHS, N->getOperand(4));
}

SDValue S1C33TargetLowering::combineSETCC(SDNode *N,
                                           SelectionDAG &DAG) const {
  SDLoc DL(N);
  SDValue LHS      = N->getOperand(0);
  SDValue RHS      = N->getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(N->getOperand(2))->get();

  SDValue NewLHS, NewRHS;
  ISD::CondCode NewCC;
  if (!foldSextInregSignBit(LHS, RHS, CC, DL, DAG, NewLHS, NewRHS, NewCC))
    return SDValue();

  return DAG.getSetCC(DL, N->getValueType(0), NewLHS, NewRHS, NewCC);
}

// Signed division/modulo by power of 2 — bias calculation optimization.
//
// LLVM's InstCombine transforms `sdiv x, 2^N` into:
//   sra(add(x, srl(sra(x, 31), 32-N)), N)
// The bias sub-expression `srl(sra(x, 31), 32-N)` computes:
//   0 when x >= 0, (2^N - 1) when x < 0.
// The generic combiner may simplify this to `and(sra(x, 31), 2^N - 1)`.
//
// On S1C33, max shift is 8 per instruction, so `sra(x, 31)` alone costs
// 4 shifts.  Replace the bias with a target-specific SELECT_CC node
// (cmp + branch) that the generic combiner cannot undo.
//
// We match both forms:
//   SRL: (srl (sra x, 31), K)          — before generic simplification
//   AND: (and (sra x, 31), 2^N - 1)    — after generic simplification

// Helper: emit S1C33ISD::SELECT_CC(x, 0, bias, 0, LT) — target-opaque.
static SDValue emitBiasSelect(SDValue X, uint64_t Bias,
                              SDLoc DL, SelectionDAG &DAG) {
  EVT VT = X.getValueType();
  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue BiasVal = DAG.getConstant(Bias, DL, VT);
  SDValue CCVal = DAG.getConstant(S1C33CC::LT, DL, MVT::i32);
  return DAG.getNode(S1C33ISD::SELECT_CC, DL, VT,
                     X, Zero, BiasVal, Zero, CCVal);
}

// Match: (srl (sra x, 31), K) → SELECT_CC bias
static SDValue combineSrlSraBias(SDNode *N, SelectionDAG &DAG) {
  EVT VT = N->getValueType(0);
  if (VT != MVT::i32)
    return SDValue();

  auto *ShiftAmtC = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!ShiftAmtC)
    return SDValue();
  unsigned K = ShiftAmtC->getZExtValue();
  if (K < 2 || K > 31)
    return SDValue();

  SDValue Inner = N->getOperand(0);
  if (Inner.getOpcode() != ISD::SRA)
    return SDValue();
  auto *InnerShiftC = dyn_cast<ConstantSDNode>(Inner.getOperand(1));
  if (!InnerShiftC || InnerShiftC->getZExtValue() != 31)
    return SDValue();

  unsigned ShiftCost = (31 + 7) / 8 + (K + 7) / 8;
  if (ShiftCost <= 4)
    return SDValue();

  SDValue X = Inner.getOperand(0);
  uint64_t Bias = (1u << (32 - K)) - 1;
  return emitBiasSelect(X, Bias, SDLoc(N), DAG);
}

// Match: (and (sra x, 31), mask) where mask = 2^N - 1 → SELECT_CC bias
static SDValue combineAndSraBias(SDNode *N, SelectionDAG &DAG) {
  EVT VT = N->getValueType(0);
  if (VT != MVT::i32)
    return SDValue();

  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);
  // Normalise: SRA on LHS
  if (LHS.getOpcode() != ISD::SRA && RHS.getOpcode() == ISD::SRA)
    std::swap(LHS, RHS);
  if (LHS.getOpcode() != ISD::SRA)
    return SDValue();

  auto *InnerShiftC = dyn_cast<ConstantSDNode>(LHS.getOperand(1));
  if (!InnerShiftC || InnerShiftC->getZExtValue() != 31)
    return SDValue();

  auto *MaskC = dyn_cast<ConstantSDNode>(RHS);
  if (!MaskC)
    return SDValue();
  uint64_t Mask = MaskC->getZExtValue();
  if (Mask == 0 || !isPowerOf2_64(Mask + 1))
    return SDValue();

  // sra 31 costs 4 shifts; AND costs 1-2 (ext+and). Only optimize if total > 4.
  unsigned AndCost = (Mask > 31) ? 2 : 1;
  if (4 + AndCost <= 4)
    return SDValue();

  SDValue X = LHS.getOperand(0);
  return emitBiasSelect(X, Mask, SDLoc(N), DAG);
}

//===----------------------------------------------------------------------===//
// 64-bit shift parts
//===----------------------------------------------------------------------===//
//
// S1C33 is a 32-bit target with no 64-bit shift instruction.  LLVM lowers
// i64 shifts into SHL_PARTS / SRL_PARTS / SRA_PARTS nodes that each take
// (lo, hi, shamt) and return (result_lo, result_hi).
//
// The generic algorithm (with an explicit SELECT to handle shamt >= 32):
//
//   SHL_PARTS(lo, hi, shamt):
//     if shamt < 32:
//       result_lo = lo << shamt
//       result_hi = (hi << shamt) | (lo >> (32 - shamt))   [0 when shamt==0]
//     else:
//       result_lo = 0
//       result_hi = lo << (shamt - 32)
//
//   SRL_PARTS(lo, hi, shamt):
//     if shamt < 32:
//       result_lo = (lo >> shamt) | (hi << (32 - shamt))   [0 when shamt==0]
//       result_hi = hi >> shamt
//     else:
//       result_lo = hi >> (shamt - 32)
//       result_hi = 0
//
//   SRA_PARTS(lo, hi, shamt) — same as SRL but arithmetic for hi and hi>>31:
//     if shamt < 32:
//       result_lo = (lo >> shamt) | (hi << (32 - shamt))   [0 when shamt==0]
//       result_hi = hi >>a shamt
//     else:
//       result_lo = hi >>a (shamt - 32)
//       result_hi = hi >>a 31

// Helper: build (shamt == 0) ? zero : val, to avoid undefined shift by 32.
static SDValue selectIfShAmtZero(SDValue Val, SDValue ShAmt, SDLoc DL,
                                  SelectionDAG &DAG) {
  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  SDValue IsZero = DAG.getSetCC(DL, MVT::i32, ShAmt, Zero, ISD::SETEQ);
  return DAG.getNode(ISD::SELECT, DL, MVT::i32, IsZero, Zero, Val);
}

SDValue S1C33TargetLowering::LowerSHL_PARTS(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue ShAmt = Op.getOperand(2);

  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  SDValue Bits = DAG.getConstant(32, DL, MVT::i32);

  // ExtraShAmt = shamt - 32  (negative when shamt < 32)
  SDValue ExtraShAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, ShAmt, Bits);
  // RevShAmt = 32 - shamt
  SDValue RevShAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, Bits, ShAmt);

  // Normal case (shamt < 32):
  //   result_lo = lo << shamt
  //   result_hi = (hi << shamt) | carry, where carry = (lo >> RevShAmt) if shamt!=0 else 0
  SDValue Carry = selectIfShAmtZero(
      DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, RevShAmt), ShAmt, DL, DAG);
  SDValue NormLo = DAG.getNode(ISD::SHL, DL, MVT::i32, Lo, ShAmt);
  SDValue NormHi = DAG.getNode(ISD::OR, DL, MVT::i32,
                                DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, ShAmt),
                                Carry);

  // Big case (shamt >= 32):
  //   result_lo = 0
  //   result_hi = lo << (shamt - 32)
  SDValue BigHi = DAG.getNode(ISD::SHL, DL, MVT::i32, Lo, ExtraShAmt);

  // Select between cases: ExtraShAmt < 0 ↔ shamt < 32
  SDValue IsBig = DAG.getSetCC(DL, MVT::i32, ExtraShAmt, Zero, ISD::SETGE);
  SDValue ResultLo = DAG.getNode(ISD::SELECT, DL, MVT::i32, IsBig, Zero, NormLo);
  SDValue ResultHi = DAG.getNode(ISD::SELECT, DL, MVT::i32, IsBig, BigHi, NormHi);

  SDValue Parts[2] = {ResultLo, ResultHi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue S1C33TargetLowering::LowerSRL_PARTS(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue ShAmt = Op.getOperand(2);

  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  SDValue Bits = DAG.getConstant(32, DL, MVT::i32);

  SDValue ExtraShAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, ShAmt, Bits);
  SDValue RevShAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, Bits, ShAmt);

  // Normal case (shamt < 32):
  //   result_lo = (lo >> shamt) | carry, carry = (hi << RevShAmt) if shamt!=0 else 0
  //   result_hi = hi >> shamt
  SDValue Carry = selectIfShAmtZero(
      DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, RevShAmt), ShAmt, DL, DAG);
  SDValue NormLo = DAG.getNode(ISD::OR, DL, MVT::i32,
                                DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, ShAmt),
                                Carry);
  SDValue NormHi = DAG.getNode(ISD::SRL, DL, MVT::i32, Hi, ShAmt);

  // Big case (shamt >= 32):
  //   result_lo = hi >> (shamt - 32)
  //   result_hi = 0
  SDValue BigLo = DAG.getNode(ISD::SRL, DL, MVT::i32, Hi, ExtraShAmt);

  SDValue IsBig = DAG.getSetCC(DL, MVT::i32, ExtraShAmt, Zero, ISD::SETGE);
  SDValue ResultLo = DAG.getNode(ISD::SELECT, DL, MVT::i32, IsBig, BigLo, NormLo);
  SDValue ResultHi = DAG.getNode(ISD::SELECT, DL, MVT::i32, IsBig, Zero, NormHi);

  SDValue Parts[2] = {ResultLo, ResultHi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue S1C33TargetLowering::LowerSRA_PARTS(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue ShAmt = Op.getOperand(2);

  SDValue Zero = DAG.getConstant(0, DL, MVT::i32);
  SDValue Bits = DAG.getConstant(32, DL, MVT::i32);

  SDValue ExtraShAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, ShAmt, Bits);
  SDValue RevShAmt = DAG.getNode(ISD::SUB, DL, MVT::i32, Bits, ShAmt);

  // Sign-extension word: all bits = sign bit of hi
  SDValue SignWord = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi,
                                 DAG.getConstant(31, DL, MVT::i32));

  // Normal case (shamt < 32):
  //   result_lo = (lo >> shamt) | carry, carry = (hi << RevShAmt) if shamt!=0 else 0
  //   result_hi = hi >>a shamt
  SDValue Carry = selectIfShAmtZero(
      DAG.getNode(ISD::SHL, DL, MVT::i32, Hi, RevShAmt), ShAmt, DL, DAG);
  SDValue NormLo = DAG.getNode(ISD::OR, DL, MVT::i32,
                                DAG.getNode(ISD::SRL, DL, MVT::i32, Lo, ShAmt),
                                Carry);
  SDValue NormHi = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi, ShAmt);

  // Big case (shamt >= 32):
  //   result_lo = hi >>a (shamt - 32)
  //   result_hi = hi >>a 31  (sign extension)
  SDValue BigLo = DAG.getNode(ISD::SRA, DL, MVT::i32, Hi, ExtraShAmt);

  SDValue IsBig = DAG.getSetCC(DL, MVT::i32, ExtraShAmt, Zero, ISD::SETGE);
  SDValue ResultLo = DAG.getNode(ISD::SELECT, DL, MVT::i32, IsBig, BigLo, NormLo);
  SDValue ResultHi = DAG.getNode(ISD::SELECT, DL, MVT::i32, IsBig, SignWord, NormHi);

  SDValue Parts[2] = {ResultLo, ResultHi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue S1C33TargetLowering::PerformDAGCombine(SDNode *N,
                                                DAGCombinerInfo &DCI) const {
  SelectionDAG &DAG = DCI.DAG;
  switch (N->getOpcode()) {
  case ISD::BR_CC:  return combineBRCC(N, DAG);
  case ISD::SETCC:  return combineSETCC(N, DAG);
  case ISD::SRL:
    return combineSrlSraBias(N, DAG);
  case ISD::AND:
    return combineAndSraBias(N, DAG);
  default:          return SDValue();
  }
}
