; RUN: llc -mtriple=s1c33-none-elf -mcpu=s1c33209 -o - %s | FileCheck %s --check-prefixes=CHECK,HWMUL
; RUN: llc -mtriple=s1c33-none-elf -mcpu=generic -o - %s | FileCheck %s --check-prefixes=CHECK,NOMUL
;
; 64-bit integer arithmetic. S1C33 has no add-with-carry instruction, so
; i64 add/sub expand inline via a setcc-based carry sequence. Multiply uses
; the hardware multiplier (mlt.w/mltu.w cross products) when available,
; otherwise the __muldi3 libcall. Division and remainder always go through
; libcalls (no hardware divider); the *di3 entry points are provided by
; compiler-rt (EPSON's gcc33-era SDK never shipped a 64-bit runtime).
;
; i64 ABI: first arg R12(lo)+R13(hi), second arg R14(lo)+R15(hi),
; return R10(lo)+R11(hi).

; CHECK-LABEL: add64:
; CHECK-NOT:   call
; lo: R10 = R12 + R14; carry detected by result < original operand
; CHECK:       add %r10, %r14
; CHECK:       cmp %r10, %r12
; CHECK:       jrult
; hi: R11 = R13 + R15 + carry
; CHECK:       add %r11, %r15
; CHECK-NOT:   call
; CHECK:       ret
define i64 @add64(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: sub64:
; CHECK-NOT:   call
; borrow detected by comparing lo operands before the subtract
; CHECK:       cmp %r10, %r14
; CHECK:       jrult
; CHECK:       sub %r11, %r15
; CHECK:       sub %r10, %r14
; CHECK-NOT:   call
; CHECK:       ret
define i64 @sub64(i64 %a, i64 %b) {
  %r = sub i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: mul64:
; NOMUL:       call __muldi3
; With the hardware multiplier, expand inline:
;   lo  = lo(a.lo * b.lo)
;   hi  = hi(a.lo * b.lo) + lo(a.lo * b.hi) + lo(a.hi * b.lo)
; HWMUL-NOT:   call
; HWMUL-DAG:   mlt.w %r12, %r15
; HWMUL-DAG:   mltu.w %r12, %r14
; HWMUL-DAG:   mlt.w %r13, %r14
; HWMUL-NOT:   call
; HWMUL:       ret
define i64 @mul64(i64 %a, i64 %b) {
  %r = mul i64 %a, %b
  ret i64 %r
}

; Args are already in R12-R15 exactly as the libcall expects, so each of
; these compiles to a bare call + ret with the result passing through
; R10/R11 untouched.

; CHECK-LABEL: sdiv64:
; CHECK:       call __divdi3
define i64 @sdiv64(i64 %a, i64 %b) {
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: udiv64:
; CHECK:       call __udivdi3
define i64 @udiv64(i64 %a, i64 %b) {
  %r = udiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: srem64:
; CHECK:       call __moddi3
define i64 @srem64(i64 %a, i64 %b) {
  %r = srem i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: urem64:
; CHECK:       call __umoddi3
define i64 @urem64(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}
