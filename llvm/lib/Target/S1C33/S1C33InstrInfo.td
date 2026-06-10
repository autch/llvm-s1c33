//===-- S1C33InstrInfo.td - S1C33 Instruction Definitions -*-- tablegen --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33000 CPU Core Manual §4.3 (Individual Instruction Reference).
//
// Encoding convention:
//   Operand names in (outs)/(ins) MUST match the bit field names in the
//   format class (e.g. $rb, $rd, $rs, $imm6, $imm8, $imm10, $imm13).
//   This is required for the MC code emitter to fill in the correct bits.
//   For 2-address (destructive) instructions, the destination and first
//   source use the same name "$rd" in both outs and ins.
//
//===----------------------------------------------------------------------===//

include "S1C33InstrFormats.td"

//===----------------------------------------------------------------------===//
// Custom SDNodes
//===----------------------------------------------------------------------===//

// S1C33Ret — DAG pattern for the RET instruction.
def S1C33Ret : SDNode<"S1C33ISD::RET_FLAG", SDTNone,
    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// S1C33Reti — DAG pattern for the RETI instruction (interrupt handler return).
def S1C33Reti : SDNode<"S1C33ISD::RETI_FLAG", SDTNone,
    [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// S1C33Call — DAG node emitted by LowerCall.
def SDT_S1C33Call : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def S1C33Call : SDNode<"S1C33ISD::CALL", SDT_S1C33Call,
    [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;

// S1C33CmpCC — compare two values, producing a PSR flag (Glue).
def SDT_S1C33CmpCC : SDTypeProfile<0, 2, [SDTCisInt<0>, SDTCisInt<1>]>;
def S1C33CmpCC : SDNode<"S1C33ISD::CMP", SDT_S1C33CmpCC,
    [SDNPOutGlue, SDNPCommutative]>;

// S1C33BrCC — conditional branch consuming the CMP flag.
// Operands: branch target (OtherVT), condition code (i32).
def SDT_S1C33BrCC : SDTypeProfile<0, 2,
    [SDTCisVT<0, OtherVT>, SDTCisVT<1, i32>]>;
def S1C33BrCC : SDNode<"S1C33ISD::BRCOND", SDT_S1C33BrCC,
    [SDNPHasChain, SDNPInGlue]>;

// S1C33Wrapper — wraps TargetGlobalAddress/TargetExternalSymbol so that
// address materialization patterns can match them as i32 value producers.
def SDT_S1C33Wrapper : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>, SDTCisPtrTy<0>]>;
def S1C33Wrapper : SDNode<"S1C33ISD::Wrapper", SDT_S1C33Wrapper>;

// S1C33SelectCC — conditional select (lowered to SELECT pseudo).
def SDT_S1C33SelectCC : SDTypeProfile<1, 5, [
    SDTCisVT<0, i32>, SDTCisVT<1, i32>, SDTCisVT<2, i32>,
    SDTCisVT<3, i32>, SDTCisVT<4, i32>, SDTCisVT<5, i32>]>;
def S1C33SelectCC : SDNode<"S1C33ISD::SELECT_CC", SDT_S1C33SelectCC>;

// Call sequence markers (ISD::CALLSEQ_START / CALLSEQ_END).
def SDT_S1C33CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                            SDTCisVT<1, i32>]>;
def SDT_S1C33CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                          SDTCisVT<1, i32>]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_S1C33CallSeqStart,
    [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_S1C33CallSeqEnd,
    [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// AsmOperandClass entries — provide range predicates for the AsmMatcher.
// Each class has a PredicateMethod that maps to a bool method on S1C33Operand.
//===----------------------------------------------------------------------===//

def UImm6AsmOperand : AsmOperandClass {
  let Name = "UImm6";
  let PredicateMethod = "isUImm6";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImm6";
}
def SImm6AsmOperand : AsmOperandClass {
  let Name = "SImm6";
  let PredicateMethod = "isSImm6";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidSImm6";
}
def UImm10AsmOperand : AsmOperandClass {
  let Name = "UImm10";
  let PredicateMethod = "isUImm10";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImm10";
}
def UImm13AsmOperand : AsmOperandClass {
  let Name = "UImm13";
  let PredicateMethod = "isUImm13";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImm13";
}
def UImm3AsmOperand : AsmOperandClass {
  let Name = "UImm3";
  let PredicateMethod = "isUImm3";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImm3";
}
def ShiftImm8AsmOperand : AsmOperandClass {
  let Name = "ShiftImm8";
  let PredicateMethod = "isShiftImm8";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidShiftImm8";
}

//===----------------------------------------------------------------------===//
// Immediate operand definitions
//===----------------------------------------------------------------------===//

// 6-bit unsigned immediate (add/sub imm6, SP-relative address)
def uimm6 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "decodeUimm6Operand";
  let ParserMatchClass = UImm6AsmOperand;
}

// 6-bit signed immediate (cmp/ld.w/and/or/xor/not sign6)
def simm6 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "decodeSimm6Operand";
  let ParserMatchClass = SImm6AsmOperand;
}

// 8-bit signed PC-relative offset (jp/call/jrXX sign8)
// Branch target = (instr_addr + 2) + 2*sign8
// No range check at parse time — resolved as fixup; labels are accepted freely.
def simm8 : Operand<OtherVT> {
  let OperandType = "OPERAND_PCREL";
  let DecoderMethod = "decodePCRelSimm8Operand";
}

// 10-bit unsigned immediate (add/sub %sp, imm10); byte count, 0..1023.
def uimm10 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "decodeUimm10Operand";
  let ParserMatchClass = UImm10AsmOperand;
}

// SP-relative memory offset — holds either a small immediate or a frame index.
// Frame indices are resolved to SP-relative immediates by eliminateFrameIndex.
def mem_sp : Operand<i32> {
  let OperandType = "OPERAND_MEMORY";
  let DecoderMethod = "decodeUimm6Operand";
  let ParserMatchClass = UImm6AsmOperand;
}

// 13-bit unsigned immediate (ext imm13)
def uimm13 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "decodeUimm13Operand";
  let ParserMatchClass = UImm13AsmOperand;
}

// Shift/rotate immediate: 1..8 (CPU manual: 0 is no-op; hardware value 1xxx=8).
// Encoded in a 4-bit field; assembler syntax accepts 1..8.
def shiftimm : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
  let DecoderMethod = "decodeUimm6Operand";  // reuse; actual field width is 4 bits
  let ParserMatchClass = ShiftImm8AsmOperand;
}

// 13-bit unsigned immediate for ext+[%rb] offset addressing (single ext, [0, 8191]).
// OPERAND_IMMEDIATE ensures the SelectionDAG ISel folds matching constants as TargetConstant
// (not materializing them as registers).
def simm13 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
}


//===----------------------------------------------------------------------===//
// Predicates for immediate values
//===----------------------------------------------------------------------===//

def immSExt6  : PatLeaf<(imm), [{ return isInt<6>(N->getSExtValue()); }]>;
// Signed 19-bit range that excludes signed 6-bit: values materializable with
// exactly one ext prefix (ext imm13 + ld.w sign6).  Used to route MOV_ri
// selection so 1-insn and 2-insn constants end up in distinct pseudos (the
// 2-insn form opts into rematerialization without being marked as
// as-cheap-as-a-move, unlike the 1-insn form).
def immSExt19NotSExt6 : PatLeaf<(imm), [{
  int64_t V = N->getSExtValue();
  return !isInt<6>(V) && isInt<19>(V);
}]>;
def immZExt6  : PatLeaf<(imm), [{ return isUInt<6>(N->getZExtValue()); }]>;
def immZExt10 : PatLeaf<(imm), [{ return isUInt<10>(N->getZExtValue()); }]>;
// 13-bit unsigned immediate for ext+[%rb] offset addressing (single ext, [0, 8191]).
// Class 1 memory instructions use unsigned immediates — ext offsets are zero-extended
// by the CPU.  Negative offsets are NOT supported (they become large positive values).
def immZExt13 : PatLeaf<(imm), [{ return isUInt<13>(N->getZExtValue()); }]>;
// 26-bit unsigned immediate for ext+ext+Class1 ALU 3-operand form.
// Excludes values that fit in 6-bit unsigned (handled by 2-operand *_ri).
def immZExt26Over63 : PatLeaf<(imm), [{
  uint64_t V = N->getZExtValue();
  return V > 63 && isUInt<26>(V);
}]>;
// Same but excluding signed 6-bit range (for and/or/xor which use signed imm6).
def immZExt26NotSExt6 : PatLeaf<(imm), [{
  int64_t S = N->getSExtValue();
  uint64_t V = N->getZExtValue();
  return !isInt<6>(S) && isUInt<26>(V);
}]>;
// Shift immediate: 1-8 (4-bit field; 0 is a no-op, 1xxx=8).
// Shifts cannot use ext prefix — max encodable count is 8.
def immShift8 : PatLeaf<(imm), [{ return N->getZExtValue() >= 1 && N->getZExtValue() <= 8; }]>;

// Negative immediate whose negation fits in unsigned 6-bit (1..63 → -1..-63).
// Used to match (add x, -N) → sub x, N.
def negImmToUImm6 : PatLeaf<(imm), [{
  int64_t V = N->getSExtValue();
  return V >= -63 && V <= -1;
}]>;

// SDNodeXForm: negate a constant (for negImmToUImm6 → uimm6 conversion).
def NegImm : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant((uint32_t)(-(uint32_t)N->getZExtValue()), SDLoc(N), MVT::i32);
}]>;

// SDNodeXForm: identity conversion of an imm to a fresh TargetConstant.
// Used in standalone Pat<> patterns to avoid shared Constant nodes getting replaced
// by a materialized register when the same constant is used elsewhere in the DAG.
def ImmAsTarget : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant((uint32_t)N->getZExtValue(), SDLoc(N), MVT::i32);
}]>;

//===----------------------------------------------------------------------===//
// CLASS 0 — No-operand instructions
//
// Encoding: |000|op1(4)|d(1)|op2(2)|0|0|0000|
//===----------------------------------------------------------------------===//

// nop  (op1=0000, d=0, op2=00) → 0x0000
def NOP : S1C33InstC0<0b0000, 0, 0b00, (outs), (ins), "nop", []> {
  let hasSideEffects = 0;
  let SchedRW = [WriteSys];
}

// slp — Sleep mode  (op1=0000, d=0, op2=01) → 0x0040
def SLP : S1C33InstC0<0b0000, 0, 0b01, (outs), (ins), "slp", []> {
  let hasSideEffects = 1;
  let SchedRW = [WriteSys];
}

// halt — HALT mode  (op1=0000, d=0, op2=10) → 0x0080
def HALT : S1C33InstC0<0b0000, 0, 0b10, (outs), (ins), "halt", []> {
  let hasSideEffects = 1;
  let SchedRW = [WriteSys];
}

// brk — Debug software exception  (op1=0010, d=0, op2=00) → 0x0400
def BRK : S1C33InstC0<0b0010, 0, 0b00, (outs), (ins), "brk", []> {
  let hasSideEffects = 1;
  let SchedRW = [WriteSys];
}

// retd — Return from debug  (op1=0010, d=0, op2=01) → 0x0440
def RETD : S1C33InstC0<0b0010, 0, 0b01, (outs), (ins), "retd", []> {
  let hasSideEffects = 1;
  let mayLoad      = 1;
  let isReturn     = 1;
  let isTerminator = 1;
  let isBarrier    = 1;
  let Defs         = [SP];
  let Uses         = [SP];
  let SchedRW      = [WriteRet];
}

// reti — Return from trap handler  (op1=0010, d=0, op2=11) → 0x04C0
def RETI : S1C33InstC0<0b0010, 0, 0b11, (outs), (ins), "reti",
    [(S1C33Reti)]> {
  let hasSideEffects = 1;
  let mayLoad      = 1;
  let isReturn     = 1;
  let isTerminator = 1;
  let isBarrier    = 1;
  let Defs         = [SP, PSR];
  let Uses         = [SP];
  let SchedRW      = [WriteRet];
}

// ret — Return from subroutine  (op1=0011, d=0, op2=01) → 0x0640
def RET : S1C33InstC0<0b0011, 0, 0b01, (outs), (ins), "ret",
    [(S1C33Ret)]> {
  let mayLoad      = 1;
  let isReturn     = 1;
  let isTerminator = 1;
  let isBarrier    = 1;
  let Defs         = [SP];
  let Uses         = [SP];
  let SchedRW      = [WriteRet];
}

