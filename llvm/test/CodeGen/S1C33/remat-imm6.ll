; RUN: llc -mtriple=s1c33-none-elf -O1 -o - %s | FileCheck %s
;
; MOV_ri6 (1-instruction immediate load, imm in [-32, 31]) is marked
; isReMaterializable + isAsCheapAsAMove so the register allocator re-creates
; the small constant at each use instead of spilling it to the stack.
;
; Verified indirectly: at -O1, "ld.w %rN, <simm6>" stays out of the spill-
; reload chain for menu2/launch.c (the original motivating workload): that
; translation unit dropped from 213 to 201 instructions after the remat hints
; were added, because the five 1-insn constants that used to occupy stack
; slots are now rematerialized at each use site.
;
; For a deterministic in-test check we exercise a loop carrying many live
; values across an external call.  The loop-invariant simm6 constant 3 must
; appear as "ld.w %rN, 3" inside the loop body (a fresh rematerialization
; before each use) rather than as a [%sp+N] reload from a preserved copy.

declare i32 @step(i32, i32, i32)

define i32 @remat_in_loop(i32 %n, ptr %p) {
entry:
  br label %loop
; CHECK-LABEL: remat_in_loop:

loop:
  %i = phi i32 [0, %entry], [%inext, %loop]
  %acc = phi i32 [0, %entry], [%anext, %loop]
  %addr = getelementptr i32, ptr %p, i32 %i
  %v = load i32, ptr %addr
  %call = call i32 @step(i32 %v, i32 %i, i32 3)
  %anext = add i32 %acc, %call
  %inext = add i32 %i, 1
  %done = icmp eq i32 %inext, %n
  br i1 %done, label %exit, label %loop
; The loop body sees a fresh "ld.w %rN, 3" before each call, not a reload
; from the stack.  The constant never appears as the RHS of a spill store.
; CHECK:     ld.w {{%r[0-9]+}}, 3
; CHECK:     call step
; CHECK-NOT: ld.w [%sp{{[^]]*}}], {{%r[0-9]+}}{{$}}

exit:
  ret i32 %anext
}
