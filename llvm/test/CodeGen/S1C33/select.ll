; RUN: llc -march=s1c33 %s -o - | FileCheck %s
;
; Test ISD::SELECT (boolean condition) lowering.
; S1C33 has no conditional-move; SELECT is lowered to SELECT_CC via CMP + branch.

target triple = "s1c33-none-elf"
target datalayout = "e-m:e-p:32:32-a:0:32-i8:8-i16:16-i32:32-n32-S32"

; select i1 (icmp sgt %x, 0), %x, %y
; Expected: compare %x with 0, branch, phi in merge block.
; CHECK-LABEL: test_select_gt:
; CHECK: cmp
; CHECK: jrgt
define i32 @test_select_gt(i32 %x, i32 %y) {
  %cmp = icmp sgt i32 %x, 0
  %result = select i1 %cmp, i32 %x, i32 %y
  ret i32 %result
}

; select i1 (icmp eq %a, %b), %t, %f
; CHECK-LABEL: test_select_eq:
; CHECK: cmp
; CHECK: jreq
define i32 @test_select_eq(i32 %a, i32 %b, i32 %t, i32 %f) {
  %cmp = icmp eq i32 %a, %b
  %result = select i1 %cmp, i32 %t, i32 %f
  ret i32 %result
}

; select i1 loaded_bool, %t, %f  (condition not directly from icmp)
; CHECK-LABEL: test_select_i1:
; CHECK: cmp
define i32 @test_select_i1(i1 %cond, i32 %t, i32 %f) {
  %result = select i1 %cond, i32 %t, i32 %f
  ret i32 %result
}
