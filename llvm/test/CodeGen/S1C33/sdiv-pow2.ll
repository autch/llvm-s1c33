; RUN: llc -march=s1c33 < %s | FileCheck %s
; Test signed division/modulo by power of 2 uses conditional bias
; (cmp+branch) instead of expensive shift sequences on S1C33
; where max shift amount per instruction is 8.

target triple = "s1c33-none-elf"

; sdiv by 4096 — bias should use cmp+branch, not 7+ sra/srl shifts
define i32 @sdiv4096(i32 %x) {
; CHECK-LABEL: sdiv4096:
; CHECK:       cmp
; CHECK:       jrlt
; The bias (4095) is loaded via ext+ld.w, not computed with sra+srl
; CHECK:       ext 63
; CHECK:       ld.w {{.*}}, -1
; CHECK:       add
; CHECK:       sra
  %r = sdiv i32 %x, 4096
  ret i32 %r
}

; srem by 4096 — also uses conditional bias
define i32 @srem4096(i32 %x) {
; CHECK-LABEL: srem4096:
; CHECK:       cmp
; CHECK:       jrlt
; CHECK:       ext 63
; CHECK:       ld.w {{.*}}, -1
  %r = srem i32 %x, 4096
  ret i32 %r
}

; sdiv by 16 — bias 15 fits in imm6, no ext needed
define i32 @sdiv16(i32 %x) {
; CHECK-LABEL: sdiv16:
; CHECK:       cmp
; CHECK:       jrlt
; CHECK:       ld.w {{.*}}, 15
; CHECK:       add
; CHECK:       sra {{.*}}, 4
  %r = sdiv i32 %x, 16
  ret i32 %r
}

