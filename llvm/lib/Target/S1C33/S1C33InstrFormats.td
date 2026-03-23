//===-- S1C33InstrFormats.td - S1C33 Instruction Formats -*-- tablegen ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33000 CPU Core Manual §4.2: 命令コード体系 (Instruction Code System)
//
// All instructions are 16-bit fixed-length.
// Bits[15:13] determine the instruction class (Class 0–7).
//
//   Class 0 (000): Single-operand + branch instructions
//   Class 1 (001): Register-indirect memory + register-register ALU
//   Class 2 (010): SP-relative displacement memory
//   Class 3 (011): 6-bit immediate ALU
//   Class 4 (100): SP arithmetic, shift/rotate, divide steps
//   Class 5 (101): Special register transfer, bit ops, multiply
//   Class 6 (110): EXT immediate extension
//   Class 7 (111): Reserved
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Base instruction class
//===----------------------------------------------------------------------===//

class S1C33Inst<dag outs, dag ins, string asmstr, list<dag> pattern,
                bits<3> cls>
    : Instruction {
  let Namespace = "S1C33";
  let Size = 2; // 16-bit instruction
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;

  field bits<16> Inst;
  let Inst{15-13} = cls;

  field bits<16> SoftFail = 0;
}

// Pseudo instructions: used only during CodeGen, not emitted.
class S1C33Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : S1C33Inst<outs, ins, asmstr, pattern, 0b000> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

// 4-byte pseudo for relaxed ext+branch/call forms.
// Emitted by MC relaxation when a branch target is out of sign8 range.
// encodeInstruction() handles these specially: ext word + branch word.
class S1C33PseudoExt1<dag outs, dag ins, string asmstr, list<dag> pattern>
    : S1C33Inst<outs, ins, asmstr, pattern, 0b000> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Size = 4;
}

// Pseudo instruction for global address materialization with 3-level relaxation.
// Size 2 = EXT0 (ld.w only), 4 = EXT1 (ext+ld.w), 6 = EXT2 (ext+ext+ld.w).
class S1C33PseudoAddrN<int Sz, dag outs, dag ins, string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b000> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Size = Sz;
}

//===----------------------------------------------------------------------===//
// CLASS 0 — Single-operand and branch instructions
//
// Fixed format:  |000|op1(4)|d(1)|op2(2)|0|0|operand(4)|
//   bits[15:13]=000, bits[12:9]=op1, bit[8]=d (delayed),
//   bits[7:6]=op2, bits[5:4]=0, bits[3:0]=operand
//
// Relative format: |000|op1(4)|d(1)|sign8(8)|
//   bits[15:13]=000, bits[12:9]=op1, bit[8]=d, bits[7:0]=sign8
//===----------------------------------------------------------------------===//

// Class 0, fixed format — no register operand (nop/slp/halt/ret/reti/...)
class S1C33InstC0<bits<4> op1, bit d, bits<2> op2, dag outs, dag ins,
                  string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b000> {
  let Inst{12-9} = op1;
  let Inst{8}    = d;
  let Inst{7-6}  = op2;
  let Inst{5-4}  = 0b00;
  let Inst{3-0}  = 0b0000;
}

// Class 0, fixed format — single register operand (pushn/popn/call %rb/jp %rb)
class S1C33InstC0Reg<bits<4> op1, bit d, bits<2> op2, dag outs, dag ins,
                     string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b000> {
  bits<4> rb;
  let Inst{12-9} = op1;
  let Inst{8}    = d;
  let Inst{7-6}  = op2;
  let Inst{5-4}  = 0b00;
  let Inst{3-0}  = rb;
}

// Class 0, fixed format — 2-bit immediate operand (int imm2)
class S1C33InstC0Imm2<bits<4> op1, bit d, bits<2> op2, dag outs, dag ins,
                      string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b000> {
  bits<2> imm2;
  let Inst{12-9} = op1;
  let Inst{8}    = d;
  let Inst{7-6}  = op2;
  let Inst{5-4}  = 0b00;
  let Inst{3-2}  = 0b00;
  let Inst{1-0}  = imm2;
}

// Class 0, PC-relative format — 8-bit signed offset (jrXX/jp/call)
class S1C33InstC0Rel<bits<4> op1, bit d, dag outs, dag ins,
                     string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b000> {
  bits<8> imm8;  // sign8 offset (in units of 2 bytes; range ±256 bytes)
  let Inst{12-9} = op1;
  let Inst{8}    = d;
  let Inst{7-0}  = imm8;
}

//===----------------------------------------------------------------------===//
// CLASS 1 — Register-indirect memory + register-register ALU
//
// Memory format: |001|op1(3)|op2(2)|rb(4)|rd/rs(4)|
// ALU format:    |001|op1(3)|10(2)|rs(4)|rd(4)|
//   bits[15:13]=001, bits[12:10]=op1, bits[9:8]=op2,
//   bits[7:4]=rb/rs, bits[3:0]=rd/rs
//===----------------------------------------------------------------------===//

// Class 1, memory indirect — rb is address register, rd/rs is data register
class S1C33InstC1Mem<bits<3> op1, bits<2> op2, dag outs, dag ins,
                     string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b001> {
  bits<4> rb;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-8}   = op2;
  let Inst{7-4}   = rb;
  let Inst{3-0}   = rd;
}

// Class 1, register-register ALU — op2 is always 0b10
class S1C33InstC1ALU<bits<3> op1, dag outs, dag ins,
                     string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b001> {
  bits<4> rs;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-8}   = 0b10;
  let Inst{7-4}   = rs;
  let Inst{3-0}   = rd;
}

