; RUN: llc -march=s1c33 -filetype=asm %s -o - | FileCheck %s

; Phase 2: getRegAllocationHints steers vregs whose live range crosses a call
; toward the callee-saved registers R0-R3 instead of caller-saved R4-R7/R9.
; This avoids stack spill+reload around calls.

declare void @sink(i32)

;------------------------------------------------------------------------------
; A single value live across a call ends up in a CSR.
; The function pushes a callee-saved register, holds the value there across
; the call, and pops on exit. NO spill to stack should appear for the value.
;------------------------------------------------------------------------------
; CHECK-LABEL: cross_call_single:
; CHECK:      pushn %r{{[0-3]}}
; CHECK:      ld.w %r{{[0-3]}}, %r12
; CHECK:      call sink
; The value survived in CSR; no stack reload of the live-across value.
; CHECK-NOT:  ld.w %r{{[0-9]+}}, [%sp+
; CHECK:      add %r{{[0-3]}}, 8
; CHECK:      popn %r{{[0-3]}}
; CHECK:      ret
define i32 @cross_call_single(i32 %x) {
entry:
  %y = add i32 %x, 7
  call void @sink(i32 %x)
  %z = add i32 %y, 1
  ret i32 %z
}

;------------------------------------------------------------------------------
; Leaf function: NO callee-saved push/pop. The hint must not steer values
; that don't cross a call into CSR (which would force prologue/epilogue
; growth in leaf functions).
;------------------------------------------------------------------------------
; CHECK-LABEL: leaf_func:
; CHECK-NOT:  pushn
; CHECK-NOT:  popn
; CHECK:      ret
define i32 @leaf_func(i32 %a, i32 %b) {
  %x = add i32 %a, %b
  %y = mul i32 %x, 3
  ret i32 %y
}

;------------------------------------------------------------------------------
; Two values live across the same call: both should end up in CSRs (R0-R3),
; producing pushn for both, no stack spills.
;------------------------------------------------------------------------------
; CHECK-LABEL: cross_call_two:
; CHECK:      pushn %r{{[1-3]}}
; CHECK:      call sink
; Neither live value should be reloaded from stack after the call.
; CHECK-NOT:  ld.w %r{{[0-9]+}}, [%sp+
; CHECK:      popn %r{{[1-3]}}
; CHECK:      ret
define i32 @cross_call_two(i32 %a, i32 %b) {
entry:
  call void @sink(i32 0)
  %r = add i32 %a, %b
  ret i32 %r
}
