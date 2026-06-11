; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; 64-bit shifts. SHL/SRL/SRA_PARTS are custom-lowered, so variable i64
; shifts expand inline (selects on count >= 32 and count == 0) instead of
; calling __ashldi3/__lshrdi3/__ashrdi3. Each 32-bit part shift then goes
; through the variable-shift expansion (the hardware shifter takes at most
; 8 bits per step, so counts > 8 loop in 8-bit chunks).
;
; The variable-shift bodies are large; only the structure is pinned here
; (no libcall, correct shift direction). Constant shifts are small and
; checked exactly.

; CHECK-LABEL: shl64:
; CHECK-NOT:   call
; 8-bit chunk loop for the count > 8 case
; CHECK:       sll {{%r[0-9]+}}, 8
; CHECK-NOT:   call
; CHECK:       ret
define i64 @shl64(i64 %a, i64 %n) {
  %r = shl i64 %a, %n
  ret i64 %r
}

; CHECK-LABEL: lshr64:
; CHECK-NOT:   call
; CHECK:       srl {{%r[0-9]+}}, 8
; CHECK-NOT:   call
; CHECK:       ret
define i64 @lshr64(i64 %a, i64 %n) {
  %r = lshr i64 %a, %n
  ret i64 %r
}

; CHECK-LABEL: ashr64:
; CHECK-NOT:   call
; hi part must shift arithmetically
; CHECK:       sra {{%r[0-9]+}}, 8
; CHECK-NOT:   call
; CHECK:       ret
define i64 @ashr64(i64 %a, i64 %n) {
  %r = ashr i64 %a, %n
  ret i64 %r
}

; CHECK-LABEL: shl64_const1:
; lo<<1, hi = (hi<<1) | (lo>>31); the >>31 is split as 8+8+8+7
; CHECK-DAG:   sll %r11, 1
; CHECK-DAG:   sll %r10, 1
; CHECK-DAG:   srl %r4, 7
; CHECK:       or %r11, %r4
define i64 @shl64_const1(i64 %a) {
  %r = shl i64 %a, 1
  ret i64 %r
}

; CHECK-LABEL: shl64_const32:
; pure register move: hi = lo, lo = 0
; CHECK:       ld.w %r11, %r12
; CHECK-NEXT:  ret.d
; CHECK-NEXT:  ld.w %r10, 0
define i64 @shl64_const32(i64 %a) {
  %r = shl i64 %a, 32
  ret i64 %r
}

; CHECK-LABEL: shl64_const33:
; hi = lo << 1, lo = 0
; CHECK-DAG:   ld.w %r11, %r12
; CHECK-DAG:   ld.w %r10, 0
; CHECK:       sll %r11, 1
define i64 @shl64_const33(i64 %a) {
  %r = shl i64 %a, 33
  ret i64 %r
}

; CHECK-LABEL: lshr64_const33:
; lo = hi >> 1 (logical), hi = 0
; CHECK-DAG:   ld.w %r10, %r13
; CHECK-DAG:   ld.w %r11, 0
; CHECK:       srl %r10, 1
define i64 @lshr64_const33(i64 %a) {
  %r = lshr i64 %a, 33
  ret i64 %r
}

; CHECK-LABEL: ashr64_const33:
; lo = hi >> 1 (arithmetic), hi = sign of hi (>>31 as 8+8+8+7)
; CHECK-DAG:   sra %r10, 1
; CHECK-DAG:   sra %r11, 8
; CHECK-DAG:   sra %r11, 7
define i64 @ashr64_const33(i64 %a) {
  %r = ashr i64 %a, 33
  ret i64 %r
}
