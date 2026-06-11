; RUN: not llvm-mc -triple=s1c33-none-elf -mcpu=generic %s 2>&1 | FileCheck %s
; RUN: llvm-mc -triple=s1c33-none-elf -mcpu=s1c33209 %s -o /dev/null
;
; Multiplier/MAC instructions require FeatureHWMul (s1c33209 has it,
; generic does not). The second RUN line proves the same input assembles
; cleanly once the feature is enabled.

mlt.w %r1, %r2 ; CHECK: :[[@LINE]]:1: error: instruction requires a CPU feature not currently enabled
mltu.w %r1, %r2 ; CHECK: :[[@LINE]]:1: error: instruction requires a CPU feature not currently enabled
mlt.h %r1, %r2 ; CHECK: :[[@LINE]]:1: error: instruction requires a CPU feature not currently enabled
mltu.h %r1, %r2 ; CHECK: :[[@LINE]]:1: error: instruction requires a CPU feature not currently enabled
mac %r1 ; CHECK: :[[@LINE]]:1: error: instruction requires a CPU feature not currently enabled