// ret.d — Delayed return from subroutine  (op1=0011, d=1, op2=01) → 0x0660
// Delay slot: the instruction following ret.d executes before the return.
// Emitted by S1C33DelaySlotFiller in place of RET when a slot can be filled.
def RET_D : S1C33InstC0<0b0011, 1, 0b01, (outs), (ins), "ret.d", []> {
  let mayLoad       = 1;
  let isReturn      = 1;
  let isTerminator  = 1;
  let isBarrier     = 1;
  let hasDelaySlot  = 1;
  let Defs          = [SP];
  let Uses          = [SP];
  let SchedRW       = [WriteRet];
}

// call.d %rb — Delayed indirect call  (op1=0011, d=1, op2=00) → 0x0650|rb
// Emitted by S1C33DelaySlotFiller in place of CALL_r when a slot can be filled.
// NOTE: jp.d %rb is FORBIDDEN (hardware DMA bug). call.d %rb is safe.
def CALL_r_D : S1C33InstC0Reg<0b0011, 1, 0b00,
    (outs), (ins GR32:$rb), "call.d\t$rb", []> {
  let isCall         = 1;
  let hasDelaySlot   = 1;
  let hasSideEffects = 1;
  let Defs           = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses           = [SP];
  let SchedRW        = [WriteCall];
}

//===----------------------------------------------------------------------===//
// CLASS 0 — Register-operand instructions
//
// Encoding: |000|op1(4)|d(1)|op2(2)|0|0|rb(4)|
// The bit field in S1C33InstC0Reg is named "rb".
//===----------------------------------------------------------------------===//

// pushn %rb — Push R0–Rb  (op1=0001, d=0, op2=00) → 0x0200|rb
def PUSHN : S1C33InstC0Reg<0b0001, 0, 0b00,
    (outs), (ins GR32:$rb), "pushn\t$rb", []> {
  let hasSideEffects = 1;
  let mayStore       = 1;
  let Defs           = [SP];
  let Uses           = [SP];
  let SchedRW        = [WritePushPop];
}

// popn %rb — Pop Rb–R0  (op1=0001, d=0, op2=01) → 0x0240|rb
def POPN : S1C33InstC0Reg<0b0001, 0, 0b01,
    (outs GR32:$rb), (ins), "popn\t$rb", []> {
  let hasSideEffects = 1;
  let mayLoad        = 1;
  let Defs           = [SP];
  let Uses           = [SP];
  let SchedRW        = [WritePushPop];
}

// call %rb — Subroutine call via register  (op1=0011, d=0, op2=00) → 0x0600|rb
// Callee may clobber PSR (N/Z/V/C flags) and caller-saved registers.
def CALL_r : S1C33InstC0Reg<0b0011, 0, 0b00,
    (outs), (ins GR32:$rb), "call\t$rb", []> {
  let isCall         = 1;
  let hasSideEffects = 1;
  let Defs           = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses           = [SP];
  let SchedRW        = [WriteCall];
}

// jp %rb — Unconditional jump via register  (op1=0011, d=0, op2=10) → 0x0680|rb
// IMPORTANT: jp.d %rb (d=1) is FORBIDDEN — hardware DMA bug (see CLAUDE.md).
def JP_r : S1C33InstC0Reg<0b0011, 0, 0b10,
    (outs), (ins GR32:$rb), "jp\t$rb",
    [(brind GR32:$rb)]> {
  let isTerminator     = 1;
  let isBarrier        = 1;
  let isBranch         = 1;
  let isIndirectBranch = 1;
  let SchedRW          = [WriteJmp];
}

//===----------------------------------------------------------------------===//
// CLASS 0 — PC-relative branch instructions
//
// Encoding: |000|op1(4)|d(1)|sign8(8)|
// The bit field in S1C33InstC0Rel is named "imm8".
//===----------------------------------------------------------------------===//

// jp sign8 — Unconditional PC-relative jump  (op1=1111, d=0) → 0x1E00|sign8
def JP_i : S1C33InstC0Rel<0b1111, 0,
    (outs), (ins simm8:$imm8), "jp\t$imm8",
    [(br bb:$imm8)]> {
  let isTerminator = 1;
  let isBarrier    = 1;
  let isBranch     = 1;
  let SchedRW      = [WriteJmp];
}

// call sign8 — PC-relative subroutine call  (op1=1110, d=0) → 0x1C00|sign8
def CALL_i : S1C33InstC0Rel<0b1110, 0,
    (outs), (ins simm8:$imm8), "call\t$imm8", []> {
  let isCall         = 1;
  let hasSideEffects = 1;
  let Defs           = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses           = [SP];
  let SchedRW        = [WriteCall];
}

// call.d sign8 — Delayed PC-relative subroutine call  (op1=1110, d=1).
def CALL_D_i : S1C33InstC0Rel<0b1110, 1,
    (outs), (ins simm8:$imm8), "call.d\t$imm8", []> {
  let isCall         = 1;
  let hasDelaySlot   = 1;
  let hasSideEffects = 1;
  let Defs           = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses           = [SP];
  let SchedRW        = [WriteCall];
}

// CALL_sym — direct call to a named symbol.
// Shares the call encoding (op1=1110); the imm8 field holds a symbol reference
// rendered by the AsmPrinter and resolved as a PC-relative fixup.
// isCodeGenOnly: excluded from disassembler table (CALL_i covers decoding).
def CALL_sym : S1C33InstC0Rel<0b1110, 0,
    (outs), (ins i32imm:$imm8), "call\t$imm8", []> {
  let isCall = 1;
  let isCodeGenOnly = 1;
  let hasSideEffects = 1;
  // Clobbers: PSR flags, caller-saved R4–R7 and return-value regs R10/R11.
  let Defs = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses = [SP];
  let SchedRW = [WriteCall];
}

// Delayed direct call to a named symbol.
def CALL_D_sym : S1C33InstC0Rel<0b1110, 1,
    (outs), (ins i32imm:$imm8), "call.d\t$imm8", []> {
  let isCall         = 1;
  let hasDelaySlot   = 1;
  let isCodeGenOnly  = 1;
  let hasSideEffects = 1;
  let Defs           = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses           = [SP];
  let SchedRW        = [WriteCall];
}

// Patterns: S1C33Call → CALL_sym (direct) or CALL_r (indirect).
def : Pat<(S1C33Call tglobaladdr:$target),  (CALL_sym tglobaladdr:$target)>;
def : Pat<(S1C33Call texternalsym:$target), (CALL_sym texternalsym:$target)>;
def : Pat<(S1C33Call GR32:$target),         (CALL_r   GR32:$target)>;

// Conditional branches — all update PSR (read) / isBranch.
let isBranch = 1, isTerminator = 1, Uses = [PSR], SchedRW = [WriteJmpCond] in {
  // jrgt sign8  (op1=0100) — !Z & !(N^V)
  def JRGT  : S1C33InstC0Rel<0b0100, 0, (outs), (ins simm8:$imm8), "jrgt\t$imm8", []>;
  // jrge sign8  (op1=0101) — !(N^V)
  def JRGE  : S1C33InstC0Rel<0b0101, 0, (outs), (ins simm8:$imm8), "jrge\t$imm8", []>;
  // jrlt sign8  (op1=0110) — N^V
  def JRLT  : S1C33InstC0Rel<0b0110, 0, (outs), (ins simm8:$imm8), "jrlt\t$imm8", []>;
  // jrle sign8  (op1=0111) — Z | (N^V)
  def JRLE  : S1C33InstC0Rel<0b0111, 0, (outs), (ins simm8:$imm8), "jrle\t$imm8", []>;
  // jrugt sign8 (op1=1000) — !Z & !C
  def JRUGT : S1C33InstC0Rel<0b1000, 0, (outs), (ins simm8:$imm8), "jrugt\t$imm8", []>;
  // jruge sign8 (op1=1001) — !C
  def JRUGE : S1C33InstC0Rel<0b1001, 0, (outs), (ins simm8:$imm8), "jruge\t$imm8", []>;
  // jrult sign8 (op1=1010) — C
  def JRULT : S1C33InstC0Rel<0b1010, 0, (outs), (ins simm8:$imm8), "jrult\t$imm8", []>;
  // jrule sign8 (op1=1011) — Z | C
  def JRULE : S1C33InstC0Rel<0b1011, 0, (outs), (ins simm8:$imm8), "jrule\t$imm8", []>;
  // jreq sign8  (op1=1100) — Z
  def JREQ  : S1C33InstC0Rel<0b1100, 0, (outs), (ins simm8:$imm8), "jreq\t$imm8", []>;
  // jrne sign8  (op1=1101) — !Z
  def JRNE  : S1C33InstC0Rel<0b1101, 0, (outs), (ins simm8:$imm8), "jrne\t$imm8", []>;
}

// Delayed conditional branches — same encoding as above but d=1.
// The delay slot instruction executes before the branch is taken.
// Emitted by pp33/hand-written assembly; not generated by the backend directly.
// NOTE: jp.d %rb is FORBIDDEN (hardware DMA bug) but jp.d sign8 is fine.
let isBranch = 1, isTerminator = 1, Uses = [PSR], hasDelaySlot = 1,
    SchedRW = [WriteJmpCond] in {
  def JRGT_D  : S1C33InstC0Rel<0b0100, 1, (outs), (ins simm8:$imm8), "jrgt.d\t$imm8", []>;
  def JRGE_D  : S1C33InstC0Rel<0b0101, 1, (outs), (ins simm8:$imm8), "jrge.d\t$imm8", []>;
  def JRLT_D  : S1C33InstC0Rel<0b0110, 1, (outs), (ins simm8:$imm8), "jrlt.d\t$imm8", []>;
  def JRLE_D  : S1C33InstC0Rel<0b0111, 1, (outs), (ins simm8:$imm8), "jrle.d\t$imm8", []>;
  def JRUGT_D : S1C33InstC0Rel<0b1000, 1, (outs), (ins simm8:$imm8), "jrugt.d\t$imm8", []>;
  def JRUGE_D : S1C33InstC0Rel<0b1001, 1, (outs), (ins simm8:$imm8), "jruge.d\t$imm8", []>;
  def JRULT_D : S1C33InstC0Rel<0b1010, 1, (outs), (ins simm8:$imm8), "jrult.d\t$imm8", []>;
  def JRULE_D : S1C33InstC0Rel<0b1011, 1, (outs), (ins simm8:$imm8), "jrule.d\t$imm8", []>;
  def JREQ_D  : S1C33InstC0Rel<0b1100, 1, (outs), (ins simm8:$imm8), "jreq.d\t$imm8", []>;
  def JRNE_D  : S1C33InstC0Rel<0b1101, 1, (outs), (ins simm8:$imm8), "jrne.d\t$imm8", []>;
}

// Delayed unconditional branch (jp.d sign8 — safe variant; jp.d %rb is forbidden)
def JP_D_i : S1C33InstC0Rel<0b1111, 1, (outs), (ins simm8:$imm8), "jp.d\t$imm8", []> {
  let isTerminator = 1; let isBarrier = 1; let isBranch = 1; let hasDelaySlot = 1;
  let SchedRW = [WriteJmp];
}

// Patterns: S1C33BrCC(dest, S1C33CC) → appropriate conditional jump.
// S1C33CC values: EQ=0, NE=1, LT=2, LE=3, GT=4, GE=5, ULT=6, ULE=7, UGT=8, UGE=9
def : Pat<(S1C33BrCC bb:$dst, (i32 0)), (JREQ  bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 1)), (JRNE  bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 2)), (JRLT  bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 3)), (JRLE  bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 4)), (JRGT  bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 5)), (JRGE  bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 6)), (JRULT bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 7)), (JRULE bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 8)), (JRUGT bb:$dst)>;
def : Pat<(S1C33BrCC bb:$dst, (i32 9)), (JRUGE bb:$dst)>;

