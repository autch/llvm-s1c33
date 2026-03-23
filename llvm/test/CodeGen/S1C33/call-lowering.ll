; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; LowerCall verification: confirm that arguments are placed in R12→R13 per
; the S5U1C33000C ABI, a 'call' instruction is emitted, and the return value
; is used from R10.

declare i32 @add(i32 %a, i32 %b)

;-------------------------------------------------------------------------------
; call add(1, 2) — verify arg placement and call instruction
;-------------------------------------------------------------------------------

; CHECK-LABEL: call_add:
; Arg 1 (1) → R12, arg 2 (2) → R13
; CHECK: ld.w %r12, 1
; CHECK: ld.w %r13, 2
; CHECK: call add
; Return value already in R10 after call; function returns it directly.
; CHECK: ret.d
define i32 @call_add() {
  %r = call i32 @add(i32 1, i32 2)
  ret i32 %r
}

;-------------------------------------------------------------------------------
; call with register arguments — args passed through from caller's R12/R13
;-------------------------------------------------------------------------------

; CHECK-LABEL: call_passthrough:
; %a is in R12, %b is in R13 from the caller.
; After setting up args for the callee (they're already in R12/R13):
; CHECK: call add
; CHECK: ret.d
define i32 @call_passthrough(i32 %a, i32 %b) {
  %r = call i32 @add(i32 %a, i32 %b)
  ret i32 %r
}
