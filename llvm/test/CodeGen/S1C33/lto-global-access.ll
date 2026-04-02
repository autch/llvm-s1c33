; RUN: llc -mtriple=s1c33-none-elf -function-sections -filetype=obj -o %t %s
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s
;
; Regression test: with -function-sections (or LTO), each function lives in its
; own small section, causing MCAssembler.relaxOnce's MaxIter to be small (2).
; A two-step EXT0→EXT1→EXT2 relaxation chain exhausted MaxIter before the final
; layoutSection call, leaving stale fragment offsets and triggering an assertion:
;   MCAssembler::writeSectionData: OS.tell() - Start == getSectionAddressSize
;
; The fix: LDW_SYM_EXT0 relaxes directly to LDW_SYM_EXT2 in one step.
; Verify that global variable access emits the correct ext+ext+ld.w sequence.

@counter = global i32 0

; CHECK-LABEL: <inc>:
; CHECK:        ext
; CHECK-NEXT:   ext
; CHECK-NEXT:   ld.w

define void @inc() {
  %p = load i32, ptr @counter
  %v = add i32 %p, 1
  store i32 %v, ptr @counter
  ret void
}