// 4-byte relaxed forms: ext + branch/call (for targets beyond sign8 range).
// Generated by S1C33AsmBackend::relaxInstruction(); not emitted by initial codegen.
// encodeInstruction() emits: ext word (0xC000|imm13) + branch word (base|sign8).
let isBranch = 1, isTerminator = 1, Uses = [PSR], SchedRW = [WriteJmpCond] in {
  def JRGT_EXT1  : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrgt\t$imm8", []>;
  def JRGE_EXT1  : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrge\t$imm8", []>;
  def JRLT_EXT1  : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrlt\t$imm8", []>;
  def JRLE_EXT1  : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrle\t$imm8", []>;
  def JRUGT_EXT1 : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrugt\t$imm8", []>;
  def JRUGE_EXT1 : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jruge\t$imm8", []>;
  def JRULT_EXT1 : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrult\t$imm8", []>;
  def JRULE_EXT1 : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrule\t$imm8", []>;
  def JREQ_EXT1  : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jreq\t$imm8", []>;
  def JRNE_EXT1  : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jrne\t$imm8", []>;
}
def JP_EXT1 : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jp\t$imm8", []> {
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1;
  let SchedRW = [WriteJmp];
}
// Delayed unconditional branch, long-range relaxed form (ext + jp.d imm8).
def JP_D_EXT1 : S1C33PseudoExt1<(outs), (ins simm8:$imm8), "jp.d\t$imm8", []> {
  let isBranch = 1; let isTerminator = 1; let isBarrier = 1; let hasDelaySlot = 1;
  let SchedRW = [WriteJmp];
}
def CALL_EXT1 : S1C33PseudoExt1<(outs), (ins i32imm:$imm8), "call\t$imm8", []> {
  let isCall = 1;
  let Defs = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses = [SP];
  let SchedRW = [WriteCall];
}
def CALL_D_EXT1 : S1C33PseudoExt1<(outs), (ins i32imm:$imm8), "call.d\t$imm8", []> {
  let isCall = 1;
  let hasDelaySlot = 1;
  let Defs = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses = [SP];
  let SchedRW = [WriteCall];
}
def CALL_EXT2 : S1C33PseudoAddrN<6, (outs), (ins i32imm:$imm8), "call\t$imm8", []> {
  let isCall = 1;
  let Defs = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses = [SP];
  let SchedRW = [WriteCall];
}
def CALL_D_EXT2 : S1C33PseudoAddrN<6, (outs), (ins i32imm:$imm8), "call.d\t$imm8", []> {
  let isCall = 1;
  let hasDelaySlot = 1;
  let Defs = [SP, PSR, R4, R5, R6, R7, R10, R11];
  let Uses = [SP];
  let SchedRW = [WriteCall];
}

//===----------------------------------------------------------------------===//
// Global address materialization: LDW_SYM_EXT0/EXT1/EXT2
//
// Materializes the absolute address of a global symbol into a register.
//   EXT0 (2 bytes): ld.w %rd, sym@l           (address fits in 6-bit signed)
//   EXT1 (4 bytes): ext sym@m; ld.w %rd, sym@l (address fits in 19-bit signed)
//   EXT2 (6 bytes): ext sym@h; ext sym@m; ld.w %rd, sym@l (full 32-bit)
//
// The MC relaxation layer starts at EXT0 and grows as needed.
//===----------------------------------------------------------------------===//

// ld.w %rd, imm6 (immediate load) clobbers PSR N/Z flags.
let Defs = [PSR], SchedRW = [WriteALU] in {
def LDW_SYM_EXT0 : S1C33PseudoAddrN<2, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.w\t$rd, $sym", []>;
def LDW_SYM_EXT1 : S1C33PseudoAddrN<4, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.w\t$rd, $sym", []>;
def LDW_SYM_EXT2 : S1C33PseudoAddrN<6, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.w\t$rd, $sym", []>;
}

//===----------------------------------------------------------------------===//
// R8-absolute load/store pseudos
//
// Expanded by MCCodeEmitter into:
//   ext  sym@ah           ; bits[25:13]
//   ext  sym@al           ; bits[12:0]
//   ld.* $rd, [%r8]       ; or ld.* [%r8], $rs  — R8 is always 0
//
// Always 6 bytes, no relaxation.  Selected by ISelDAGToDAG only when the
// S1C33ISD::Wrapper around the global address has a single use (otherwise
// the existing LDW_SYM_EXT2 materialization path is more compact).
//
// Class 1 memory loads/stores do not update PSR, so no Defs=[PSR].  The
// address is a 26-bit absolute value; AsmBackend reports an error if the
// symbol resolves outside that range.  Compatible with gcc33's @ah/@al.
//===----------------------------------------------------------------------===//

let mayLoad = 1, SchedRW = [WriteLd] in {
def LDB_ABS  : S1C33PseudoAddrN<6, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.b\t$rd, $sym",  []>;
def LDUB_ABS : S1C33PseudoAddrN<6, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.ub\t$rd, $sym", []>;
def LDH_ABS  : S1C33PseudoAddrN<6, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.h\t$rd, $sym",  []>;
def LDUH_ABS : S1C33PseudoAddrN<6, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.uh\t$rd, $sym", []>;
def LDW_ABS  : S1C33PseudoAddrN<6, (outs GR32:$rd), (ins i32imm:$sym),
    "ld.w\t$rd, $sym",  []>;
}

let mayStore = 1, SchedRW = [WriteALU] in {
def STB_ABS : S1C33PseudoAddrN<6, (outs), (ins i32imm:$sym, GR32:$rs),
    "ld.b\t$sym, $rs", []>;
def STH_ABS : S1C33PseudoAddrN<6, (outs), (ins i32imm:$sym, GR32:$rs),
    "ld.h\t$sym, $rs", []>;
def STW_ABS : S1C33PseudoAddrN<6, (outs), (ins i32imm:$sym, GR32:$rs),
    "ld.w\t$sym, $rs", []>;
}

// int imm2 — Software exception  (op1=0010, d=0, op2=10) → 0x0480|imm2
def INT : S1C33InstC0Imm2<0b0010, 0, 0b10,
    (outs), (ins i32imm:$imm2), "int\t$imm2", []> {
  let hasSideEffects = 1;
  let mayStore      = 1;
  let isTerminator  = 1;
  let isBarrier     = 1;
  let Defs          = [SP];
  let Uses          = [SP, PSR];
  let SchedRW       = [WriteSys];
}

//===----------------------------------------------------------------------===//
// CLASS 1 — Register-indirect memory transfers
//
// Load format:  |001|op1(3)|00(2)|rb(4)|rd(4)|
// Store format: same, rd field holds the source register
//
// Bit fields: "rb" (address reg), "rd" (data reg).
//===----------------------------------------------------------------------===//

// Loads — op2=00
let mayLoad = 1, hasSideEffects = 0, SchedRW = [WriteLd] in {
  // ld.b %rd,[%rb]   (op1=000) → 0x2000|(rb<<4)|rd
  def LDB_ri  : S1C33InstC1Mem<0b000, 0b00,
      (outs GR32:$rd), (ins GR32:$rb),
      "ld.b\t$rd, [$rb]",
      [(set GR32:$rd, (sextloadi8 GR32:$rb))]>;

  // ld.ub %rd,[%rb]  (op1=001) → 0x2400|(rb<<4)|rd
  def LDUB_ri : S1C33InstC1Mem<0b001, 0b00,
      (outs GR32:$rd), (ins GR32:$rb),
      "ld.ub\t$rd, [$rb]",
      [(set GR32:$rd, (zextloadi8 GR32:$rb))]>;

  // ld.h %rd,[%rb]   (op1=010) → 0x2800|(rb<<4)|rd
  def LDH_ri  : S1C33InstC1Mem<0b010, 0b00,
      (outs GR32:$rd), (ins GR32:$rb),
      "ld.h\t$rd, [$rb]",
      [(set GR32:$rd, (sextloadi16 GR32:$rb))]>;

  // ld.uh %rd,[%rb]  (op1=011) → 0x2C00|(rb<<4)|rd
  def LDUH_ri : S1C33InstC1Mem<0b011, 0b00,
      (outs GR32:$rd), (ins GR32:$rb),
      "ld.uh\t$rd, [$rb]",
      [(set GR32:$rd, (zextloadi16 GR32:$rb))]>;

  // ld.w %rd,[%rb]   (op1=100) → 0x3000|(rb<<4)|rd
  def LDW_ri  : S1C33InstC1Mem<0b100, 0b00,
      (outs GR32:$rd), (ins GR32:$rb),
      "ld.w\t$rd, [$rb]",
      [(set GR32:$rd, (load GR32:$rb))]>;
}

// Any-extending loads (extloadi8/16): use unsigned (zero-extending) loads.
// ISD::EXTLOAD means "any valid extension is acceptable"; LDUB/LDUH are correct.
def : Pat<(i32 (extloadi8  GR32:$rb)), (LDUB_ri GR32:$rb)>;
def : Pat<(i32 (extloadi16 GR32:$rb)), (LDUH_ri GR32:$rb)>;

// i1 (bool) loads: a bool occupies one byte in memory; treat as byte load.
def : Pat<(i32 (zextloadi1 GR32:$rb)), (LDUB_ri GR32:$rb)>;
def : Pat<(i32 (extloadi1  GR32:$rb)), (LDUB_ri GR32:$rb)>;

// Stores — op2=00; rd field holds the source register
let mayStore = 1, hasSideEffects = 0, SchedRW = [WriteALU] in {
  // ld.b [%rb],%rd  byte store  (op1=101) → 0x3400|(rb<<4)|rd
  def STB_ri : S1C33InstC1Mem<0b101, 0b00,
      (outs), (ins GR32:$rb, GR32:$rd),
      "ld.b\t[$rb], $rd",
      [(truncstorei8 GR32:$rd, GR32:$rb)]>;

  // ld.h [%rb],%rd  halfword store  (op1=110) → 0x3800|(rb<<4)|rd
  def STH_ri : S1C33InstC1Mem<0b110, 0b00,
      (outs), (ins GR32:$rb, GR32:$rd),
      "ld.h\t[$rb], $rd",
      [(truncstorei16 GR32:$rd, GR32:$rb)]>;

  // ld.w [%rb],%rd  word store  (op1=111) → 0x3C00|(rb<<4)|rd
  def STW_ri : S1C33InstC1Mem<0b111, 0b00,
      (outs), (ins GR32:$rb, GR32:$rd),
      "ld.w\t[$rb], $rd",
      [(store GR32:$rd, GR32:$rb)]>;
}

// Post-increment store — op2=01; address register is incremented after store.
// CPU Manual §4.3: ld.X [%rb]+, %rd  → store rd at [rb], then rb += sizeof(X)
// Selected from IndexedStoreSDNode by tryIndexedStore in S1C33ISelDAGToDAG.cpp
// (no TableGen pattern — manual selection avoids PatFrag complexity since all
// S1C33 post-inc increments are fixed to the transfer size).
let mayStore = 1, hasSideEffects = 1, SchedRW = [WriteALU] in {
  // ld.h [%rb]+, %rd  halfword post-increment store  (op1=110, op2=01)
  // → 0x3900|(rb<<4)|rd;  rb += 2 after store
  def STH_ri_pi : S1C33InstC1Mem<0b110, 0b01,
      (outs GR32:$rb_out), (ins GR32:$rb, GR32:$rd),
      "ld.h\t[$rb]+, $rd", []> {
    let Constraints = "$rb_out = $rb";
  }

  // ld.w [%rb]+, %rd  word post-increment store  (op1=111, op2=01)
  // → 0x3D00|(rb<<4)|rd;  rb += 4 after store
  def STW_ri_pi : S1C33InstC1Mem<0b111, 0b01,
      (outs GR32:$rb_out), (ins GR32:$rb, GR32:$rd),
      "ld.w\t[$rb]+, $rd", []> {
    let Constraints = "$rb_out = $rb";
  }

  // ld.b [%rb]+, %rd  byte post-increment store  (op1=101, op2=01)
  // → 0x3500|(rb<<4)|rd;  rb += 1 after store
  def STB_ri_pi : S1C33InstC1Mem<0b101, 0b01,
      (outs GR32:$rb_out), (ins GR32:$rb, GR32:$rd),
      "ld.b\t[$rb]+, $rd", []> {
    let Constraints = "$rb_out = $rb";
  }
}

