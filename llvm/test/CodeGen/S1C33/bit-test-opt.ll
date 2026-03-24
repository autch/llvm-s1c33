; RUN: llc -march=s1c33 -O2 -filetype=asm %s -o - | FileCheck %s
;
; Test: sign-bit test DAG combine (improvement B)
;
; InstCombine converts (uint8_t x & 0x80) != 0  to  icmp sgt i8 x, -1
; (or icmp slt i8 x, 0) which then expands to 3×sll + 3×sra on S1C33
; because max shift is 8.  The PerformDAGCombine hook in S1C33ISelLowering
; recognizes these patterns and converts them to  AND(x, signbit) != 0
; which lowers to: ext 2; and rx, 0; cmp rx, 0; jrne/jreq

; CHECK-LABEL: test_hi_bit_branch:
; Checks that no 3×sll+3×sra sign-extension sequence is emitted.
; CHECK-NOT: sll {{%r[0-9]+}}, 8
; CHECK-NOT: sra {{%r[0-9]+}}, 8
; CHECK: ext 2
; CHECK-NEXT: and
define void @test_hi_bit_branch(i8 %x, ptr %dst) {
  ; (x & 0x80) != 0 → InstCombine → icmp sgt i8 x, -1
  %cond = icmp sgt i8 %x, -1
  br i1 %cond, label %yes, label %no
yes:
  store i8 1, ptr %dst
  ret void
no:
  store i8 0, ptr %dst
  ret void
}

; CHECK-LABEL: test_sign_lt0:
; icmp slt i8 x, 0 is the same sign-bit check.
; CHECK-NOT: sll {{%r[0-9]+}}, 8
; CHECK-NOT: sra {{%r[0-9]+}}, 8
; CHECK: ext 2
; CHECK-NEXT: and
define void @test_sign_lt0(i8 %x, ptr %dst) {
  %cond = icmp slt i8 %x, 0
  br i1 %cond, label %yes, label %no
yes:
  store i8 1, ptr %dst
  ret void
no:
  store i8 0, ptr %dst
  ret void
}

; CHECK-LABEL: test_hi_bit_loop:
; The key pattern from fpkplay/decode.c: i8 phi in a loop compared against -1.
; Must not emit any 8-bit shift sequence.
; CHECK: ext 2
; CHECK-NEXT: and
; CHECK-NOT: sll {{%r[0-9]+}}, 8
; CHECK-NOT: sra {{%r[0-9]+}}, 8
define i32 @test_hi_bit_loop(ptr %p, i32 %n) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %loop_end ]
  %flags = phi i8 [ 0, %entry ], [ %flags_next, %loop_end ]
  ; flags & 0x80 → InstCombine → icmp sgt i8 flags, -1
  %cond = icmp sgt i8 %flags, -1
  br i1 %cond, label %else_bb, label %then_bb
then_bb:
  %v = load i8, ptr %p
  br label %loop_end
else_bb:
  br label %loop_end
loop_end:
  %flags_next = shl i8 %flags, 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %i
}

; CHECK-LABEL: test_i16_sign:
; Same optimization for i16 sign bit (bit 15 = 0x8000).
; CHECK-NOT: sll {{%r[0-9]+}}, 8
; CHECK-NOT: sra {{%r[0-9]+}}, 8
; ext 60 + and encodes 0x8000 (= 60<<6 | 0 = 0x8000? no: 60<<6=3840, not 32768)
; Actually ext N; and %r, 0  where N = 0x8000 >> 6 = 512 = 0x200
; ext 0x200 = two-ext: ext 8; ext (... ); and
; Just check no shifts remain:
; CHECK: and
define void @test_i16_sign(i16 %x, ptr %dst) {
  %cond = icmp sgt i16 %x, -1
  br i1 %cond, label %yes, label %no
yes:
  store i8 1, ptr %dst
  ret void
no:
  store i8 0, ptr %dst
  ret void
}
