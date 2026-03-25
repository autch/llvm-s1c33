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

// Stack-only calling convention used when calling variadic functions.
// gcc33 ABI: when the callee is variadic, ALL arguments (including fixed
// ones) go to the stack in left-to-right order.  No argument registers used.
def CC_S1C33_VarArg : CallingConv<[
  CCAssignToStack<4, 4>
]>;

// Callee-saved registers: R0–R3 per S5U1C33000C ABI.
def CSR_S1C33 : CalleeSavedRegs<(add R0, R1, R2, R3)>;
