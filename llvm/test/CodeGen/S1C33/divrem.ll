; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Division and remainder operations: no hardware divider on S1C33000.
; All four operations must be lowered to gcc/libgcc-compatible libcalls,
; which are also present in the P/ECE SDK idiv.lib.

; CHECK-LABEL: div_test:
; CHECK:       call __divsi3
define i32 @div_test(i32 %a, i32 %b) {
  %r = sdiv i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: udiv_test:
; CHECK:       call __udivsi3
define i32 @udiv_test(i32 %a, i32 %b) {
  %r = udiv i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: mod_test:
; CHECK:       call __modsi3
define i32 @mod_test(i32 %a, i32 %b) {
  %r = srem i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: umod_test:
; CHECK:       call __umodsi3
define i32 @umod_test(i32 %a, i32 %b) {
  %r = urem i32 %a, %b
  ret i32 %r
}