// Post-increment loads — op2=01; data register is read, address register incremented.
// CPU Manual §4.3: ld.X %rd, [%rb]+  → load [rb] into rd, then rb += sizeof(X)
// The notation may seem like a store but it IS a load: rd=dest, rb=addr
// (op1=000..100 correspond to b/ub/h/uh/w same as plain loads)
// Selected from IndexedLoadSDNode by tryIndexedLoad in S1C33ISelDAGToDAG.cpp
// (no TableGen pattern — LLVM has no post_load PatFrag, so manual selection
// is the idiomatic approach, mirroring MSP430/PPC/ARM).
// Two SchedWrites: WriteLdPI for the loaded value (latency 2 like a normal
// load), WriteALU for the post-incremented base register (1 cycle).  Both
// defs must have a corresponding SchedWrite or CompleteModel=1 trips a
// "DefIdx exceeds machine model writes" assertion in TargetSchedule.
let mayLoad = 1, hasSideEffects = 1, SchedRW = [WriteLdPI, WriteALU] in {
  // ld.b %rd, [%rb]+   byte post-increment load  (op1=000, op2=01)
  def LDB_ri_pi  : S1C33InstC1Mem<0b000, 0b01,
      (outs GR32:$rd, GR32:$rb_out), (ins GR32:$rb),
      "ld.b\t$rd, [$rb]+", []> {
    let Constraints = "$rb_out = $rb";
  }
  // ld.ub %rd, [%rb]+  byte unsigned post-increment load  (op1=001, op2=01)
  def LDUB_ri_pi : S1C33InstC1Mem<0b001, 0b01,
      (outs GR32:$rd, GR32:$rb_out), (ins GR32:$rb),
      "ld.ub\t$rd, [$rb]+", []> {
    let Constraints = "$rb_out = $rb";
  }
  // ld.h %rd, [%rb]+   halfword post-increment load  (op1=010, op2=01)
  def LDH_ri_pi  : S1C33InstC1Mem<0b010, 0b01,
      (outs GR32:$rd, GR32:$rb_out), (ins GR32:$rb),
      "ld.h\t$rd, [$rb]+", []> {
    let Constraints = "$rb_out = $rb";
  }
  // ld.uh %rd, [%rb]+  halfword unsigned post-increment load  (op1=011, op2=01)
  def LDUH_ri_pi : S1C33InstC1Mem<0b011, 0b01,
      (outs GR32:$rd, GR32:$rb_out), (ins GR32:$rb),
      "ld.uh\t$rd, [$rb]+", []> {
    let Constraints = "$rb_out = $rb";
  }
  // ld.w %rd, [%rb]+   word post-increment load  (op1=100, op2=01)
  def LDW_ri_pi  : S1C33InstC1Mem<0b100, 0b01,
      (outs GR32:$rd, GR32:$rb_out), (ins GR32:$rb),
      "ld.w\t$rd, [$rb]+", []> {
    let Constraints = "$rb_out = $rb";
  }
}

//===----------------------------------------------------------------------===//
// Register-indirect with offset (ext+[%rb]) pseudos.
//
// Each pseudo expands in expandPostRAPseudo to: ext $off; ld.x [$rb].
// The offset is 13-bit unsigned (single ext), covering [0, 8191] bytes.
// Class 1 memory instructions use unsigned immediates — the CPU zero-extends
// the ext-provided offset.  Negative offsets are NOT supported.
// DAG patterns are added at the bottom of this file.
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 1, SchedRW = [WriteLd] in {
  def LDB_ri_off  : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rb, simm13:$off),
      "# ld.b\t$rd, [$rb+$off]",
      [(set GR32:$rd, (sextloadi8  (add GR32:$rb, immZExt13:$off)))]>;
  def LDUB_ri_off : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rb, simm13:$off),
      "# ld.ub\t$rd, [$rb+$off]",
      [(set GR32:$rd, (zextloadi8  (add GR32:$rb, immZExt13:$off)))]>;
  def LDH_ri_off  : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rb, simm13:$off),
      "# ld.h\t$rd, [$rb+$off]",
      [(set GR32:$rd, (sextloadi16 (add GR32:$rb, immZExt13:$off)))]>;
  def LDUH_ri_off : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rb, simm13:$off),
      "# ld.uh\t$rd, [$rb+$off]",
      [(set GR32:$rd, (zextloadi16 (add GR32:$rb, immZExt13:$off)))]>;
  def LDW_ri_off  : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rb, simm13:$off),
      "# ld.w\t$rd, [$rb+$off]",
      [(set GR32:$rd, (load         (add GR32:$rb, immZExt13:$off)))]>;
}
let hasSideEffects = 0, mayStore = 1, SchedRW = [WriteALU] in {
  def STB_ri_off  : S1C33Pseudo<(outs), (ins GR32:$rb, simm13:$off, GR32:$rs),
      "# ld.b\t[$rb+$off], $rs",
      [(truncstorei8  GR32:$rs, (add GR32:$rb, immZExt13:$off))]>;
  def STH_ri_off  : S1C33Pseudo<(outs), (ins GR32:$rb, simm13:$off, GR32:$rs),
      "# ld.h\t[$rb+$off], $rs",
      [(truncstorei16 GR32:$rs, (add GR32:$rb, immZExt13:$off))]>;
  def STW_ri_off  : S1C33Pseudo<(outs), (ins GR32:$rb, simm13:$off, GR32:$rs),
      "# ld.w\t[$rb+$off], $rs",
      [(store         GR32:$rs, (add GR32:$rb, immZExt13:$off))]>;
}

// SP-relative load/store whose byte displacement exceeds the scaled-imm6
// encoding of the Class 2 instructions.  Created only by
// eliminateFrameIndex (never selected).  The full byte offset rides inside
// one pseudo so the eventual ext+instruction pair stays fused while the
// post-RA scheduler runs — a bare EXT emitted at PEI time has no
// dependency edges and could be separated from its target.
// ExpandExtPseudos splits the pseudo into one or two EXT prefixes plus the
// real *_sp instruction after all scheduling.  $off is an unsigned raw
// byte displacement (NOT scaled, unlike the plain *_sp imm6 field).
let hasSideEffects = 0, mayLoad = 1, SchedRW = [WriteLd] in {
  def LDB_sp_off  : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$off),
      "# ld.b\t$rd, [%sp+$off]", []>;
  def LDUB_sp_off : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$off),
      "# ld.ub\t$rd, [%sp+$off]", []>;
  def LDH_sp_off  : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$off),
      "# ld.h\t$rd, [%sp+$off]", []>;
  def LDUH_sp_off : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$off),
      "# ld.uh\t$rd, [%sp+$off]", []>;
  def LDW_sp_off  : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$off),
      "# ld.w\t$rd, [%sp+$off]", []>;
}
let hasSideEffects = 0, mayStore = 1, SchedRW = [WriteALU] in {
  def STB_sp_off  : S1C33Pseudo<(outs), (ins i32imm:$off, GR32:$rs),
      "# ld.b\t[%sp+$off], $rs", []>;
  def STH_sp_off  : S1C33Pseudo<(outs), (ins i32imm:$off, GR32:$rs),
      "# ld.h\t[%sp+$off], $rs", []>;
  def STW_sp_off  : S1C33Pseudo<(outs), (ins i32imm:$off, GR32:$rs),
      "# ld.w\t[%sp+$off], $rs", []>;
}

//===----------------------------------------------------------------------===//
// CLASS 1 — Register-register ALU
//
// Encoding: |001|op1(3)|10(2)|rs(4)|rd(4)|
// Bit fields: "rs" (source), "rd" (destination).
// All ops are destructive 2-operand (rd = rd OP rs), except:
//   - cmp (no dest register), not (rd = ~rs, non-destructive), ld.w (copy).
// All ALU ops update PSR flags; ld.w (plain copy) does not.
//===----------------------------------------------------------------------===//

// 2-address destructive: rd = rd OP rs
// Use Constraints "$rd = $rs1" to tie destination to first source.
// The encoder maps $rd→bits<4>rd, $rs→bits<4>rs; $rs1 is the tied-to operand.
let Defs = [PSR], hasSideEffects = 0, SchedRW = [WriteALU] in {
  let Constraints = "$rd = $rs1" in {
    // add %rd, %rs  (op1=000) → 0x2200|(rs<<4)|rd
    def ADD_rr : S1C33InstC1ALU<0b000,
        (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
        "add\t$rd, $rs",
        [(set GR32:$rd, (add GR32:$rs1, GR32:$rs))]>;

    // sub %rd, %rs  (op1=001) → 0x2600|(rs<<4)|rd
    def SUB_rr : S1C33InstC1ALU<0b001,
        (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
        "sub\t$rd, $rs",
        [(set GR32:$rd, (sub GR32:$rs1, GR32:$rs))]>;

    // and %rd, %rs  (op1=100) → 0x3200|(rs<<4)|rd
    def AND_rr : S1C33InstC1ALU<0b100,
        (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
        "and\t$rd, $rs",
        [(set GR32:$rd, (and GR32:$rs1, GR32:$rs))]>;

    // or %rd, %rs   (op1=101) → 0x3600|(rs<<4)|rd
    def OR_rr  : S1C33InstC1ALU<0b101,
        (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
        "or\t$rd, $rs",
        [(set GR32:$rd, (or GR32:$rs1, GR32:$rs))]>;

    // xor %rd, %rs  (op1=110) → 0x3A00|(rs<<4)|rd
    def XOR_rr : S1C33InstC1ALU<0b110,
        (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
        "xor\t$rd, $rs",
        [(set GR32:$rd, (xor GR32:$rs1, GR32:$rs))]>;
  }

  // cmp %rd, %rs  (op1=010) — flags only, no register destination
  // → 0x2A00|(rs<<4)|rd
  def CMP_rr : S1C33InstC1ALU<0b010,
      (outs), (ins GR32:$rd, GR32:$rs),
      "cmp\t$rd, $rs",
      [(S1C33CmpCC GR32:$rd, GR32:$rs)]>;

  // not %rd, %rs  (op1=111) — rd = ~rs (NOT destructive: rs is source)
  // → 0x3E00|(rs<<4)|rd
  def NOT_rr : S1C33InstC1ALU<0b111,
      (outs GR32:$rd), (ins GR32:$rs),
      "not\t$rd, $rs",
      [(set GR32:$rd, (not GR32:$rs))]>;
}

// ld.w %rd, %rs  — register copy  (op1=011) → 0x2E00|(rs<<4)|rd
// No PSR update. Used by copyPhysReg; no DAG pattern (LLVM handles COPY internally).
def MOV_rr : S1C33InstC1ALU<0b011,
    (outs GR32:$rd), (ins GR32:$rs),
    "ld.w\t$rd, $rs", []> {
  let hasSideEffects = 0;
  let SchedRW = [WriteALU];
}

//===----------------------------------------------------------------------===//
// CLASS 1 ALU — ext + 3-operand form (untied real instructions)
//
// ext imm / op %rd, %rs → rd = rs <op> zero_ext(imm)
// Same encoding as the 2-operand *_rr forms above, but WITHOUT the
// Constraints = "$rd = $rs1" tie.  The CPU interprets the presence of ext
// as switching to 3-operand mode.
//
// These are NOT selected directly by ISel — they are emitted only by
// ExpandExtPseudos when expanding *_rri pseudos below.
//===----------------------------------------------------------------------===//
let Defs = [PSR], hasSideEffects = 0, SchedRW = [WriteALU], isCodeGenOnly = 1 in {
  def ADD_rr_ext : S1C33InstC1ALU<0b000,
      (outs GR32:$rd), (ins GR32:$rs), "add\t$rd, $rs", []>;
  def SUB_rr_ext : S1C33InstC1ALU<0b001,
      (outs GR32:$rd), (ins GR32:$rs), "sub\t$rd, $rs", []>;
  def AND_rr_ext : S1C33InstC1ALU<0b100,
      (outs GR32:$rd), (ins GR32:$rs), "and\t$rd, $rs", []>;
  def OR_rr_ext  : S1C33InstC1ALU<0b101,
      (outs GR32:$rd), (ins GR32:$rs), "or\t$rd, $rs", []>;
  def XOR_rr_ext : S1C33InstC1ALU<0b110,
      (outs GR32:$rd), (ins GR32:$rs), "xor\t$rd, $rs", []>;
}

//===----------------------------------------------------------------------===//
// 3-operand ALU pseudos — ext imm / op %rd, %rs
//
// Expanded by ExpandExtPseudos to ext(s) + *_rr_ext.
// The immediate is UNSIGNED (zero-extended by ext).
// For ADD/SUB: excludes immZExt6 range (handled by 2-operand *_ri).
// For AND/OR/XOR: excludes immSExt6 range (handled by 2-operand *_ri).
//===----------------------------------------------------------------------===//
let Defs = [PSR], AddedComplexity = 5, SchedRW = [WriteALU] in {
  def ADD_rri : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm),
      "# ADD_rri $rd, $rs, $imm",
      [(set GR32:$rd, (add GR32:$rs, immZExt26Over63:$imm))]>;
  def SUB_rri : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm),
      "# SUB_rri $rd, $rs, $imm",
      [(set GR32:$rd, (sub GR32:$rs, immZExt26Over63:$imm))]>;
  def AND_rri : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm),
      "# AND_rri $rd, $rs, $imm",
      [(set GR32:$rd, (and GR32:$rs, immZExt26NotSExt6:$imm))]>;
  def OR_rri  : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm),
      "# OR_rri $rd, $rs, $imm",
      [(set GR32:$rd, (or  GR32:$rs, immZExt26NotSExt6:$imm))]>;
  def XOR_rri : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm),
      "# XOR_rri $rd, $rs, $imm",
      [(set GR32:$rd, (xor GR32:$rs, immZExt26NotSExt6:$imm))]>;
}

