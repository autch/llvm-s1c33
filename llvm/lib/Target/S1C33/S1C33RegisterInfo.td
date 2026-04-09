//===-- S1C33RegisterInfo.td - S1C33 Register defs -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33000 CPU Core Manual §3: Register File
//
// General-purpose registers: R0–R15 (32-bit × 16)
// Special registers: SP (stack pointer), PC, PSR, ALR, AHR
//
// ABI (S5U1C33000C):
//   R0–R3   : Callee-saved
//   R4–R7   : Scratch (caller-saved)
//   R8      : Kernel table base pointer — always 0x0 per kernel calling convention.
//             The P/ECE kernel sets R8 = 0x0 before calling any application callback.
//             pceapi stubs use "ext N / ld.w %r9, [%r8]" to fetch function pointers
//             from the kernel jump table at 0x0 + N.  User-compiled code must NEVER
//             modify R8.  Reserved (not allocatable) to enforce this.
//             Note: this is NOT a MIPS-style user-data GP — the "GP optimization"
//             (pointing R8 at user .sdata) is not implemented for user apps.
//             Future: kernel startup code must explicitly set R8 = 0 in assembly.
//   R9      : Scratch (caller-saved), same as R4-R7.
//             All P/ECE kernel interrupt handlers save/restore R0-R15 via
//             pushn %r15 / popn %r15 (INT_BEGIN/INT_END macros), so R9 is
//             safe to use as a scratch register.  pceapi stubs already treat
//             R9 as caller-saved (ext33 ABI convention).
//   R10     : Return value (32-bit / 64-bit low word)
//   R11     : Return value (64-bit high word)
//   R12–R15 : Arguments (up to 4 words)
//   SP      : Stack pointer (special, separate register file)
//   PC      : Program counter
//   PSR     : Processor Status Register (N, Z, V, C flags)
//   ALR     : Arithmetic Low Register  (multiply/divide/MAC result low 32)
//   AHR     : Arithmetic High Register (multiply/divide/MAC result high 32)
//
//===----------------------------------------------------------------------===//

// Registers are identified with a numeric encoding.
// HWEncoding is bits<16> in the LLVM Register base class.
class S1C33Reg<bits<16> Enc, string n, list<string> altNames = []>
    : Register<n, altNames> {
  let HWEncoding = Enc;
  let Namespace = "S1C33";
}

// General-purpose registers R0–R15.
// CPU Manual §3.1: 16 × 32-bit general-purpose registers.
foreach i = 0-15 in {
  def R#i : S1C33Reg<i, "r"#i>, DwarfRegNum<[i]>;
}

// Special registers (not in GPR encoding space).
// CPU Manual §3.2: SP, PC, PSR, ALR, AHR.
//
// DWARF register numbering (this project's convention):
//   SP=16, PC=17, PSR=18, ALR=19, AHR=20
//
// Note: the S1C33 %ss file has its own order (PSR=0, SP=1, ALR=2, AHR=3) and
// PC is not part of %ss at all.  Neither hardware order maps cleanly to a
// simple sequential scheme, so we assign DWARF numbers after the 16 GPRs in
// logical order (SP first, then PC) as the canonical convention for this
// toolchain.
//
// GDB RSP emulators must output the 'g' packet in this DWARF order:
//   r0–r15, sp, pc, psr, alr, ahr
// and the qXfer target-description XML must list registers in the same order.
def SP  : S1C33Reg<16, "sp">,  DwarfRegNum<[16]>;
def PC  : S1C33Reg<17, "pc">,  DwarfRegNum<[17]>;
def PSR : S1C33Reg<18, "psr">, DwarfRegNum<[18]>;
def ALR : S1C33Reg<19, "alr">, DwarfRegNum<[19]>;
def AHR : S1C33Reg<20, "ahr">, DwarfRegNum<[20]>;

//===----------------------------------------------------------------------===//
// Register Classes
//===----------------------------------------------------------------------===//

// GR32: Allocatable general-purpose registers.
// Allocation order: caller-saved first (prefer spilling cheapest), then
// callee-saved, then special-purpose (arg/retval registers) last.
// R8 is excluded (reserved: kernel table base pointer).
def GR32 : RegisterClass<"S1C33", [i32], 32,
    (add // Scratch (caller-saved, free to use first)
         R4, R5, R6, R7, R9,
         // Callee-saved (allocate before arg/retval to preserve them)
         R0, R1, R2, R3,
         // Return value registers
         R10, R11,
         // Argument registers (highest cost to spill)
         R12, R13, R14, R15
    )> {
  let DecoderMethod = "DecodeGR32RegisterClass";
}

// SPREG: Stack pointer register class. Not allocatable.
// Needed so getCopyFromReg/CopyToReg can move SP to/from a virtual register.
def SPREG : RegisterClass<"S1C33", [i32], 32, (add SP)> {
  let CopyCost = -1;
  let isAllocatable = 0;
}

// CCR: Condition code / flag register (PSR). Not allocatable.
def CCR : RegisterClass<"S1C33", [i32], 32, (add PSR)> {
  let CopyCost = -1;
  let isAllocatable = 0;
}

// ACCR: Multiply/accumulate result registers (ALR=lo, AHR=hi). Not allocatable.
// Written by mlt.w/mltu.w/mlt.h/mltu.h; read by ld.w %rd, %alr/ahr.
def ACCR : RegisterClass<"S1C33", [i32], 32, (add ALR, AHR)> {
  let CopyCost = -1;
  let isAllocatable = 0;
}
