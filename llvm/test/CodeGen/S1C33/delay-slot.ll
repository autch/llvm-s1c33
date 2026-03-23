; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Delay slot filler tests for S1C33.
;
; Constraints (CPU manual / DESIGN_SPEC §4.3 / errata §1):
;   - jp.d %rb is FORBIDDEN (hardware DMA bug) — never generated.
;   - call.d %rb and ret.d are safe.
;   - Delay slot: 1-cycle, no memory access, no ext prefix, no branch.
;   - If no valid candidate, a nop is inserted.

;-------------------------------------------------------------------------------
; Case 1: ret.d with a useful instruction in the delay slot.
; The return-value setup (ld.w %r10, %r12 — register copy) is moved to the slot.
;-------------------------------------------------------------------------------
; CHECK-LABEL: useful_slot:
; CHECK:       ret.d
; CHECK-NEXT:  ld.w %r10, %r12
define i32 @useful_slot(i32 %x) {
  ret i32 %x
}

;-------------------------------------------------------------------------------
; Case 2: ret.d with nop — empty function has no instruction before ret.
;-------------------------------------------------------------------------------
; CHECK-LABEL: nop_slot:
; CHECK:       ret.d
; CHECK-NEXT:  nop
define void @nop_slot() {
  ret void
}

;-------------------------------------------------------------------------------
; Case 3: jp.d — unconditional branch uses delayed form.
; Tested with -O0 to prevent the branch optimizer from eliminating the jump.
;-------------------------------------------------------------------------------
; RUN: llc -mtriple=s1c33-none-elf -O0 -o - %s | FileCheck %s --check-prefix=CHECK-JPD
; CHECK-JPD-LABEL: jpd_uncond:
; CHECK-JPD:       jp.d
; CHECK-JPD-NEXT:  nop
declare i32 @callee_a()
declare i32 @callee_b()
define i32 @jpd_uncond(i32 %x) {
entry:
  %cmp = icmp eq i32 %x, 0
  br i1 %cmp, label %then, label %else
then:
  %a = call i32 @callee_a()
  br label %merge
else:
  %b = call i32 @callee_b()
  br label %merge
merge:
  %r = phi i32 [ %a, %then ], [ %b, %else ]
  ret i32 %r
}
