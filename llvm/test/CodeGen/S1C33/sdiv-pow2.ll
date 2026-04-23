; RUN: llc -march=s1c33 < %s | FileCheck %s
; Test signed division/modulo by power of 2 uses conditional bias
; (cmp+branch) instead of expensive shift sequences on S1C33
; where max shift amount per instruction is 8.

target triple = "s1c33-none-elf"

; sdiv by 4096 — bias should use cmp+branch, not 7+ sra/srl shifts.
; The bias (4095) is loaded via ext+ld.w and the divide becomes a shift; we
; don't constrain ordering because MOV_ri6 is rematerializable (the allocator
; may hoist the zero operand of the compare above or below the compare).
define i32 @sdiv4096(i32 %x) {
; CHECK-LABEL: sdiv4096:
; CHECK-DAG:   cmp
; CHECK-DAG:   jrlt
; CHECK-DAG:   ext 63
; CHECK-DAG:   ld.w {{.*}}, -1
; CHECK-DAG:   add
; CHECK-DAG:   sra
; CHECK-NOT:   srl
  %r = sdiv i32 %x, 4096
  ret i32 %r
}

; srem by 4096 — also uses conditional bias
define i32 @srem4096(i32 %x) {
; CHECK-LABEL: srem4096:
; CHECK-DAG:   cmp
; CHECK-DAG:   jrlt
; CHECK-DAG:   ext 63
; CHECK-DAG:   ld.w {{.*}}, -1
; CHECK-NOT:   srl
  %r = srem i32 %x, 4096
  ret i32 %r
}

; sdiv by 16 — bias 15 fits in imm6, no ext needed
define i32 @sdiv16(i32 %x) {
; CHECK-LABEL: sdiv16:
; CHECK-DAG:   cmp
; CHECK-DAG:   jrlt
; CHECK-DAG:   ld.w {{.*}}, 15
; CHECK-DAG:   add
; CHECK-DAG:   sra {{.*}}, 4
; CHECK-NOT:   srl
  %r = sdiv i32 %x, 16
  ret i32 %r
}