//===----------------------------------------------------------------------===//
// CLASS 2 — SP-relative displacement memory transfer
//
// Format: |010|op1(3)|imm6(6)|rd/rs(4)|
//   bits[15:13]=010, bits[12:10]=op1, bits[9:4]=imm6, bits[3:0]=rd/rs
//===----------------------------------------------------------------------===//

class S1C33InstC2<bits<3> op1, dag outs, dag ins,
                  string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b010> {
  bits<6> imm6;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-4}   = imm6;
  let Inst{3-0}   = rd;
}

//===----------------------------------------------------------------------===//
// CLASS 3 — 6-bit immediate ALU
//
// Format: |011|op1(3)|imm6(6)|rd(4)|
//   bits[15:13]=011, bits[12:10]=op1, bits[9:4]=imm6/sign6, bits[3:0]=rd
//===----------------------------------------------------------------------===//

class S1C33InstC3<bits<3> op1, dag outs, dag ins,
                  string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b011> {
  bits<6> imm6;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-4}   = imm6;
  let Inst{3-0}   = rd;
}

//===----------------------------------------------------------------------===//
// CLASS 4 — SP arithmetic + shift/rotate + divide steps
//
// SP format:    |100|op1(3)|imm10(10)|  (op1=000: add, op1=001: sub)
// Shift format: |100|op1(3)|op2(2)|imm4/rs(4)|rd(4)|
//   bits[15:13]=100
//===----------------------------------------------------------------------===//

// Class 4, SP add/sub with 10-bit immediate
// op1 bit[12:10]: 000 = add, 001 = sub  (bits[12:11]=00, bit[10]=0 or 1)
class S1C33InstC4SP<bit subop, dag outs, dag ins,
                    string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b100> {
  bits<10> imm10;
  let Inst{12-11} = 0b00;
  let Inst{10}    = subop;
  let Inst{9-0}   = imm10;
}

// Class 4, shift with immediate operand (op2=00)
class S1C33InstC4ShiftImm<bits<3> op1, dag outs, dag ins,
                          string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b100> {
  bits<4> imm4;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-8}   = 0b00;
  let Inst{7-4}   = imm4;
  let Inst{3-0}   = rd;
}

// Class 4, shift with register operand (op2=01)
class S1C33InstC4ShiftReg<bits<3> op1, dag outs, dag ins,
                          string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b100> {
  bits<4> rs;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-8}   = 0b01;
  let Inst{7-4}   = rs;
  let Inst{3-0}   = rd;
}

// Class 4, two-register ops (scan/swap/mirror/divide steps, op2=10 or 11)
class S1C33InstC4Reg2<bits<3> op1, bits<2> op2, dag outs, dag ins,
                      string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b100> {
  bits<4> rs;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-8}   = op2;
  let Inst{7-4}   = rs;
  let Inst{3-0}   = rd;
}

//===----------------------------------------------------------------------===//
// CLASS 5 — Special register transfer, bit ops, multiply
//
// Format: |101|op1(3)|op2(2)|rs(4)|rd(4)|
//   bits[15:13]=101, bits[12:10]=op1, bits[9:8]=op2, bits[7:4]=rs, bits[3:0]=rd
//
// Special register transfer (op2=00):
//   ld.w %sd, %rs  op1=000 — GPR → special reg (sd: sp=0,psr=1,alr=2,ahr=3)
//   ld.w %rd, %ss  op1=001 — special reg → GPR (ss: sp=0,psr=1,alr=2,ahr=3)
//
// Multiply (all use op2=10; CPU Manual §4.2 Class 5 table):
//   mlt.h  %rd, %rs  op1=000 op2=10 — 16×16→32-bit signed   (ALR=result)
//   mltu.h %rd, %rs  op1=001 op2=10 — 16×16→32-bit unsigned  (ALR=result)
//   mlt.w  %rd, %rs  op1=010 op2=10 — 32×32→64-bit signed    (ALR=lo, AHR=hi)
//   mltu.w %rd, %rs  op1=011 op2=10 — 32×32→64-bit unsigned   (ALR=lo, AHR=hi)
//===----------------------------------------------------------------------===//

// Class 5, two-register format (multiply and special reg transfers)
class S1C33InstC5Reg2<bits<3> op1, bits<2> op2, dag outs, dag ins,
                      string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b101> {
  bits<4> rs;
  bits<4> rd;
  let Inst{12-10} = op1;
  let Inst{9-8}   = op2;
  let Inst{7-4}   = rs;
  let Inst{3-0}   = rd;
}

//===----------------------------------------------------------------------===//
// CLASS 5 — Bit operation format (btst/bclr/bset/bnot)
//
// Format: |101|op1(3)|00(2)|rb(4)|0|imm3(3)|
//   bits[15:13]=101, bits[12:10]=op1, bits[9:8]=00,
//   bits[7:4]=rb, bits[3]=0, bits[2:0]=imm3
//===----------------------------------------------------------------------===//

class S1C33InstC5BitOp<bits<3> op1, dag outs, dag ins,
                       string asmstr, list<dag> pat>
    : S1C33Inst<outs, ins, asmstr, pat, 0b101> {
  bits<4> rb;
  bits<3> imm3;
  let Inst{12-10} = op1;
  let Inst{9-8}   = 0b00;
  let Inst{7-4}   = rb;
  let Inst{3}     = 0;
  let Inst{2-0}   = imm3;
}

//===----------------------------------------------------------------------===//
// CLASS 6 — EXT immediate extension
//
// Format: |110|imm13(13)|
//   bits[15:13]=110, bits[12:0]=imm13
//===----------------------------------------------------------------------===//

class S1C33InstExt : S1C33Inst<(outs), (ins i32imm:$imm13),
                                "ext\t$imm13", [], 0b110> {
  bits<13> imm13;
  let Inst{12-0} = imm13;
}