//===----------------------------------------------------------------------===//
// CLASS 2 — SP-relative displacement memory
//
// Encoding: |010|op1(3)|imm6(6)|rd(4)|
// Bit fields: "imm6" (offset), "rd" (data register).
// imm6 is unsigned; byte address = SP + imm6.
//===----------------------------------------------------------------------===//

let hasSideEffects = 0 in {
  let mayLoad = 1, SchedRW = [WriteLd] in {
    // ld.b %rd,[%sp+imm6]   (op1=000)
    def LDB_sp  : S1C33InstC2<0b000,
        (outs GR32:$rd), (ins mem_sp:$imm6),
        "ld.b\t$rd, [%sp+$imm6]", []>;
    // ld.ub %rd,[%sp+imm6]  (op1=001)
    def LDUB_sp : S1C33InstC2<0b001,
        (outs GR32:$rd), (ins mem_sp:$imm6),
        "ld.ub\t$rd, [%sp+$imm6]", []>;
    // ld.h %rd,[%sp+imm6]   (op1=010)
    def LDH_sp  : S1C33InstC2<0b010,
        (outs GR32:$rd), (ins mem_sp:$imm6),
        "ld.h\t$rd, [%sp+$imm6]", []>;
    // ld.uh %rd,[%sp+imm6]  (op1=011)
    def LDUH_sp : S1C33InstC2<0b011,
        (outs GR32:$rd), (ins mem_sp:$imm6),
        "ld.uh\t$rd, [%sp+$imm6]", []>;
    // ld.w %rd,[%sp+imm6]   (op1=100)
    def LDW_sp  : S1C33InstC2<0b100,
        (outs GR32:$rd), (ins mem_sp:$imm6),
        "ld.w\t$rd, [%sp+$imm6]", []>;
  }
  let mayStore = 1, SchedRW = [WriteALU] in {
    // ld.b [%sp+imm6],%rd   (op1=101)
    def STB_sp  : S1C33InstC2<0b101,
        (outs), (ins mem_sp:$imm6, GR32:$rd),
        "ld.b\t[%sp+$imm6], $rd", []>;
    // ld.h [%sp+imm6],%rd   (op1=110)
    def STH_sp  : S1C33InstC2<0b110,
        (outs), (ins mem_sp:$imm6, GR32:$rd),
        "ld.h\t[%sp+$imm6], $rd", []>;
    // ld.w [%sp+imm6],%rd   (op1=111)
    def STW_sp  : S1C33InstC2<0b111,
        (outs), (ins mem_sp:$imm6, GR32:$rd),
        "ld.w\t[%sp+$imm6], $rd", []>;
  }
}

// Patterns for SP-relative frame index loads/stores.
// Frame indices are resolved to SP-relative offsets by eliminateFrameIndex.
def : Pat<(i32 (load          (frameindex:$fi))), (LDW_sp  $fi)>;
def : Pat<(i32 (sextloadi8    (frameindex:$fi))), (LDB_sp  $fi)>;
def : Pat<(i32 (zextloadi8    (frameindex:$fi))), (LDUB_sp $fi)>;
def : Pat<(i32 (sextloadi16   (frameindex:$fi))), (LDH_sp  $fi)>;
def : Pat<(i32 (zextloadi16   (frameindex:$fi))), (LDUH_sp $fi)>;
def : Pat<(i32 (zextloadi1    (frameindex:$fi))), (LDUB_sp $fi)>;
def : Pat<(i32 (extloadi1     (frameindex:$fi))), (LDUB_sp $fi)>;
def : Pat<(store     GR32:$src, (frameindex:$fi)), (STW_sp $fi, GR32:$src)>;
def : Pat<(truncstorei8  GR32:$src, (frameindex:$fi)), (STB_sp $fi, GR32:$src)>;
def : Pat<(truncstorei16 GR32:$src, (frameindex:$fi)), (STH_sp $fi, GR32:$src)>;


//===----------------------------------------------------------------------===//
// CLASS 3 — 6-bit immediate ALU
//
// Encoding: |011|op1(3)|imm6(6)|rd(4)|
// Bit fields: "imm6" (immediate), "rd" (register).
// add/sub: imm6 is unsigned; cmp/ld.w/and/or/xor: sign6 is signed.
// 2-address for add/sub/and/or/xor: rd = rd OP imm6.
//===----------------------------------------------------------------------===//

// 2-address immediate ALU: rd = rd OP imm6/sign6
// Constraints: $rd = $rs (rd is tied to the source register).
let Defs = [PSR], hasSideEffects = 0, SchedRW = [WriteALU] in {
  let Constraints = "$rd = $rs" in {
    // add %rd, imm6   (op1=000) → 0x6000|(imm6<<4)|rd
    def ADD_ri : S1C33InstC3<0b000,
        (outs GR32:$rd), (ins GR32:$rs, uimm6:$imm6),
        "add\t$rd, $imm6",
        [(set GR32:$rd, (add GR32:$rs, immZExt6:$imm6))]>;

    // sub %rd, imm6   (op1=001) → 0x6400|(imm6<<4)|rd
    def SUB_ri : S1C33InstC3<0b001,
        (outs GR32:$rd), (ins GR32:$rs, uimm6:$imm6),
        "sub\t$rd, $imm6",
        [(set GR32:$rd, (sub GR32:$rs, immZExt6:$imm6))]>;

    // and %rd, sign6  (op1=100) → 0x7000|(sign6<<4)|rd
    def AND_ri : S1C33InstC3<0b100,
        (outs GR32:$rd), (ins GR32:$rs, simm6:$imm6),
        "and\t$rd, $imm6",
        [(set GR32:$rd, (and GR32:$rs, immSExt6:$imm6))]>;

    // or %rd, sign6   (op1=101) → 0x7400|(sign6<<4)|rd
    def OR_ri  : S1C33InstC3<0b101,
        (outs GR32:$rd), (ins GR32:$rs, simm6:$imm6),
        "or\t$rd, $imm6",
        [(set GR32:$rd, (or GR32:$rs, immSExt6:$imm6))]>;

    // xor %rd, sign6  (op1=110) → 0x7800|(sign6<<4)|rd
    def XOR_ri : S1C33InstC3<0b110,
        (outs GR32:$rd), (ins GR32:$rs, simm6:$imm6),
        "xor\t$rd, $imm6",
        [(set GR32:$rd, (xor GR32:$rs, immSExt6:$imm6))]>;
  }

  // cmp %rd, sign6  (op1=010) — flags only
  def CMP_ri : S1C33InstC3<0b010,
      (outs), (ins GR32:$rd, simm6:$imm6),
      "cmp\t$rd, $imm6",
      [(S1C33CmpCC GR32:$rd, immSExt6:$imm6)]>;
}

// ld.w %rd, sign6 — immediate load  (op1=011) → 0x6C00|(sign6<<4)|rd
// PSR updated (N/Z per CPU manual).
// Operand is i32imm (not simm6) so that expandPostRAPseudo can pass the
// full intended value V; the MC encoder uses V & 0x3F for the 6-bit field.
//
// isReMaterializable + isAsCheapAsAMove tell the register allocator that
// a 6-bit immediate is as cheap to produce as a register-register copy.
// Without these hints the allocator spills 1-insn constants to the stack
// and generates store+reload pairs where rematerialization at the use site
// is strictly cheaper (same dynamic count, no stack slot needed).
// PSR liveness across the rematerialized def is tracked through Defs=[PSR].
def MOV_ri6 : S1C33InstC3<0b011,
    (outs GR32:$rd), (ins simm6:$imm6),
    "ld.w\t$rd, $imm6",
    [(set GR32:$rd, immSExt6:$imm6)]> {
  let hasSideEffects = 0;
  let isMoveImm = 1;
  let isReMaterializable = 1;
  let isAsCheapAsAMove = 1;
  let Defs = [PSR];
  let SchedRW = [WriteALU];
}

// not %rd, sign6 — rd = ~sign6  (op1=111) → 0x7C00|(sign6<<4)|rd
def NOT_ri : S1C33InstC3<0b111,
    (outs GR32:$rd), (ins simm6:$imm6),
    "not\t$rd, $imm6",
    [(set GR32:$rd, (not immSExt6:$imm6))]> {
  let hasSideEffects = 0;
  let Defs = [PSR];
  let SchedRW = [WriteALU];
}

//===----------------------------------------------------------------------===//
// CLASS 4 — SP arithmetic
//
// Encoding: |100|00|imm10| (add) / |100|01|imm10| (sub)
// Bit field: "imm10".
//===----------------------------------------------------------------------===//

let Defs = [SP, PSR], Uses = [SP], hasSideEffects = 0, SchedRW = [WriteALU] in {
  // add %sp, imm10  → 0x8000|imm10
  def ADDSP_i : S1C33InstC4SP<0,
      (outs), (ins uimm10:$imm10),
      "add\t%sp, $imm10", []>;

  // sub %sp, imm10  → 0x8400|imm10
  def SUBSP_i : S1C33InstC4SP<1,
      (outs), (ins uimm10:$imm10),
      "sub\t%sp, $imm10", []>;
}

//===----------------------------------------------------------------------===//
// CLASS 4 — Shift/rotate with immediate
//
// Encoding: |100|op1(3)|00(2)|imm4(4)|rd(4)|
// Bit fields: "imm4" (shift amount), "rd" (register).
// 2-address: rd = rd SHIFT imm4.
//===----------------------------------------------------------------------===//

let Defs = [PSR], hasSideEffects = 0, Constraints = "$rd = $rs", SchedRW = [WriteALU] in {
  // srl %rd, imm4  logical right shift  (op1=010) → 0x9000|(imm4<<4)|rd
  def SRL_ri : S1C33InstC4ShiftImm<0b010,
      (outs GR32:$rd), (ins GR32:$rs, shiftimm:$imm4),
      "srl\t$rd, $imm4",
      [(set GR32:$rd, (srl GR32:$rs, immShift8:$imm4))]>;

  // sll %rd, imm4  logical left shift   (op1=011) → 0x9400|(imm4<<4)|rd
  def SLL_ri : S1C33InstC4ShiftImm<0b011,
      (outs GR32:$rd), (ins GR32:$rs, shiftimm:$imm4),
      "sll\t$rd, $imm4",
      [(set GR32:$rd, (shl GR32:$rs, immShift8:$imm4))]>;

  // sra %rd, imm4  arithmetic right shift  (op1=100) → 0x9800|(imm4<<4)|rd
  def SRA_ri : S1C33InstC4ShiftImm<0b100,
      (outs GR32:$rd), (ins GR32:$rs, shiftimm:$imm4),
      "sra\t$rd, $imm4",
      [(set GR32:$rd, (sra GR32:$rs, immShift8:$imm4))]>;

  // sla %rd, imm4  shift left arithmetic (= sll for integers) (op1=101)
  // → 0x9C00|(imm4<<4)|rd
  def SLA_ri : S1C33InstC4ShiftImm<0b101,
      (outs GR32:$rd), (ins GR32:$rs, shiftimm:$imm4),
      "sla\t$rd, $imm4", []>;

  // rr %rd, imm4   rotate right  (op1=110) → 0xA000|(imm4<<4)|rd
  def RR_ri  : S1C33InstC4ShiftImm<0b110,
      (outs GR32:$rd), (ins GR32:$rs, shiftimm:$imm4),
      "rr\t$rd, $imm4",
      [(set GR32:$rd, (rotr GR32:$rs, immShift8:$imm4))]>;

  // rl %rd, imm4   rotate left   (op1=111) → 0xA400|(imm4<<4)|rd
  def RL_ri  : S1C33InstC4ShiftImm<0b111,
      (outs GR32:$rd), (ins GR32:$rs, shiftimm:$imm4),
      "rl\t$rd, $imm4",
      [(set GR32:$rd, (rotl GR32:$rs, immShift8:$imm4))]>;
}

//===----------------------------------------------------------------------===//
// CLASS 4 — Shift/rotate with register amount
//
// Encoding: |100|op1(3)|01(2)|rs(4)|rd(4)|
// Bit fields: "rs" (shift amount register), "rd" (register to shift).
// 2-address: rd = rd SHIFT rs.
//===----------------------------------------------------------------------===//

