; RUN: llc -march=s1c33 -filetype=asm < %s | FileCheck %s

; Test ISel coverage for instructions that were previously unreachable or crashing.

;--- Switch statement (BR_JT was crashing) ---
; CHECK-LABEL: switch_test:
; CHECK: cmp
; CHECK: jreq
; CHECK-NOT: .LJTI
declare void @f0()
declare void @f1()
declare void @f2()
declare void @f3()
declare void @f4()
declare void @f5()
declare void @f6()
declare void @f7()

define void @switch_test(i32 %x) {
entry:
  switch i32 %x, label %def [
    i32 0, label %c0
    i32 1, label %c1
    i32 2, label %c2
    i32 3, label %c3
    i32 4, label %c4
    i32 5, label %c5
    i32 6, label %c6
    i32 7, label %c7
  ]
c0: call void @f0() br label %def
c1: call void @f1() br label %def
c2: call void @f2() br label %def
c3: call void @f3() br label %def
c4: call void @f4() br label %def
c5: call void @f5() br label %def
c6: call void @f6() br label %def
c7: call void @f7() br label %def
def:
  ret void
}

;--- Sign-extend i8 uses ld.b (was 6 shifts) ---
; CHECK-LABEL: sext_i8:
; CHECK: ld.b %r10, %r12
; CHECK-NOT: sll
; CHECK-NOT: sra
define i32 @sext_i8(i32 %x) {
  %trunc = trunc i32 %x to i8
  %ext = sext i8 %trunc to i32
  ret i32 %ext
}

;--- Sign-extend i16 uses ld.h (was 4 shifts) ---
; CHECK-LABEL: sext_i16:
; CHECK: ld.h %r10, %r12
; CHECK-NOT: sll
; CHECK-NOT: sra
define i32 @sext_i16(i32 %x) {
  %trunc = trunc i32 %x to i16
  %ext = sext i16 %trunc to i32
  ret i32 %ext
}

;--- Bitreverse uses mirror instruction ---
; CHECK-LABEL: bitreverse:
; CHECK: mirror %r10, %r12
declare i32 @llvm.bitreverse.i32(i32)
define i32 @bitreverse(i32 %x) {
  %r = call i32 @llvm.bitreverse.i32(i32 %x)
  ret i32 %r
}

;--- Bswap doesn't crash (expanded to shifts) ---
; CHECK-LABEL: bswap:
; CHECK-NOT: Cannot select
; CHECK: ret
declare i32 @llvm.bswap.i32(i32)
define i32 @bswap(i32 %x) {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}
