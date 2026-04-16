; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Large stack frame: verify that SP-relative accesses and add/sub %sp use the
; correct offset encoding.
;
; SP-relative word load/store:
;   - without ext: imm6 is in word units (×4)
;   - with ext:    ext+imm6 forms a byte displacement directly
; add/sub %sp: imm10 is in word units (×4).
;   Range without ext: 0–1023 words = 0–4092 bytes.
;
; For word offsets > 252 bytes, eliminateFrameIndex inserts 'ext' before the
; memory instruction and places the low 6 bits of the byte offset in imm6.

;-------------------------------------------------------------------------------
; sp_ext_access: alloca two objects — a 256-byte pad and a 4-byte variable.
; Frame layout (SP grows down):
;   SP+0  : %pad  (256 bytes, second alloca → lowest address)
;   SP+256: %var  (4 bytes, first alloca → highest address)
; Total: 260 bytes = 65 words.
;
; Byte offset 256 for %var requires ext 4 with low imm6 = 0.
;-------------------------------------------------------------------------------

; CHECK-LABEL: sp_ext_access:
; CHECK: sub %sp, 65
; Ext-prefixed store to %var at byte offset 256:
; CHECK: ext 4
; CHECK-NEXT: ld.w [%sp+0],
; CHECK: add %sp, 65
; CHECK: ret
define void @sp_ext_access(i32 %val) {
  %var = alloca i32, align 4            ; 4 bytes, first alloca → word offset 64
  %pad = alloca [64 x i32], align 4     ; 256 bytes, second alloca → SP+0..SP+255
  store i32 %val, ptr %var              ; byte offset 256 → ext 4 / imm6 0
  ; store 0 to pad[0] to prevent %pad from being DCE'd
  store i32 0, ptr %pad
  ret void
}

;-------------------------------------------------------------------------------
; sp_ext_access_nonzero_imm6: byte offset 380 = ext 5 + imm6 60.
; This is word-aligned but exercises the byte-displacement EXT encoding.
;-------------------------------------------------------------------------------

; CHECK-LABEL: sp_ext_access_nonzero_imm6:
; CHECK: sub %sp, 96
; CHECK: ext 5
; CHECK-NEXT: ld.w [%sp+60],
define void @sp_ext_access_nonzero_imm6(i32 %val) {
  %var = alloca i32, align 4
  %pad = alloca [95 x i32], align 4
  store i32 %val, ptr %var
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
; CHECK: ret
define void @large_array(i32 %val) {
  %arr = alloca [32 x i32], align 4
  %p = getelementptr [32 x i32], ptr %arr, i32 0, i32 0
  store i32 %val, ptr %p
  ret void
}
