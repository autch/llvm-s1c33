; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Callee-saved register (R0–R3) save/restore via pushn/popn.
;
; S5U1C33000C ABI: R0–R3 are callee-saved.
; pushn %rN pushes R0..RN;  popn %rN pops RN..R0.
;
; When a value must survive a call (caller-saved R12–R15 are clobbered),
; the register allocator places it in a callee-saved register (R0–R3).
; emitPrologue emits pushn to save it; emitEpilogue emits popn to restore it.

declare i32 @callee()

;-------------------------------------------------------------------------------
; use_callee_saved_reg: %a (in R12) must survive call @callee().
; R12 is clobbered by the call (not callee-saved), so LLVM allocates R0
; (first callee-saved register) to hold %a across the call.
;
; Expected prologue/epilogue:
;   pushn %r0       ← save R0 before modifying it
;   ...
;   popn %r0        ← restore R0 before returning
;   ret
;-------------------------------------------------------------------------------

; CHECK-LABEL: use_callee_saved_reg:
; CHECK: pushn %r{{[0-3]}}
; CHECK: call callee
; CHECK: popn %r{{[0-3]}}
; CHECK: ret
define i32 @use_callee_saved_reg(i32 %a) {
  %r = call i32 @callee()
  %s = add i32 %r, %a
  ret i32 %s
}

;-------------------------------------------------------------------------------
; multi_callee_saved: use multiple callee-saved registers across a call.
; 4 args (%a–%d in R12–R15) must survive; LLVM will use R0, R1, R2, R3.
; Expects pushn %r3 (pushes R0, R1, R2, R3) in prologue.
;-------------------------------------------------------------------------------

; CHECK-LABEL: multi_callee_saved:
; CHECK: pushn %r{{[0-3]}}
; CHECK: call callee
; CHECK: popn %r{{[0-3]}}
; CHECK: ret
define i32 @multi_callee_saved(i32 %a, i32 %b, i32 %c, i32 %d) {
  %r = call i32 @callee()
  %s = add i32 %r, %a
  %t = add i32 %s, %b
  %u = add i32 %t, %c
  %v = add i32 %u, %d
  ret i32 %v
}
