; RUN: llc -mtriple=s1c33-none-elf -O2 -o - %s | FileCheck %s
;
; ADJFI / ADD(FrameIndex, imm) expansion used to emit raw EXT instructions from
; eliminateFrameIndex(). With the post-RA scheduler enabled, those prefixes
; could be detached from their ADD target and slide into the next address
; materialization sequence, producing three consecutive ext instructions.
;
; Keep large frame-address adjustments in the late-expanded ADD_rri pseudo so
; ext+add stays adjacent until pre-emit expansion.

declare void @use(ptr)

define void @large_frame_call() {
; CHECK-LABEL: large_frame_call:
; CHECK:      ld.w %r12, %sp
; CHECK-NEXT: ext 320
; CHECK-NEXT: add %r12, %r12
; CHECK:      call use
entry:
  %obj = alloca [16 x i32], align 4
  %pad = alloca [80 x i32], align 4
  call void @use(ptr %obj)
  store i32 0, ptr %pad, align 4
  ret void
}
