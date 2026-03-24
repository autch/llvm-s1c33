; RUN: llc -march=s1c33 -filetype=asm %s -o - | FileCheck %s

; Improvement C: constants requiring 2 ext prefixes (magnitude >= 2^18)
; should be materialized via MOV_ri32 so MachineLICM can hoist them out of
; loops, replacing repeated "ext+ext+and" (3 instr/iter) with a single
; register AND (1 instr/iter).

; CHECK-LABEL: test_and_large_in_loop:
; Two ext prefixes + ld.w appear BEFORE the loop label (hoisted out).
; CHECK:      ext
; CHECK:      ext
; CHECK-NEXT: ld.w {{%r[0-9]+}},
; Loop begins after the hoisted materialization.
; CHECK:      .LBB0_{{[0-9]+}}:
; Inside the loop: no ext before the first AND (constant is in a register).
; CHECK-NOT:  ext
; CHECK:      and %r{{[0-9]+}}, %r{{[0-9]+}}
define i32 @test_and_large_in_loop(ptr %p, i32 %n) {
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %ptr = getelementptr i32, ptr %p, i32 %i
  %val = load i32, ptr %ptr
  %masked = and i32 %val, 1048575
  store i32 %masked, ptr %ptr
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret i32 %masked
}

; CHECK-LABEL: test_or_large_in_loop:
; CHECK:      ext
; CHECK:      ext
; CHECK-NEXT: ld.w {{%r[0-9]+}},
; CHECK:      .LBB1_{{[0-9]+}}:
; CHECK-NOT:  ext
; CHECK:      or %r{{[0-9]+}}, %r{{[0-9]+}}
define i32 @test_or_large_in_loop(ptr %p, i32 %n) {
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %ptr = getelementptr i32, ptr %p, i32 %i
  %val = load i32, ptr %ptr
  %masked = or i32 %val, 786432
  store i32 %masked, ptr %ptr
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret i32 %masked
}
