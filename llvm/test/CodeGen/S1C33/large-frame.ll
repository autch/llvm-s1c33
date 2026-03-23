; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Large stack frame: verify that SP-relative accesses with offset > 63
; are prefixed with 'ext' as required by the S1C33 ISA.
;
; SP-relative instructions use a 6-bit unsigned field (range 0..63 bytes).
; For offsets 64 and above, eliminateFrameIndex inserts 'ext imm13' before
; the memory instruction so the hardware sees:
;   address = SP + sign_extend_19({ext_imm13, offset[5:0]})

;-------------------------------------------------------------------------------
; sp_ext_access: alloca two objects — a 64-byte pad and a 4-byte variable.
; Frame layout (SP grows down):
;   [SP+68] virtual frame base
;   [SP+64] var (offset 64 from SP → needs ext 1)
;   [SP+0 ] pad[0..15] (offset 0 from SP → no ext)
;   [SP]    stack pointer
;
; The store to 'var' at SP+64 requires:  ext 1 / ld.w [%sp+64], %rN
;-------------------------------------------------------------------------------

; Frame layout for sp_ext_access (LLVM assigns first alloca to highest SP offset):
;   SP+0  : %pad  (64 bytes, second alloca → lowest address)
;   SP+64 : %var  (4 bytes, first alloca → highest address)
; Total: 68 bytes.
;
; Direct store to %var (no GEP) at SP+64 requires ext 1.

; CHECK-LABEL: sp_ext_access:
; CHECK: sub %sp, 68
; Ext-prefixed store to %var at SP+64:
; CHECK: ext 1
; CHECK: ld.w [%sp+64],
; The SP restore (add %sp, 68) moves to the delay slot of ret.d;
; it executes before the return completes, restoring SP for the pop.
; CHECK: ret.d
; CHECK-NEXT: add %sp, 68
define void @sp_ext_access(i32 %val) {
  %var = alloca i32, align 4          ; 4 bytes, first alloca → SP+64 (needs ext)
  %pad = alloca [16 x i32], align 4   ; 64 bytes, second alloca → SP+0..SP+63
  store i32 %val, ptr %var            ; direct store, no GEP: SP+64 → ext 1
  ; store 0 to pad[0] to prevent %pad from being DCE'd
  store i32 0, ptr %pad
  ret void
}

;-------------------------------------------------------------------------------
; large_array: alloca [32 x i32] (128 bytes) — large sub %sp, verify frame.
;-------------------------------------------------------------------------------

; CHECK-LABEL: large_array:
; CHECK: sub %sp, 128
; CHECK: ret.d
; CHECK-NEXT: add %sp, 128
define void @large_array(i32 %val) {
  %arr = alloca [32 x i32], align 4
  %p = getelementptr [32 x i32], ptr %arr, i32 0, i32 0
  store i32 %val, ptr %p
  ret void
}
