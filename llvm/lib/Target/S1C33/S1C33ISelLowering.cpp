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

  // S1C33 is not in the generated RuntimeLibcallsImpl target table
  // (setTargetRuntimeLibcallSets has no S1C33 entry), so AvailableLibcallImpls
  // remains empty and getLibcallName() returns nullptr for every RTLIB entry.
  // We must explicitly register every libcall implementation we want to use.

  // Memory operations — implemented in P/ECE SDK string.lib.
  setLibcallImpl(RTLIB::MEMCPY,  RTLIB::impl_memcpy);
  setLibcallImpl(RTLIB::MEMMOVE, RTLIB::impl_memmove);
  setLibcallImpl(RTLIB::MEMSET,  RTLIB::impl_memset);

  // Integer division/remainder — implemented in P/ECE SDK idiv.lib.
  setLibcallImpl(RTLIB::SDIV_I32,  RTLIB::impl___divsi3);
  setLibcallImpl(RTLIB::UDIV_I32,  RTLIB::impl___udivsi3);
  setLibcallImpl(RTLIB::SREM_I32,  RTLIB::impl___modsi3);
  setLibcallImpl(RTLIB::UREM_I32,  RTLIB::impl___umodsi3);

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

  // S1C33 has no sign-extend-in-register instruction.  Expand to shifts:
  // SIGN_EXTEND_INREG i8  → (x << 24) >> 24 (SLL + SRA)
  // SIGN_EXTEND_INREG i16 → (x << 16) >> 16
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8,  Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);

  // Conditional branches: lower BR_CC to S1C33ISD::CMP + S1C33ISD::BRCOND.
  setOperationAction(ISD::BR_CC,    MVT::i32,   Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i32,  Custom);
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
  setOperationAction(ISD::VACOPY,  MVT::Other, Custom);
  setOperationAction(ISD::VAEND,   MVT::Other, Expand);
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
  case ISD::SELECT_CC: return LowerSELECT_CC(Op, DAG);
  case ISD::SETCC:    return LowerSETCC(Op, DAG);
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

  // Pre-pass: compute extra stack space needed for byval struct copies.
  // Byval args are copied to the outgoing argument area by the caller and
  // their address (StackPtr + offset) is passed in the argument register.
  // We do NOT use CreateStackObject here to avoid FrameIndex-in-CopyToReg,
  // which S1C33 cannot lower (no LEA instruction).
  unsigned ByValExtra = 0;
  SmallVector<unsigned, 4> ByValOffsets;
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    ISD::ArgFlagsTy Flags = Outs[i].Flags;
    if (!Flags.isByVal())
      continue;
    unsigned Align = Flags.getNonZeroByValAlign().value();
    ByValExtra = llvm::alignTo(ByValExtra, Align);
    ByValOffsets.push_back(ByValExtra);
    ByValExtra += Flags.getByValSize();
  }

  // Analyze outgoing arguments (call site).
  // gcc33 varargs ABI: when calling a variadic function, all args go to stack.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  if (CLI.IsVarArg)
    CCInfo.AnalyzeCallOperands(Outs, CC_S1C33_VarArg);
  else
    CCInfo.AnalyzeCallOperands(Outs, CC_S1C33);

  // Total outgoing stack: regular stack args + byval struct copies.
  unsigned RegArgsSize = CCInfo.getStackSize();
  unsigned ArgsSize = RegArgsSize + ByValExtra;

  // ADJCALLSTACKDOWN — reserve stack for any stack-passed arguments.
  Chain = DAG.getCALLSEQ_START(Chain, ArgsSize, 0, DL);

  // Copy arguments to registers or the stack.
  SmallVector<std::pair<Register, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  // Copy SP to a virtual register so that ADD_ri (2-address, tied) can operate
  // on a virtual register. Using a physical register directly here would cause
  // TwoAddressInstructionPass to fail (regB.isVirtual() assertion).
  SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, S1C33::SP, MVT::i32);

  unsigned ByValIdx = 0;
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    const CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];
    ISD::ArgFlagsTy Flags = Outs[i].Flags;

    if (Flags.isByVal()) {
      // Byval struct arg: copy the struct into the outgoing arg area and pass
      // the address (StackPtr + offset) in the argument register.
      // Offset is placed after the regular stack arg area to avoid aliasing.
      unsigned BVOff = RegArgsSize + ByValOffsets[ByValIdx++];
      unsigned Size = Flags.getByValSize();
      Align Alignment = Flags.getNonZeroByValAlign();
      SDValue DstAddr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr,
                                    DAG.getConstant(BVOff, DL, MVT::i32));
      SDValue SizeNode = DAG.getConstant(Size, DL, MVT::i32);
      Chain = DAG.getMemcpy(Chain, DL, DstAddr, Arg, SizeNode, Alignment,
                            /*isVol=*/false, /*AlwaysInline=*/false,
                            /*CI=*/nullptr, /*OverrideTailCall=*/std::nullopt,
                            MachinePointerInfo::getStack(MF, BVOff),
                            MachinePointerInfo());
      MemOpChains.push_back(Chain);
      // Pass the copy's address (an i32 ADD node, not a raw FrameIndex) so
      // that CopyToReg can emit it without needing a LEA instruction.
      Arg = DstAddr;
    }

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

// Expand the SELECT pseudo using the triangle pattern (following MSP430):
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
  assert(MI.getOpcode() == S1C33::SELECT && "Unexpected pseudo opcode");

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
