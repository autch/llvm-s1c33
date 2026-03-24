; RUN: llc -march=s1c33 -O2 -filetype=asm %s -o - | FileCheck %s
;
; Test: ext+[%rb] offset addressing mode (improvement A)
;
; S1C33 supports:  ext N; ld.x [%rb]  which reads from [%rb + N]
; without modifying %rb.  Previously clang emitted add+load which
; destroyed the base register.

; CHECK-LABEL: read4bytes:
; Base pointer must be unchanged: offsets 1/2/3 use ext+load, NOT add+load.
; The scheduler may reorder independent loads, so use CHECK-DAG for the
; ext+load pairs and verify no add modifies the base.
; CHECK-DAG: ext 1
; CHECK-DAG: ext 2
; CHECK-DAG: ext 3
; CHECK-NOT: add {{%r[0-9]+}}, 1
; CHECK-NOT: add {{%r[0-9]+}}, 2
; CHECK-NOT: add {{%r[0-9]+}}, 3
define i32 @read4bytes(ptr %p) {
  %p0 = getelementptr i8, ptr %p, i32 0
  %p1 = getelementptr i8, ptr %p, i32 1
  %p2 = getelementptr i8, ptr %p, i32 2
  %p3 = getelementptr i8, ptr %p, i32 3
  %b0 = load i8, ptr %p0
  %b1 = load i8, ptr %p1
  %b2 = load i8, ptr %p2
  %b3 = load i8, ptr %p3
  %e0 = zext i8 %b0 to i32
  %e1 = zext i8 %b1 to i32
  %e2 = zext i8 %b2 to i32
  %e3 = zext i8 %b3 to i32
  %s01 = or i32 %e0, %e1
  %s012 = or i32 %s01, %e2
  %r = or i32 %s012, %e3
  ret i32 %r
}

; CHECK-LABEL: sum_fields:
; All four fields accessed via ext+ld.w, base pointer unchanged.
; CHECK-DAG: ext 4
; CHECK-DAG: ext 8
; CHECK-DAG: ext 12
; CHECK-NOT: add {{%r[0-9]+}}, 4
; CHECK-NOT: add {{%r[0-9]+}}, 8
; CHECK-NOT: add {{%r[0-9]+}}, 12
%Header = type { i32, i32, i32, i32 }
define i32 @sum_fields(ptr %h) {
  %f0 = getelementptr %Header, ptr %h, i32 0, i32 0
  %f1 = getelementptr %Header, ptr %h, i32 0, i32 1
  %f2 = getelementptr %Header, ptr %h, i32 0, i32 2
  %f3 = getelementptr %Header, ptr %h, i32 0, i32 3
  %v0 = load i32, ptr %f0
  %v1 = load i32, ptr %f1
  %v2 = load i32, ptr %f2
  %v3 = load i32, ptr %f3
  %s01 = add i32 %v0, %v1
  %s012 = add i32 %s01, %v2
  %r = add i32 %s012, %v3
  ret i32 %r
}

; CHECK-LABEL: write3bytes:
; Stores to ptr+1, ptr+2, ptr+3 use ext+store, NOT add+store.
; CHECK-DAG: ext 1
; CHECK-DAG: ext 2
; CHECK-DAG: ext 3
; CHECK-NOT: add {{%r[0-9]+}}, 1
; CHECK-NOT: add {{%r[0-9]+}}, 2
; CHECK-NOT: add {{%r[0-9]+}}, 3
define void @write3bytes(ptr %p, i8 %a, i8 %b, i8 %c) {
  %p1 = getelementptr i8, ptr %p, i32 1
  %p2 = getelementptr i8, ptr %p, i32 2
  %p3 = getelementptr i8, ptr %p, i32 3
  store i8 %a, ptr %p1
  store i8 %b, ptr %p2
  store i8 %c, ptr %p3
  ret void
}
