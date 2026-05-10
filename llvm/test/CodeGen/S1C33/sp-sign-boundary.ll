; RUN: llc -mtriple=s1c33-none-elf -O0 -o - %s | FileCheck %s
;
; SP-relative word-scaled offset encoding tests.
;
; With word-scaled SP offsets:
;   ld.w [%sp+N]: N is in word units (×4)
;   ld.h [%sp+N]: N is in halfword units (×2)
;   ld.b [%sp+N]: N is in byte units (×1)
;
; All have 6-bit unsigned immediate fields (0..63 scaled units).
; Values >= 64 require ext prefix.

;-----------------------------------------------------------------------
; half_array: write to buf[15] (byte offset 30 = halfword offset 15)
; and buf[16] (byte offset 32 = halfword offset 16).
; Both fit in 6-bit unsigned → no ext needed.
; Frame: buf[40 × i16] = 80 bytes, sub %sp = 80/4 = 20 words.
;-----------------------------------------------------------------------

; CHECK-LABEL: half_array:
; CHECK: sub %sp, 20
; Both halfword offsets (15, 16) fit in 6 bits — no ext prefix
; CHECK-NOT: ext
; The two stores are independent; the scheduler may emit them in either
; order, so CHECK-DAG accepts both sequences.
; CHECK-DAG: ld.h [%sp+16],
; CHECK-DAG: ld.h [%sp+15],
define void @half_array(i32 %x) {
  %buf = alloca [40 x i16], align 2
  %p15 = getelementptr [40 x i16], ptr %buf, i32 0, i32 15
  %p16 = getelementptr [40 x i16], ptr %buf, i32 0, i32 16
  %v = trunc i32 %x to i16
  store i16 %v, ptr %p15
  store i16 %v, ptr %p16
  ret void
}

;-----------------------------------------------------------------------
; sign_boundary_word: store to a 4-byte variable at byte offset 32
; = word offset 8.  Fits in 6-bit → no ext needed.
;-----------------------------------------------------------------------

; CHECK-LABEL: sign_boundary_word:
; CHECK: sub %sp, 9
; Word offset 8 fits in 6 bits — no ext
; CHECK-NOT: ext
; CHECK: ld.w [%sp+8],
define void @sign_boundary_word(i32 %val) {
  %var = alloca i32, align 4
  %pad = alloca [8 x i32], align 4    ; 32 bytes → SP+0..SP+31
  store i32 %val, ptr %var            ; byte offset 32 → word offset 8
  store i32 0, ptr %pad
  ret void
}

;-----------------------------------------------------------------------
; sign_boundary_62: store 16-bit value at byte offset 62 = halfword
; offset 31.  Fits in 6-bit → no ext needed.
;-----------------------------------------------------------------------

; CHECK-LABEL: sign_boundary_62:
; CHECK: sub %sp, 20
; Halfword offset 31 fits in 6 bits — no ext
; CHECK-NOT: ext
; CHECK: ld.h [%sp+31],
define void @sign_boundary_62(i32 %x) {
  %buf = alloca [40 x i16], align 2
  %p31 = getelementptr [40 x i16], ptr %buf, i32 0, i32 31
  %v = trunc i32 %x to i16
  store i16 %v, ptr %p31              ; byte offset 62 → halfword offset 31
  ret void
}
