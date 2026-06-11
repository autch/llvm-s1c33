; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; i64 argument and return ABI (S5U1C33000C):
;   - i64 args occupy two consecutive arg registers, lo first:
;     first i64 in R12(lo)+R13(hi), second in R14(lo)+R15(hi)
;   - i64 return in R10(lo)+R11(hi)
;   - if fewer than two arg registers remain, the whole i64 goes to the
;     stack -- never split between a register and the stack
;     (CC_S1C33_AssignDoubleHalf)
;   - incoming stack args start at [%sp+1] (word units; [%sp+0] holds the
;     return address pushed by call)

; CHECK-LABEL: ret_first:
; CHECK-DAG:   ld.w %r10, %r12
; CHECK-DAG:   ld.w %r11, %r13
define i64 @ret_first(i64 %a, i64 %b) {
  ret i64 %a
}

; CHECK-LABEL: ret_second:
; CHECK-DAG:   ld.w %r10, %r14
; CHECK-DAG:   ld.w %r11, %r15
define i64 @ret_second(i64 %a, i64 %b) {
  ret i64 %b
}

; Third i64 arg: R12-R15 are taken, so %c lives on the stack.
; CHECK-LABEL: third_on_stack:
; CHECK-DAG:   ld.w %r10, [%sp+1]
; CHECK-DAG:   ld.w %r11, [%sp+2]
define i64 @third_on_stack(i64 %a, i64 %b, i64 %c) {
  ret i64 %c
}

; i64 after one i32: no alignment padding, %a takes R13(lo)+R14(hi).
; CHECK-LABEL: mixed_i32_i64:
; CHECK-DAG:   ld.w %r10, %r13
; CHECK-DAG:   ld.w %r11, %r14
define i64 @mixed_i32_i64(i32 %x, i64 %a) {
  ret i64 %a
}

; Three i32s leave only R15: the i64 must NOT be split (R15 + stack);
; the whole value goes to the stack instead.
; CHECK-LABEL: no_split_across_reg_stack:
; CHECK-DAG:   ld.w %r10, [%sp+1]
; CHECK-DAG:   ld.w %r11, [%sp+2]
define i64 @no_split_across_reg_stack(i32 %x, i32 %y, i32 %z, i64 %a) {
  ret i64 %a
}

; Caller side: constants materialized into the register pairs, result
; consumed from R10/R11.
; CHECK-LABEL: caller:
; CHECK-DAG:   ld.w %r12, 1
; CHECK-DAG:   ld.w %r13, 0
; CHECK-DAG:   ld.w %r14, -2
; CHECK-DAG:   ld.w %r15, -1
; CHECK:       call ext_callee
declare i64 @ext_callee(i64, i64)
define i64 @caller() {
  %r = call i64 @ext_callee(i64 1, i64 -2)
  ret i64 %r
}

; i64 equality: inline two-word compare (xor halves, or, test), no libcall.
; CHECK-LABEL: cmp_eq:
; CHECK-NOT:   call
; CHECK-DAG:   xor %r13, %r15
; CHECK-DAG:   xor %r12, %r14
; CHECK:       or %r12, %r13
; CHECK:       ret
define i32 @cmp_eq(i64 %a, i64 %b) {
  %c = icmp eq i64 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}

; i64 unsigned less-than: hi compare decides unless equal, then lo compare.
; CHECK-LABEL: cmp_ult:
; CHECK-NOT:   call
; CHECK:       cmp %r13, %r15
; CHECK:       cmp %r12, %r14
; CHECK:       ret
define i32 @cmp_ult(i64 %a, i64 %b) {
  %c = icmp ult i64 %a, %b
  %r = zext i1 %c to i32
  ret i32 %r
}
