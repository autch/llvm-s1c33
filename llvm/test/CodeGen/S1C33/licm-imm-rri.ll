; RUN: llc -march=s1c33 -filetype=asm %s -o - | FileCheck %s

; S1C33HoistImmInLoop: a constant used >=2 times inside a natural loop via the
; *_rri pseudos (ADD_rri/SUB_rri/AND_rri/OR_rri/XOR_rri) gets materialized once
; in the preheader and the in-loop uses become 2-address reg-reg ALU.
;
; This complements licm-const.ll which covers the two-ext range (>= 2^18).
; This file covers the SINGLE-ext range (e.g. 0xFFF = 4095).

;------------------------------------------------------------------------------
; AND with 4095 used twice in the loop -> materialized once in preheader.
;------------------------------------------------------------------------------
; CHECK-LABEL: and_4095_twice:
; The 4095 materialization (ext 63 / ld.w %rN, -1 -- sign-ext form of 4095)
; appears BEFORE the loop label.
; CHECK:      ext 63
; CHECK-NEXT: ld.w {{%r[0-9]+}}, -1
; CHECK:      .LBB0_{{[0-9]+}}:
; Inside the loop body: the AND is reg-reg, no `ext 4095` materialization.
; CHECK-NOT:  ext 4095
; CHECK:      and %r{{[0-9]+}}, %r{{[0-9]+}}
; CHECK:      and %r{{[0-9]+}}, %r{{[0-9]+}}
define void @and_4095_twice(ptr %a, ptr %b, i32 %n) {
entry:
  %cmp.entry = icmp sgt i32 %n, 0
  br i1 %cmp.entry, label %loop, label %exit
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %pa = getelementptr i32, ptr %a, i32 %i
  %pb = getelementptr i32, ptr %b, i32 %i
  %va = load i32, ptr %pa
  %vb = load i32, ptr %pb
  %ma = and i32 %va, 4095
  %mb = and i32 %vb, 4095
  store i32 %ma, ptr %pa
  store i32 %mb, ptr %pb
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret void
}

;------------------------------------------------------------------------------
; OR with 0x3F0 used twice -> hoisted.
;------------------------------------------------------------------------------
; CHECK-LABEL: or_x3f0_twice:
; CHECK:      ext
; CHECK:      ld.w {{%r[0-9]+}},
; CHECK:      .LBB1_{{[0-9]+}}:
; CHECK-NOT:  ext 1008
; CHECK:      or %r{{[0-9]+}}, %r{{[0-9]+}}
; CHECK:      or %r{{[0-9]+}}, %r{{[0-9]+}}
define void @or_x3f0_twice(ptr %a, ptr %b, i32 %n) {
entry:
  %cmp.entry = icmp sgt i32 %n, 0
  br i1 %cmp.entry, label %loop, label %exit
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %pa = getelementptr i32, ptr %a, i32 %i
  %pb = getelementptr i32, ptr %b, i32 %i
  %va = load i32, ptr %pa
  %vb = load i32, ptr %pb
  %ma = or i32 %va, 1008
  %mb = or i32 %vb, 1008
  store i32 %ma, ptr %pa
  store i32 %mb, ptr %pb
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret void
}

;------------------------------------------------------------------------------
; XOR with 255 used twice -> hoisted (still outside imm6 sign6 range).
;------------------------------------------------------------------------------
; CHECK-LABEL: xor_255_twice:
; CHECK:      .LBB2_{{[0-9]+}}:
; CHECK-NOT:  ext {{[0-9]+}}
; CHECK:      xor %r{{[0-9]+}}, %r{{[0-9]+}}
; CHECK:      xor %r{{[0-9]+}}, %r{{[0-9]+}}
define void @xor_255_twice(ptr %a, ptr %b, i32 %n) {
entry:
  %cmp.entry = icmp sgt i32 %n, 0
  br i1 %cmp.entry, label %loop, label %exit
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %pa = getelementptr i32, ptr %a, i32 %i
  %pb = getelementptr i32, ptr %b, i32 %i
  %va = load i32, ptr %pa
  %vb = load i32, ptr %pb
  %ma = xor i32 %va, 255
  %mb = xor i32 %vb, 255
  store i32 %ma, ptr %pa
  store i32 %mb, ptr %pb
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret void
}

;------------------------------------------------------------------------------
; ADD with 256 used twice -> hoisted.
;------------------------------------------------------------------------------
; CHECK-LABEL: add_256_twice:
; CHECK:      .LBB3_{{[0-9]+}}:
; CHECK-NOT:  ext 256
; CHECK:      add %r{{[0-9]+}}, %r{{[0-9]+}}
; CHECK:      add %r{{[0-9]+}}, %r{{[0-9]+}}
define void @add_256_twice(ptr %a, ptr %b, i32 %n) {
entry:
  %cmp.entry = icmp sgt i32 %n, 0
  br i1 %cmp.entry, label %loop, label %exit
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %pa = getelementptr i32, ptr %a, i32 %i
  %pb = getelementptr i32, ptr %b, i32 %i
  %va = load i32, ptr %pa
  %vb = load i32, ptr %pb
  %ma = add i32 %va, 256
  %mb = add i32 %vb, 256
  store i32 %ma, ptr %pa
  store i32 %mb, ptr %pb
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret void
}

;------------------------------------------------------------------------------
; Negative: AND with 4095 used only ONCE inside the loop must NOT be hoisted.
; Hoisting a single use buys nothing (preheader 2 inst + loop 1 inst = 3,
; vs. AND_rri = ext + and = 2 inst per use). The pass requires count >= 2.
;------------------------------------------------------------------------------
; CHECK-LABEL: and_4095_once:
; CHECK:      .LBB4_{{[0-9]+}}:
; The constant should remain inline as ext + and %rd, imm6.
; CHECK:      ext
; CHECK:      and %r{{[0-9]+}},
define void @and_4095_once(ptr %a, i32 %n) {
entry:
  %cmp.entry = icmp sgt i32 %n, 0
  br i1 %cmp.entry, label %loop, label %exit
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %pa = getelementptr i32, ptr %a, i32 %i
  %va = load i32, ptr %pa
  %ma = and i32 %va, 4095
  store i32 %ma, ptr %pa
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit
exit:
  ret void
}

;------------------------------------------------------------------------------
; Negative: AND in straight-line (no loop) must NOT be hoisted.
; Hoisting outside a loop trades a 2-instruction sequence for a 3-instruction
; sequence and is a regression.
;------------------------------------------------------------------------------
; CHECK-LABEL: and_4095_straight:
; The and uses the inline immediate form.
; CHECK:      ext
; CHECK:      and %r{{[0-9]+}},
; Make sure no MOV materialization (ld.w with -1) appears.
; CHECK-NOT:  ld.w {{%r[0-9]+}}, -1
define i32 @and_4095_straight(i32 %a, i32 %b) {
  %m1 = and i32 %a, 4095
  %m2 = and i32 %b, 4095
  %r = add i32 %m1, %m2
  ret i32 %r
}
