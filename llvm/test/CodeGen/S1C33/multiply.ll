; RUN: llc -mtriple=s1c33-none-elf -mcpu=s1c33209 -o - %s | FileCheck %s --check-prefix=HWMUL
; RUN: llc -mtriple=s1c33-none-elf -mcpu=generic   -o - %s | FileCheck %s --check-prefix=NOMUL
;
; 4-4 Hardware multiplier tests.
; With +hwmul (s1c33209): ISD::MUL -> mlt.w + ld.w %rd, %alr
; Without +hwmul (generic): ISD::MUL -> call __mulsi3

;-------------------------------------------------------------------------------
; mul_i32: 32-bit integer multiply
;   With hwmul: mlt.w + ld.w %r10, %alr (in delay slot)
;   Without:    call __mulsi3
;-------------------------------------------------------------------------------

; HWMUL-LABEL: mul_i32:
; HWMUL: mlt.w
; HWMUL: ret.d
; HWMUL: ld.w %r10, %alr

; NOMUL-LABEL: mul_i32:
; NOMUL: call __mulsi3
; NOMUL: ret.d
define i32 @mul_i32(i32 %a, i32 %b) {
  %r = mul i32 %a, %b
  ret i32 %r
}

;-------------------------------------------------------------------------------
; mulhs_i32: signed multiply high word (e.g., 64-bit multiply upper half)
;   With hwmul: mlt.w + ld.w %r10, %ahr (in delay slot)
;-------------------------------------------------------------------------------

; HWMUL-LABEL: mulhs_i32:
; HWMUL: mlt.w
; HWMUL: ret.d
; HWMUL: ld.w %r10, %ahr
define i32 @mulhs_i32(i32 %a, i32 %b) {
  %a64 = sext i32 %a to i64
  %b64 = sext i32 %b to i64
  %r64 = mul i64 %a64, %b64
  %hi = lshr i64 %r64, 32
  %r = trunc i64 %hi to i32
  ret i32 %r
}

;-------------------------------------------------------------------------------
; mulhu_i32: unsigned multiply high word
;   With hwmul: mltu.w + ld.w %r10, %ahr (in delay slot)
;-------------------------------------------------------------------------------

; HWMUL-LABEL: mulhu_i32:
; HWMUL: mltu.w
; HWMUL: ret.d
; HWMUL: ld.w %r10, %ahr
define i32 @mulhu_i32(i32 %a, i32 %b) {
  %a64 = zext i32 %a to i64
  %b64 = zext i32 %b to i64
  %r64 = mul i64 %a64, %b64
  %hi = lshr i64 %r64, 32
  %r = trunc i64 %hi to i32
  ret i32 %r
}

;-------------------------------------------------------------------------------
; mul16_unsigned: 16-bit unsigned multiply → mltu.h (1 clock vs 5 for mlt.w)
;   Both operands are zero-extended from i16, so result fits in 32 bits.
;-------------------------------------------------------------------------------

; HWMUL-LABEL: mul16_unsigned:
; HWMUL: mltu.h
; HWMUL: ret.d
; HWMUL: ld.w %r10, %alr
define i32 @mul16_unsigned(i16 zeroext %a, i16 zeroext %b) {
  %a32 = zext i16 %a to i32
  %b32 = zext i16 %b to i32
  %r = mul i32 %a32, %b32
  ret i32 %r
}

;-------------------------------------------------------------------------------
; mul16_signed: 16-bit signed multiply → mlt.h (1 clock vs 5 for mlt.w)
;   Both operands are sign-extended from i16, so result fits in 32 bits.
;-------------------------------------------------------------------------------

; HWMUL-LABEL: mul16_signed:
; HWMUL: mlt.h
; HWMUL: ret.d
; HWMUL: ld.w %r10, %alr
define i32 @mul16_signed(i16 signext %a, i16 signext %b) {
  %a32 = sext i16 %a to i32
  %b32 = sext i16 %b to i32
  %r = mul i32 %a32, %b32
  ret i32 %r
}

;-------------------------------------------------------------------------------
; mul_32bit_stays_mltw: full 32-bit operands must use mlt.w, not mlt.h
;-------------------------------------------------------------------------------

; HWMUL-LABEL: mul_full32:
; HWMUL: mlt.w
; HWMUL-NOT: mlt.h
; HWMUL-NOT: mltu.h
define i32 @mul_full32(i32 %a, i32 %b) {
  %r = mul i32 %a, %b
  ret i32 %r
}
