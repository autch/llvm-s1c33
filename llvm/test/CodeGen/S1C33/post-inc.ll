; RUN: llc -march=s1c33 -O2 < %s | FileCheck %s
;
; Post-increment addressing: ld.X %rd, [%rb]+ and ld.X [%rb]+, %rs
; (CPU Manual §4.3).  The DAG combiner fuses (load ptr) + (add ptr, size) into
; an IndexedLoadSDNode / IndexedStoreSDNode with AM=POST_INC; S1C33's
; tryIndexedLoad / tryIndexedStore then pick the right opcode.

;-------------------------------------------------------------------------------
; i32 post-inc load AND post-inc store fused in a memcpy-style loop.
;-------------------------------------------------------------------------------
define void @copy_words(ptr %dst, ptr %src, i32 %n) {
; CHECK-LABEL: copy_words:
; CHECK: ld.w %r{{[0-9]+}}, [%r{{[0-9]+}}]+
; CHECK: ld.w [%r{{[0-9]+}}]+, %r{{[0-9]+}}
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %sp = phi ptr [%src, %entry], [%sp1, %loop]
  %dp = phi ptr [%dst, %entry], [%dp1, %loop]
  %v = load i32, ptr %sp
  %sp1 = getelementptr i32, ptr %sp, i32 1
  store i32 %v, ptr %dp
  %dp1 = getelementptr i32, ptr %dp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

;-------------------------------------------------------------------------------
; i8 sign-extending post-inc load (ld.b).
;-------------------------------------------------------------------------------
define i32 @sum_sbytes(ptr %p, i32 %n) {
; CHECK-LABEL: sum_sbytes:
; CHECK: ld.b %r{{[0-9]+}}, [%r{{[0-9]+}}]+
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %acc = phi i32 [0, %entry], [%acc1, %loop]
  %pp = phi ptr [%p, %entry], [%pp1, %loop]
  %b = load i8, ptr %pp
  %bs = sext i8 %b to i32
  %acc1 = add i32 %acc, %bs
  %pp1 = getelementptr i8, ptr %pp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %acc
}

;-------------------------------------------------------------------------------
; i8 zero-extending post-inc load (ld.ub).
;-------------------------------------------------------------------------------
define i32 @sum_ubytes(ptr %p, i32 %n) {
; CHECK-LABEL: sum_ubytes:
; CHECK: ld.ub %r{{[0-9]+}}, [%r{{[0-9]+}}]+
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %acc = phi i32 [0, %entry], [%acc1, %loop]
  %pp = phi ptr [%p, %entry], [%pp1, %loop]
  %b = load i8, ptr %pp
  %bz = zext i8 %b to i32
  %acc1 = add i32 %acc, %bz
  %pp1 = getelementptr i8, ptr %pp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %acc
}

;-------------------------------------------------------------------------------
; i16 sign-extending post-inc load (ld.h).
;-------------------------------------------------------------------------------
define i32 @sum_shalfs(ptr %p, i32 %n) {
; CHECK-LABEL: sum_shalfs:
; CHECK: ld.h %r{{[0-9]+}}, [%r{{[0-9]+}}]+
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %acc = phi i32 [0, %entry], [%acc1, %loop]
  %pp = phi ptr [%p, %entry], [%pp1, %loop]
  %h = load i16, ptr %pp
  %hs = sext i16 %h to i32
  %acc1 = add i32 %acc, %hs
  %pp1 = getelementptr i16, ptr %pp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %acc
}

;-------------------------------------------------------------------------------
; i16 zero-extending post-inc load (ld.uh).
;-------------------------------------------------------------------------------
define i32 @sum_uhalfs(ptr %p, i32 %n) {
; CHECK-LABEL: sum_uhalfs:
; CHECK: ld.uh %r{{[0-9]+}}, [%r{{[0-9]+}}]+
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %acc = phi i32 [0, %entry], [%acc1, %loop]
  %pp = phi ptr [%p, %entry], [%pp1, %loop]
  %h = load i16, ptr %pp
  %hz = zext i16 %h to i32
  %acc1 = add i32 %acc, %hz
  %pp1 = getelementptr i16, ptr %pp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %acc
}

;-------------------------------------------------------------------------------
; i8 truncating post-inc store (ld.b [%rb]+, %rs).
;-------------------------------------------------------------------------------
define void @fill_bytes(ptr %p, i32 %val, i32 %n) {
; CHECK-LABEL: fill_bytes:
; CHECK: ld.b [%r{{[0-9]+}}]+, %r{{[0-9]+}}
entry:
  %b = trunc i32 %val to i8
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %pp = phi ptr [%p, %entry], [%pp1, %loop]
  store i8 %b, ptr %pp
  %pp1 = getelementptr i8, ptr %pp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

;-------------------------------------------------------------------------------
; i16 truncating post-inc store (ld.h [%rb]+, %rs).
;-------------------------------------------------------------------------------
define void @fill_halfs(ptr %p, i32 %val, i32 %n) {
; CHECK-LABEL: fill_halfs:
; CHECK: ld.h [%r{{[0-9]+}}]+, %r{{[0-9]+}}
entry:
  %h = trunc i32 %val to i16
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i1, %loop]
  %pp = phi ptr [%p, %entry], [%pp1, %loop]
  store i16 %h, ptr %pp
  %pp1 = getelementptr i16, ptr %pp, i32 1
  %i1 = add i32 %i, 1
  %done = icmp eq i32 %i1, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

;-------------------------------------------------------------------------------
; Non-matching increment (pointer bumped by something other than transfer size)
; must NOT be folded into a post-inc.  Here we load an i32 but bump by 1 byte
; — this cannot be expressed as ld.w [%rb]+ (which always bumps by 4).
;-------------------------------------------------------------------------------
define i32 @no_fuse_mismatch(ptr %p) {
; CHECK-LABEL: no_fuse_mismatch:
; CHECK-NOT: ld.w %r{{[0-9]+}}, [%r{{[0-9]+}}]+
entry:
  %v = load i32, ptr %p
  %pn = getelementptr i8, ptr %p, i32 1
  store ptr %pn, ptr @next_ptr
  ret i32 %v
}
@next_ptr = external global ptr