let Defs = [PSR], hasSideEffects = 0, Constraints = "$rd = $rs1", SchedRW = [WriteALU] in {
  // srl %rd, %rs  logical right shift by register amount  (op1=010)
  // No pattern — variable shifts go through VSRL pseudo (shift loop).
  def SRL_rr : S1C33InstC4ShiftReg<0b010,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "srl\t$rd, $rs", []>;

  // sll %rd, %rs  logical left shift by register amount  (op1=011)
  def SLL_rr : S1C33InstC4ShiftReg<0b011,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "sll\t$rd, $rs", []>;

  // sra %rd, %rs  arithmetic right shift by register amount  (op1=100)
  def SRA_rr : S1C33InstC4ShiftReg<0b100,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "sra\t$rd, $rs", []>;

  // sla %rd, %rs  shift left arithmetic by register  (op1=101)
  def SLA_rr : S1C33InstC4ShiftReg<0b101,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "sla\t$rd, $rs", []>;

  // rr %rd, %rs   rotate right by register amount  (op1=110)
  def RR_rr  : S1C33InstC4ShiftReg<0b110,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "rr\t$rd, $rs",
      [(set GR32:$rd, (rotr GR32:$rs1, GR32:$rs))]>;

  // rl %rd, %rs   rotate left by register amount   (op1=111)
  def RL_rr  : S1C33InstC4ShiftReg<0b111,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "rl\t$rd, $rs",
      [(set GR32:$rd, (rotl GR32:$rs1, GR32:$rs))]>;
}

//===----------------------------------------------------------------------===//
// CLASS 4 — Special register ops (scan/swap/mirror)
//
// Encoding: |100|op1(3)|10(2)|rs(4)|rd(4)|
//
// Verified op1 values (confirmed by user, 2026-03-24):
//   scan0: op1=010
//   scan1: op1=011
//   swap:  op1=100 (verified: 'swap %r5,%r13' → 0x92D5 in memset.o)
//   mirror: op1=101
//===----------------------------------------------------------------------===//

// scan0/scan1 update PSR flags (C and Z; V and N cleared).
let hasSideEffects = 0, Defs = [PSR], SchedRW = [WriteALU] in {
  // scan0 %rd, %rs  scan first 0 from MSB (op1=010, op2=10)
  def SCAN0 : S1C33InstC4Reg2<0b010, 0b10,
      (outs GR32:$rd), (ins GR32:$rs),
      "scan0\t$rd, $rs", []>;

  // scan1 %rd, %rs  scan first 1 from MSB (op1=011, op2=10)
  def SCAN1 : S1C33InstC4Reg2<0b011, 0b10,
      (outs GR32:$rd), (ins GR32:$rs),
      "scan1\t$rd, $rs",
      [(set GR32:$rd, (ctlz GR32:$rs))]>;
}

// swap/mirror do NOT update PSR flags.
let hasSideEffects = 0, SchedRW = [WriteALU] in {
  // swap %rd, %rs   swap upper/lower 16 bits (op1=100, op2=10)
  // Note: semantics from CPU manual - swaps the two halfwords of %rs into %rd.
  def SWAP : S1C33InstC4Reg2<0b100, 0b10,
      (outs GR32:$rd), (ins GR32:$rs),
      "swap\t$rd, $rs", []>;

  // mirror %rd, %rs  bit reversal (op1=101, op2=10)
  def MIRROR : S1C33InstC4Reg2<0b101, 0b10,
      (outs GR32:$rd), (ins GR32:$rs),
      "mirror\t$rd, $rs",
      [(set GR32:$rd, (bitreverse GR32:$rs))]>;
}

//===----------------------------------------------------------------------===//
// CLASS 4 — Division step instructions
//
// Encoding: |100|op1(3)|11(2)|rs(4)|0000|
// These implement S1C33000's software division algorithm (35 steps).
// Results accumulate in the implicit ALR/AHR special registers.
// CPU Manual §4.3: div0s/div0u initialize; div1 steps; div2s/div3s finalize.
//
// All div instructions have rd=0000 hardcoded.  The divisor is %rs.
// div3s has no register operands (rs=0000, rd=0000).
// op1 values confirmed by user 2026-03-24.
//===----------------------------------------------------------------------===//

let hasSideEffects = 1, Defs = [PSR, ALR, AHR], Uses = [ALR, AHR],
    SchedRW = [WriteDivStep] in {
  // div0s %rs  setup signed divide (op1=010, op2=11); reads ALR (dividend)
  def DIV0S : S1C33InstC4Reg2<0b010, 0b11,
      (outs), (ins GR32:$rs),
      "div0s\t$rs", []> {
    let rd = 0b0000;
  }

  // div0u %rs  setup unsigned divide (op1=011, op2=11); reads ALR (dividend)
  def DIV0U : S1C33InstC4Reg2<0b011, 0b11,
      (outs), (ins GR32:$rs),
      "div0u\t$rs", []> {
    let rd = 0b0000;
  }

  // div1 %rs   divide step (op1=100, op2=11); reads/writes ALR, AHR
  def DIV1 : S1C33InstC4Reg2<0b100, 0b11,
      (outs), (ins GR32:$rs),
      "div1\t$rs", []> {
    let rd = 0b0000;
  }

  // div2s %rs  signed divide adjust (op1=101, op2=11); reads/writes ALR, AHR
  def DIV2S : S1C33InstC4Reg2<0b101, 0b11,
      (outs), (ins GR32:$rs),
      "div2s\t$rs", []> {
    let rd = 0b0000;
  }

  // div3s  signed divide finalize (op1=110, op2=11); no register operands
  def DIV3S : S1C33InstC4Reg2<0b110, 0b11,
      (outs), (ins),
      "div3s", []> {
    let rs = 0b0000;
    let rd = 0b0000;
  }
}

//===----------------------------------------------------------------------===//
// CLASS 6 — EXT immediate extension
//
// Encoding: |110|imm13(13)| → 0xC000|imm13
// Bit field: "imm13".
//===----------------------------------------------------------------------===//

// hasSideEffects = 1 is deliberate even though ext writes no register:
// its effect (extending the NEXT instruction's immediate) is invisible to
// the dependency model, and bare EXT instructions created before the
// post-RA scheduler (eliminateFrameIndex emits them for large SP-relative
// offsets) would otherwise carry no edges at all and could be moved away
// from their target instruction.  The flag makes EXT a scheduling barrier
// (TargetInstrInfo::isGlobalMemoryObject), which keeps later memory ops
// ordered after it.  Most EXTs avoid the problem entirely by staying
// fused inside pseudos until ExpandExtPseudos, which runs after all
// scheduling; the delay slot filler also special-cases EXT.
def EXT : S1C33InstExt {
  let hasSideEffects = 1;
  let SchedRW = [WriteALU];
}

//===----------------------------------------------------------------------===//
// Pseudo instructions
//===----------------------------------------------------------------------===//

// ADJFI — materialize the address of a frame slot into a general register.
// Expanded by eliminateFrameIndex to: ld.w %rd, %sp; (add %rd, offset).
// Used when a frame slot address must appear as a data value (e.g. sret pointer).
def ADJFI : S1C33Pseudo<(outs GR32:$dst), (ins i32imm:$fi),
    "# ADJFI $dst, $fi", []> {
  let SchedRW = [WriteALU];
}

// ADJCALLSTACKDOWN / ADJCALLSTACKUP — call frame setup/destroy markers.
// Expanded to sub/add %sp by eliminateCallFramePseudoInstr.
let Defs = [SP], Uses = [SP], SchedRW = [WriteALU] in {
  def ADJCALLSTACKDOWN : S1C33Pseudo<(outs), (ins i32imm:$amt, i32imm:$amt2),
      "# ADJCALLSTACKDOWN $amt, $amt2",
      [(callseq_start timm:$amt, timm:$amt2)]>;
  def ADJCALLSTACKUP : S1C33Pseudo<(outs), (ins i32imm:$amt, i32imm:$amt2),
      "# ADJCALLSTACKUP $amt, $amt2",
      [(callseq_end timm:$amt, timm:$amt2)]>;
}

// MOV_ri19 — load a signed 19-bit immediate that does NOT fit in sign6.
// Expands to: ext imm13 + ld.w %rd, imm6 (two real instructions).
//
// isReMaterializable + isAsCheapAsAMove let the allocator re-create the
// constant at each use instead of spilling it when no callee-saved
// register is free.  Break-even for a constant used N times: spilling
// costs 2 (materialize) + 1 (store) + N (reloads) = 3 + N instructions;
// rematerializing costs 2N.  Remat wins for N <= 2 and ties at N = 3;
// for N >= 3 the allocator tends to keep the constant in a callee-saved
// register anyway, so the theoretical regression does not materialize.
//
// Defs = [PSR]: the expanded ld.w with an immediate operand clobbers the
// N/Z flags (see the note on MOV_ri32 below).
def MOV_ri19 : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$imm),
    "# MOV_ri19 $rd, $imm",
    [(set GR32:$rd, immSExt19NotSExt6:$imm)]> {
  let isMoveImm = 1;
  let isReMaterializable = 1;
  let isAsCheapAsAMove = 1;
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let Defs = [PSR];
  let SchedRW = [WriteALU];
}

// MOV_ri32 — load any 32-bit immediate into a register.
// Expanded by expandPostRAPseudo to:
//   isInt<6>(V):  ld.w %rd, imm6                             [matched by MOV_ri6]
//   isInt<19>(V): ext imm13 + ld.w %rd, imm6                 [matched by MOV_ri19]
//   otherwise:    ext imm13_hi + ext imm13_lo + ld.w %rd, imm6
def MOV_ri32 : S1C33Pseudo<(outs GR32:$rd), (ins i32imm:$imm32),
    "# MOV_ri32 $rd, $imm32",
    [(set GR32:$rd, imm:$imm32)]> {
  let isMoveImm = 1;
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  // ld.w %rd, sign6 (the final instruction after ext expansion) clobbers
  // N/Z flags.  Without this, the scheduler can place MOV_ri32 between
  // a compare and a conditional branch, silently clobbering PSR.
  let Defs = [PSR];
  let SchedRW = [WriteALU];
}

// ALU_ri32 pseudos — 2-address immediate ALU for constants beyond imm6 range.
// Expanded by expandPostRAPseudo to ext(s) + real ALU_ri instruction.
// TableGen selects these when the more-specific AND_ri/OR_ri/XOR_ri/SUB_ri
// patterns (constrained to immSExt6/immZExt6) do not match.
let Defs = [PSR], Constraints = "$rd = $rs", SchedRW = [WriteALU] in {
  def AND_ri32 : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm32),
      "# AND_ri32 $rd, $imm32",
      [(set GR32:$rd, (and GR32:$rs, imm:$imm32))]>;
  def OR_ri32  : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm32),
      "# OR_ri32 $rd, $imm32",
      [(set GR32:$rd, (or  GR32:$rs, imm:$imm32))]>;
  def XOR_ri32 : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm32),
      "# XOR_ri32 $rd, $imm32",
      [(set GR32:$rd, (xor GR32:$rs, imm:$imm32))]>;
  def SUB_ri32 : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs, i32imm:$imm32),
      "# SUB_ri32 $rd, $imm32",
      [(set GR32:$rd, (sub GR32:$rs, imm:$imm32))]>;
}

// CMP_ri32 — compare with arbitrary 32-bit immediate (flags only).
// Expanded by ExpandExtPseudos to ext(s) + cmp %rd, sign6.
// TableGen prefers the more-specific CMP_ri (immSExt6) when the immediate
// fits in 6 bits; this pseudo catches the rest.
let Defs = [PSR] in
def CMP_ri32 : S1C33Pseudo<(outs), (ins GR32:$rd, i32imm:$imm32),
    "# CMP_ri32 $rd, $imm32",
    [(S1C33CmpCC GR32:$rd, imm:$imm32)]> {
  let SchedRW = [WriteALU];
}

// Decrement idiom: (add %rs, -N) where 1 ≤ N ≤ 63 → sub %rd, N
// Handles the common case where the DAGCombiner leaves (add x, -1) instead of (sub x, 1).
def : Pat<(add GR32:$rs, negImmToUImm6:$imm),
          (SUB_ri GR32:$rs, (NegImm $imm))>;
// (add %rs, -N) where 64 ≤ N ≤ 2^26-1 → SUB_rri (3-operand form)
// DAGCombiner canonicalizes (sub x, C) to (add x, -C), so we must match
// the add-with-negative pattern to select the 3-operand subtract.
def negImmZExt26Over63 : PatLeaf<(imm), [{
  int64_t V = N->getSExtValue();
  return V < -63 && (-V) <= ((1 << 26) - 1);
}]>;
let AddedComplexity = 5 in
def : Pat<(add GR32:$rs, negImmZExt26Over63:$imm),
          (SUB_rri GR32:$rs, (NegImm $imm))>;
