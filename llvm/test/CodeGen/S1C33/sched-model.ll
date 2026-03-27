; RUN: llc -march=s1c33 -mcpu=s1c33209 -O2 -o - %s | FileCheck %s

; Verify the post-RA scheduler uses the S1C33 scheduling model to hide
; load-use latency.  A load (latency=2) should have an unrelated
; instruction placed between it and the consumer of the loaded register.

define i32 @load_use_reorder(ptr %p, i32 %a) {
; The scheduler should place the move between the load and its use:
;   ld.w %r4, [%r12]   ← load (latency 2)
;   ld.w %r10, %r13    ← fills gap (move %a)
;   add  %r10, %r4     ← use of loaded value, 2 cycles later
;
; CHECK-LABEL: load_use_reorder:
; CHECK:       ld.w	%r4, [%r12]
; CHECK-NEXT:  ld.w	%r10, %r13
; CHECK-NEXT:  add	%r10, %r4
entry:
  %val = load i32, ptr %p
  %sum = add i32 %a, 1
  %result = add i32 %sum, %val
  ret i32 %result
}
