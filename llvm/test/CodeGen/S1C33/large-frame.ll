; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Large stack frame: verify that SP-relative accesses and add/sub %sp use
; word-scaled immediates as required by the S1C33 ISA.
;
; SP-relative word load/store: imm6 is in word units (×4).
;   Range without ext: 0–63 words = 0–252 bytes.
; add/sub %sp: imm10 is in word units (×4).
;   Range without ext: 0–1023 words = 0–4092 bytes.
;
; For word offsets > 63, eliminateFrameIndex inserts 'ext' before the
; memory instruction.

;-------------------------------------------------------------------------------
; sp_ext_access: alloca two objects — a 256-byte pad and a 4-byte variable.
; Frame layout (SP grows down):
;   SP+0  : %pad  (256 bytes, second alloca → lowest address)
;   SP+256: %var  (4 bytes, first alloca → highest address)
; Total: 260 bytes = 65 words.
;
; Word offset 64 for %var (256 bytes / 4) requires ext because 64 > 63.
;-------------------------------------------------------------------------------

; CHECK-LABEL: sp_ext_access:
; CHECK: sub %sp, 65
; Ext-prefixed store to %var at word offset 64:
; CHECK: ext 1
; CHECK-NEXT: ld.w [%sp+64],
; CHECK: add %sp, 65
; CHECK: ret.d
; CHECK-NEXT: nop
define void @sp_ext_access(i32 %val) {
  %var = alloca i32, align 4            ; 4 bytes, first alloca → word offset 64
  %pad = alloca [64 x i32], align 4     ; 256 bytes, second alloca → SP+0..SP+255
  store i32 %val, ptr %var              ; word offset 64 → ext 1
  ; store 0 to pad[0] to prevent %pad from being DCE'd
  store i32 0, ptr %pad
  ret void
}

;-------------------------------------------------------------------------------
; large_array: alloca [32 x i32] (128 bytes = 32 words) — verify word-scaled
; sub/add %sp.
;-------------------------------------------------------------------------------

; CHECK-LABEL: large_array:
; CHECK: sub %sp, 32
; CHECK: add %sp, 32
; CHECK: ret.d
; CHECK-NEXT: nop
define void @large_array(i32 %val) {
  %arr = alloca [32 x i32], align 4
  %p = getelementptr [32 x i32], ptr %arr, i32 0, i32 0
  store i32 %val, ptr %p
  ret void
}
