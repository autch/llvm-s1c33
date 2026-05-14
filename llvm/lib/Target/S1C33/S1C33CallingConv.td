//===-- S1C33CallingConv.td - S1C33 Calling Convention ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S5U1C33000C (gcc33) calling convention — see DESIGN_SPEC §3.
//
// Arguments: R12 → R13 → R14 → R15 (overflow to stack)
// Return: R10 (32-bit), R10+R11 (64-bit)
// Callee-saved: R0, R1, R2, R3
// Reserved: R8 (GP/kernel table base)
//
//===----------------------------------------------------------------------===//

// S1C33 C return-value convention.
def RetCC_S1C33 : CallingConv<[
  CCIfType<[i32, f32], CCAssignToReg<[R10, R11]>>,
  CCAssignToStack<4, 4>
]>;

// S1C33 C calling convention.
def CC_S1C33 : CallingConv<[
  // gcc33 rule: a split i32 is the lo half of an i64/f64 pair.
  // Both halves must go into consecutive registers, or both must go to stack.
  // If fewer than 2 argument registers remain, the whole pair goes to stack.
  // CC_S1C33_AssignDoubleHalf handles this: returns false (handled/stack) when
  // fewer than 2 regs are free, or returns true (fall-through) when a pair is
  // available so the normal CCAssignToReg below allocates the lo half.
  CCIfSplit<CCCustom<"CC_S1C33_AssignDoubleHalf">>,
  CCIfType<[i32, f32], CCAssignToReg<[R12, R13, R14, R15]>>,
  CCAssignToStack<4, 4>
]>;

// Variadic calls do NOT have a dedicated CallingConv def.
//
// gcc33 ABI: when the callee is variadic, every named argument EXCEPT the
// last one is passed per the normal CC_S1C33 (R12-R15, overflow to stack);
// the LAST named argument and all variadic arguments are passed on the
// stack, contiguously.  This lets the callee's va_start(ap, lastnamed) —
// which takes &lastnamed — find the variadic arguments immediately after it.
//
// This rule cannot be expressed in TableGen because the CC infrastructure
// processes one argument at a time and has no notion of "the last named
// parameter".  It is implemented in C++ in S1C33ISelLowering.cpp
// (LowerCall / LowerFormalArguments), which splits the argument list at the
// last named parameter and runs CC_S1C33 on the register part.
//
// The earlier all-arguments-on-the-stack convention was only accidentally
// correct for printf, whose sole named argument IS the last one; it broke
// sprintf/fprintf/etc., whose earlier named arguments (the destination
// buffer / stream) must stay in registers to match the gcc33-built kernel
// and SDK library implementations.

// Callee-saved registers: R0–R3 per S5U1C33000C ABI.
def CSR_S1C33 : CalleeSavedRegs<(add R0, R1, R2, R3)>;