// (add %rs, -N) where N > 2^26-1 → SUB_ri32 (2-operand, needs full 32-bit)
def negImmLarge : PatLeaf<(imm), [{ return N->getSExtValue() < -63; }]>;
def : Pat<(add GR32:$rs, negImmLarge:$imm),
          (SUB_ri32 GR32:$rs, (NegImm $imm))>;


// SELECT — conditional select pseudo, expanded by EmitInstrWithCustomInserter.
// Operands: dst, lhs, rhs, trueval, falseval, cc (S1C33CC enum).
def SELECT : S1C33Pseudo<
    (outs GR32:$dst),
    (ins GR32:$lhs, GR32:$rhs, GR32:$trueval, GR32:$falseval, i32imm:$cc),
    "# SELECT $dst, $lhs, $rhs, $trueval, $falseval, $cc",
    [(set GR32:$dst, (S1C33SelectCC GR32:$lhs, GR32:$rhs,
                                    GR32:$trueval, GR32:$falseval,
                                    imm:$cc))]> {
  let usesCustomInserter = 1;
  let SchedRW = [WriteALU];
}

//===----------------------------------------------------------------------===//
// Variable shift pseudos — expanded by EmitInstrWithCustomInserter.
// S1C33 shift instructions only support amounts 0-8 (both immediate and
// register-register forms).  For variable shifts the amount may exceed 8,
// so we expand into a loop that shifts by 8 at a time.
//===----------------------------------------------------------------------===//
let usesCustomInserter = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0,
    SchedRW = [WriteALU] in {
  def VSRL : S1C33Pseudo<(outs GR32:$dst), (ins GR32:$val, GR32:$amt),
      "# VSRL $dst, $val, $amt",
      [(set GR32:$dst, (srl GR32:$val, GR32:$amt))]>;
  def VSLL : S1C33Pseudo<(outs GR32:$dst), (ins GR32:$val, GR32:$amt),
      "# VSLL $dst, $val, $amt",
      [(set GR32:$dst, (shl GR32:$val, GR32:$amt))]>;
  def VSRA : S1C33Pseudo<(outs GR32:$dst), (ins GR32:$val, GR32:$amt),
      "# VSRA $dst, $val, $amt",
      [(set GR32:$dst, (sra GR32:$val, GR32:$amt))]>;
}

//===----------------------------------------------------------------------===//
// Global address/symbol Pat<> matching patterns.
// These must come after LDW_ri and STW_ri are defined.
//===----------------------------------------------------------------------===//

// Address materialization: S1C33Wrapper(tglobaladdr/texternalsym) → LDW_SYM_EXT0.
// LowerOperation(ISD::GlobalAddress/ExternalSymbol/ConstantPool) wraps the target
// node in S1C33ISD::Wrapper so isel can match it as a value-producing node.
def : Pat<(S1C33Wrapper tglobaladdr:$sym),  (LDW_SYM_EXT0 tglobaladdr:$sym)>;
def : Pat<(S1C33Wrapper texternalsym:$sym), (LDW_SYM_EXT0 texternalsym:$sym)>;
def : Pat<(S1C33Wrapper tconstpool:$sym),   (LDW_SYM_EXT0 tconstpool:$sym)>;

// Load word from global/external symbol address.
def : Pat<(i32 (load (S1C33Wrapper tglobaladdr:$sym))),
          (LDW_ri (LDW_SYM_EXT0 tglobaladdr:$sym))>;
def : Pat<(i32 (load (S1C33Wrapper texternalsym:$sym))),
          (LDW_ri (LDW_SYM_EXT0 texternalsym:$sym))>;
def : Pat<(i32 (load (S1C33Wrapper tconstpool:$sym))),
          (LDW_ri (LDW_SYM_EXT0 tconstpool:$sym))>;

// Byte/bool loads from global/external symbol address.
def : Pat<(i32 (zextloadi8 (S1C33Wrapper tglobaladdr:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 tglobaladdr:$sym))>;
def : Pat<(i32 (zextloadi8 (S1C33Wrapper texternalsym:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 texternalsym:$sym))>;
def : Pat<(i32 (zextloadi8 (S1C33Wrapper tconstpool:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 tconstpool:$sym))>;
def : Pat<(i32 (sextloadi8 (S1C33Wrapper tglobaladdr:$sym))),
          (LDB_ri (LDW_SYM_EXT0 tglobaladdr:$sym))>;
def : Pat<(i32 (sextloadi8 (S1C33Wrapper texternalsym:$sym))),
          (LDB_ri (LDW_SYM_EXT0 texternalsym:$sym))>;
def : Pat<(i32 (sextloadi8 (S1C33Wrapper tconstpool:$sym))),
          (LDB_ri (LDW_SYM_EXT0 tconstpool:$sym))>;
def : Pat<(i32 (zextloadi1 (S1C33Wrapper tglobaladdr:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 tglobaladdr:$sym))>;
def : Pat<(i32 (zextloadi1 (S1C33Wrapper texternalsym:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 texternalsym:$sym))>;
def : Pat<(i32 (zextloadi1 (S1C33Wrapper tconstpool:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 tconstpool:$sym))>;
def : Pat<(i32 (extloadi1 (S1C33Wrapper tglobaladdr:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 tglobaladdr:$sym))>;
def : Pat<(i32 (extloadi1 (S1C33Wrapper texternalsym:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 texternalsym:$sym))>;
def : Pat<(i32 (extloadi1 (S1C33Wrapper tconstpool:$sym))),
          (LDUB_ri (LDW_SYM_EXT0 tconstpool:$sym))>;

// Store word to global/external symbol address.
def : Pat<(store GR32:$rs, (S1C33Wrapper tglobaladdr:$sym)),
          (STW_ri (LDW_SYM_EXT0 tglobaladdr:$sym), GR32:$rs)>;
def : Pat<(store GR32:$rs, (S1C33Wrapper texternalsym:$sym)),
          (STW_ri (LDW_SYM_EXT0 texternalsym:$sym), GR32:$rs)>;

// Register-indirect with offset patterns: (load/store (add %rb, imm13)) → ext+ld.x [%rb]
// These are selected as Lxx_ri_off pseudos and expanded to ext+Lxx_ri in expandPostRAPseudo.
// Primary patterns are inline in the instruction definitions above.
// Additional patterns for "any-extend" loads (map to unsigned variants).
// ImmAsTarget wraps the offset in a fresh TargetConstant to avoid node-sharing issues
// where a shared Constant gets replaced by a materialized register.
def : Pat<(extloadi8  (add GR32:$rb, immZExt13:$off)), (LDUB_ri_off GR32:$rb, (ImmAsTarget $off))>;
def : Pat<(extloadi16 (add GR32:$rb, immZExt13:$off)), (LDUH_ri_off GR32:$rb, (ImmAsTarget $off))>;

//===----------------------------------------------------------------------===//
// CLASS 5 — Special register transfers (op2=00)
//
// CPU Manual §4.2 Class 5 table, op2=00:
//   ld.w %sd, %rs  op1=000 — write GPR to special reg (sd in bits[3:0])
//   ld.w %rd, %ss  op1=001 — read special reg to GPR  (ss in bits[7:4])
//   adc  %rd, %rs  op1=110 — add with carry (rd += rs + C)
//   sbc  %rd, %rs  op1=111 — subtract with carry (rd -= rs + ~C)
//
// Special register codes: psr=0, sp=1, alr=2, ahr=3
//===----------------------------------------------------------------------===//

// --- Write GPR to special register (op1=000, op2=00) ---

// ld.w %sp, %rs — write GPR to SP
// Encoding: 0xA000|(rs<<4)|1
let hasSideEffects = 1, Defs = [SP], SchedRW = [WriteSpecMove] in
def STW_to_SP : S1C33InstC5Reg2<0b000, 0b00,
    (outs), (ins GR32:$rs),
    "ld.w\t%sp, $rs", []> {
  let rd = 0b0001;  // sd=1 (SP) hardcoded in bits[3:0]
}

// ld.w %psr, %rs — write GPR to PSR (flags register)
// Encoding: 0xA000|(rs<<4)|0
let hasSideEffects = 1, Defs = [PSR], SchedRW = [WriteSpecMove] in
def STW_to_PSR : S1C33InstC5Reg2<0b000, 0b00,
    (outs), (ins GR32:$rs),
    "ld.w\t%psr, $rs", []> {
  let rd = 0b0000;  // sd=0 (PSR) hardcoded in bits[3:0]
}

// ld.w %alr, %rs — write GPR to ALR (accumulator low)
// Encoding: 0xA000|(rs<<4)|2
let hasSideEffects = 1, Defs = [ALR], SchedRW = [WriteSpecMove] in
def STW_to_ALR : S1C33InstC5Reg2<0b000, 0b00,
    (outs), (ins GR32:$rs),
    "ld.w\t%alr, $rs", []> {
  let rd = 0b0010;  // sd=2 (ALR) hardcoded in bits[3:0]
}

// ld.w %ahr, %rs — write GPR to AHR (accumulator high)
// Encoding: 0xA000|(rs<<4)|3
let hasSideEffects = 1, Defs = [AHR], SchedRW = [WriteSpecMove] in
def STW_to_AHR : S1C33InstC5Reg2<0b000, 0b00,
    (outs), (ins GR32:$rs),
    "ld.w\t%ahr, $rs", []> {
  let rd = 0b0011;  // sd=3 (AHR) hardcoded in bits[3:0]
}

// --- Read special register to GPR (op1=001, op2=00) ---

// ld.w %rd, %sp — read SP into GPR
// Encoding: 0xA400|0x10|rd  (ss=1 in bits[7:4])
let hasSideEffects = 0, Uses = [SP], SchedRW = [WriteSpecMove] in
def LDW_from_SP : S1C33InstC5Reg2<0b001, 0b00,
    (outs GR32:$rd), (ins),
    "ld.w\t$rd, %sp", []> {
  let rs = 0b0001;  // ss=1 (SP) hardcoded in bits[7:4]
}

// ld.w %rd, %psr — read PSR into GPR
// Encoding: 0xA400|0|rd  (ss=0 in bits[7:4])
let hasSideEffects = 0, Uses = [PSR], SchedRW = [WriteSpecMove] in
def LDW_from_PSR : S1C33InstC5Reg2<0b001, 0b00,
    (outs GR32:$rd), (ins),
    "ld.w\t$rd, %psr", []> {
  let rs = 0b0000;  // ss=0 (PSR) hardcoded in bits[7:4]
}

// ld.w %rd, %alr — read ALR (multiply/divide low result) into GPR
// Class 5: op1=001, op2=00, ss=2 (ALR) in bits[7:4], rd in bits[3:0]
// Encoding: 0xA420 | rd
let hasSideEffects = 0, Uses = [ALR], SchedRW = [WriteSpecMove] in
def LDW_from_ALR : S1C33InstC5Reg2<0b001, 0b00,
    (outs GR32:$rd), (ins),
    "ld.w\t$rd, %alr", []> {
  let rs = 0b0010;  // ss=2 (ALR) hardcoded in bits[7:4]
}

// ld.w %rd, %ahr — read AHR (multiply/divide high result) into GPR
// Class 5: op1=001, op2=00, ss=3 (AHR) in bits[7:4], rd in bits[3:0]
// Encoding: 0xA430 | rd
let hasSideEffects = 0, Uses = [AHR], SchedRW = [WriteSpecMove] in
def LDW_from_AHR : S1C33InstC5Reg2<0b001, 0b00,
    (outs GR32:$rd), (ins),
    "ld.w\t$rd, %ahr", []> {
  let rs = 0b0011;  // ss=3 (AHR) hardcoded in bits[7:4]
}

// --- Add/subtract with carry (op1=110/111, op2=00) ---

let Defs = [PSR], Uses = [PSR], hasSideEffects = 0,
    Constraints = "$rd = $rs1", SchedRW = [WriteALU] in {
  // adc %rd, %rs — add with carry: rd += rs + C  (op1=110)
  def ADC_rr : S1C33InstC5Reg2<0b110, 0b00,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "adc\t$rd, $rs", []>;

  // sbc %rd, %rs — subtract with carry: rd -= rs + ~C  (op1=111)
  def SBC_rr : S1C33InstC5Reg2<0b111, 0b00,
      (outs GR32:$rd), (ins GR32:$rs1, GR32:$rs),
      "sbc\t$rd, $rs", []>;
}

//===----------------------------------------------------------------------===//
// CLASS 5 — Register-to-register narrow loads (op2=01)
//
// CPU Manual §4.2 Class 5 table, op2=01:
//   ld.b  %rd, %rs  op1=000 — sign-extend byte    (bits[7:0] of rs → rd)
//   ld.ub %rd, %rs  op1=001 — zero-extend byte    (bits[7:0] of rs → rd)
//   ld.h  %rd, %rs  op1=010 — sign-extend halfword (bits[15:0] of rs → rd)
//   ld.uh %rd, %rs  op1=011 — zero-extend halfword (bits[15:0] of rs → rd)
//
// NOTE: The correct op1 values are 000-011, NOT 100-111.
//       Verified against EPSON as33 output: ld.ub %r13,%r13 → 0xA5DD
//       (class=101, op1=001, op2=01, rs=r13, rd=r13).
//
// These differ from Class 1 register-indirect loads ([%rb]) in that the
// source is a register, not a memory address.
// The AsmString has no brackets; the AsmMatcher distinguishes by operand type.
//===----------------------------------------------------------------------===//

// Register-to-register narrow loads do NOT update PSR flags (CPU Manual QR).
let hasSideEffects = 0, SchedRW = [WriteALU] in {
  // ld.b %rd, %rs   sign-extend byte in rs into rd (op1=000, op2=01)
  def LDB_rr  : S1C33InstC5Reg2<0b000, 0b01,
      (outs GR32:$rd), (ins GR32:$rs),
      "ld.b\t$rd, $rs",
      [(set GR32:$rd, (sext_inreg GR32:$rs, i8))]>;

  // ld.ub %rd, %rs  zero-extend byte in rs into rd (op1=001, op2=01)
  def LDUB_rr : S1C33InstC5Reg2<0b001, 0b01,
      (outs GR32:$rd), (ins GR32:$rs),
      "ld.ub\t$rd, $rs",
      [(set GR32:$rd, (and GR32:$rs, (i32 255)))]>;

  // ld.h %rd, %rs   sign-extend halfword in rs into rd (op1=010, op2=01)
  def LDH_rr  : S1C33InstC5Reg2<0b010, 0b01,
      (outs GR32:$rd), (ins GR32:$rs),
      "ld.h\t$rd, $rs",
      [(set GR32:$rd, (sext_inreg GR32:$rs, i16))]>;

  // ld.uh %rd, %rs  zero-extend halfword in rs into rd (op1=011, op2=01)
  def LDUH_rr : S1C33InstC5Reg2<0b011, 0b01,
      (outs GR32:$rd), (ins GR32:$rs),
      "ld.uh\t$rd, $rs",
      [(set GR32:$rd, (and GR32:$rs, (i32 65535)))]>;
}

//===----------------------------------------------------------------------===//
// CLASS 5 — Bit operations (btst/bclr/bset/bnot)
//
// Format: |101|op1(3)|00(2)|rb(4)|0|imm3(3)|
//   op1=010: btst (bit test  → PSR.Z = ~bit[imm3] of B[rb])
//   op1=011: bclr (bit clear → B[rb] &= ~(1<<imm3))
//   op1=100: bset (bit set   → B[rb] |=  (1<<imm3))
//   op1=101: bnot (bit invert→ B[rb] ^=  (1<<imm3))
//
// These are MEMORY operations: rb is an address register (not modified).
// The byte at memory address rb is read (and for bclr/bset/bnot, written).
// Assembly syntax: btst [%rb], imm3  (brackets are mandatory — CPU manual §2.5.5)
//===----------------------------------------------------------------------===//

// 3-bit unsigned immediate for bit position (0..7)
def uimm3 : Operand<i32> {
  let OperandType = "OPERAND_IMMEDIATE";
  let ParserMatchClass = UImm3AsmOperand;
}

// btst [%rb], imm3 — bit test: PSR.Z = ~bit[imm3] of B[rb]  (memory read)
let Defs = [PSR], mayLoad = 1, hasSideEffects = 0, SchedRW = [WriteBitOp] in
def BTST : S1C33InstC5BitOp<0b010,
    (outs), (ins GR32:$rb, uimm3:$imm3),
    "btst\t[$rb], $imm3", []>;

// bclr [%rb], imm3 — bit clear: B[rb] &= ~(1<<imm3)  (memory read-modify-write)
let mayLoad = 1, mayStore = 1, hasSideEffects = 0, SchedRW = [WriteBitOp] in
def BCLR : S1C33InstC5BitOp<0b011,
    (outs), (ins GR32:$rb, uimm3:$imm3),
    "bclr\t[$rb], $imm3", []>;

// bset [%rb], imm3 — bit set: B[rb] |= (1<<imm3)  (memory read-modify-write)
let mayLoad = 1, mayStore = 1, hasSideEffects = 0, SchedRW = [WriteBitOp] in
def BSET : S1C33InstC5BitOp<0b100,
    (outs), (ins GR32:$rb, uimm3:$imm3),
    "bset\t[$rb], $imm3", []>;

// bnot [%rb], imm3 — bit invert: B[rb] ^= (1<<imm3)  (memory read-modify-write)
let mayLoad = 1, mayStore = 1, hasSideEffects = 0, SchedRW = [WriteBitOp] in
def BNOT : S1C33InstC5BitOp<0b101,
    (outs), (ins GR32:$rb, uimm3:$imm3),
    "bnot\t[$rb], $imm3", []>;

//===----------------------------------------------------------------------===//
// Bit operation with ext-extended displacement (Class 5 + ext form).
//
// CPU Manual §2.5.5 (Immediate Extension, case 1): each bit operation accepts
// one or two preceding ext prefixes, yielding `[%rb+imm13]` (or `imm26`) as the
// effective memory byte.  The displacement is UNSIGNED (zero-extended by ext)
// — same rule as Class 1 memory instructions.
//
// These pseudos let codegen (and asm aliases) express the single-line
// `btst/bclr/bset/bnot [%rb+off], imm3` form.  ExpandExtPseudos emits:
//   off == 0:   <opcode> [%rb], imm3
//   0 < off ≤ 8191: ext off / <opcode> [%rb], imm3
// Larger offsets are not representable in a single ext13 — the user must
// recompute the base address separately.
//===----------------------------------------------------------------------===//

let Defs = [PSR], mayLoad = 1, hasSideEffects = 0, SchedRW = [WriteBitOp] in
def BTST_ri_off : S1C33Pseudo<(outs),
    (ins GR32:$rb, uimm13:$off, uimm3:$imm3),
    "# btst\t[$rb+$off], $imm3", []>;

let mayLoad = 1, mayStore = 1, hasSideEffects = 0, SchedRW = [WriteBitOp] in {
  def BCLR_ri_off : S1C33Pseudo<(outs),
      (ins GR32:$rb, uimm13:$off, uimm3:$imm3),
      "# bclr\t[$rb+$off], $imm3", []>;
  def BSET_ri_off : S1C33Pseudo<(outs),
      (ins GR32:$rb, uimm13:$off, uimm3:$imm3),
      "# bset\t[$rb+$off], $imm3", []>;
  def BNOT_ri_off : S1C33Pseudo<(outs),
      (ins GR32:$rb, uimm13:$off, uimm3:$imm3),
      "# bnot\t[$rb+$off], $imm3", []>;
}

//===----------------------------------------------------------------------===//
// CLASS 5 — Multiply instructions (S1C33209 optional hardware multiplier)
//
// CPU Manual §2.5.7, §4.2 Class 5 table:
//   mlt.h  %rd, %rs  op1=000 op2=10  16×16→32-bit signed  (ALR=result)
//   mltu.h %rd, %rs  op1=001 op2=10  16×16→32-bit unsigned (ALR=result)
//   mlt.w  %rd, %rs  op1=010 op2=10  32×32→64-bit signed   (AHR:ALR)
//   mltu.w %rd, %rs  op1=011 op2=10  32×32→64-bit unsigned  (AHR:ALR)
//   mac    %rs       op1=100 op2=10  AHR:ALR += %rs × ALR   (rd=0 fixed)
//
// For encoding: rs (multiplicand) at bits[7:4], rd (multiplier) at bits[3:0].
// Both operands are sources; results go implicitly to ALR/AHR.
// Guarded by Requires<[FeatureHWMul]>.
//===----------------------------------------------------------------------===//

let Predicates = [HasHWMul] in {

// mlt.h %rd, %rs — 16×16→32-bit signed multiply.
// Hardware writes the arithmetic result registers as a unit; model both ALR/AHR.
// Class 5: op1=000, op2=10 → 0xA800|(rs<<4)|rd
let Defs = [ALR, AHR], hasSideEffects = 0, SchedRW = [WriteMltH] in
def MLT_H : S1C33InstC5Reg2<0b000, 0b10,
    (outs), (ins GR32:$rs, GR32:$rd),
    "mlt.h\t$rd, $rs", []>;

// mltu.h %rd, %rs — 16×16→32-bit unsigned multiply.
// Hardware writes the arithmetic result registers as a unit; model both ALR/AHR.
// Class 5: op1=001, op2=10 → 0xA900|(rs<<4)|rd
let Defs = [ALR, AHR], hasSideEffects = 0, SchedRW = [WriteMltH] in
def MLTU_H : S1C33InstC5Reg2<0b001, 0b10,
    (outs), (ins GR32:$rs, GR32:$rd),
    "mltu.h\t$rd, $rs", []>;

// mlt.w %rd, %rs — 32×32→64-bit signed multiply (AHR:ALR = rd × rs)
// Class 5: op1=010, op2=10 → 0xAA00|(rs<<4)|rd
let Defs = [ALR, AHR], hasSideEffects = 0, SchedRW = [WriteMltW] in
def MLT_W : S1C33InstC5Reg2<0b010, 0b10,
    (outs), (ins GR32:$rs, GR32:$rd),
    "mlt.w\t$rd, $rs", []>;

// mltu.w %rd, %rs — 32×32→64-bit unsigned multiply (AHR:ALR = rd × rs)
// Class 5: op1=011, op2=10 → 0xAB00|(rs<<4)|rd
let Defs = [ALR, AHR], hasSideEffects = 0, SchedRW = [WriteMltW] in
def MLTU_W : S1C33InstC5Reg2<0b011, 0b10,
    (outs), (ins GR32:$rs, GR32:$rd),
    "mltu.w\t$rd, $rs", []>;

// mac %rs — multiply-accumulate with implicit register window and memory access.
// Class 5: op1=100, op2=10, rs in bits[7:4], rd=0000 (fixed).
// CPU Manual §2.5.7: updates AHR:ALR, reads memory through the implicit
// register window rooted at %rs, and post-updates that window (%rs..%rs+2).
// The register window cannot be modeled statically in TableGen, so keep the
// instruction conservatively side-effecting and memory-reading.
let Defs = [ALR, AHR], Uses = [ALR, AHR], mayLoad = 1, hasSideEffects = 1,
    SchedRW = [WriteMltW] in
def MAC : S1C33InstC5Reg2<0b100, 0b10,
    (outs), (ins GR32:$rs),
    "mac\t$rs", []> {
  let rd = 0b0000;  // rd field fixed to 0
}

//===----------------------------------------------------------------------===//
// Multiply pseudo instructions
//
// MUL_r:   32×32→32-bit multiply — expanded to mlt.w + ld.w %rd, %alr
// MULHS_r: signed   high-32     — expanded to mlt.w  + ld.w %rd, %ahr
// MULHU_r: unsigned high-32     — expanded to mltu.w + ld.w %rd, %ahr
//===----------------------------------------------------------------------===//

let SchedRW = [WriteMltW] in {
def MUL_r : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs1, GR32:$rs2),
    "# MUL_r $rd, $rs1, $rs2",
    [(set GR32:$rd, (mul GR32:$rs1, GR32:$rs2))]>;

def MULHS_r : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs1, GR32:$rs2),
    "# MULHS_r $rd, $rs1, $rs2",
    [(set GR32:$rd, (mulhs GR32:$rs1, GR32:$rs2))]>;

def MULHU_r : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs1, GR32:$rs2),
    "# MULHU_r $rd, $rs1, $rs2",
    [(set GR32:$rd, (mulhu GR32:$rs1, GR32:$rs2))]>;
}

let SchedRW = [WriteMltH] in {
// MUL16U_r: 16×16 unsigned multiply (1 clock) — selected when both operands
// are known to fit in unsigned 16 bits.  Expanded to: mltu.h + ld.w %alr
def MUL16U_r : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs1, GR32:$rs2),
    "# MUL16U_r $rd, $rs1, $rs2", []>;

// MUL16S_r: 16×16 signed multiply (1 clock) — selected when both operands
// are known to fit in signed 16 bits.  Expanded to: mlt.h + ld.w %alr
def MUL16S_r : S1C33Pseudo<(outs GR32:$rd), (ins GR32:$rs1, GR32:$rs2),
    "# MUL16S_r $rd, $rs1, $rs2", []>;
}

} // let Predicates = [HasHWMul]
