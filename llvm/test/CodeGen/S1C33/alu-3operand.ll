; RUN: llc -march=s1c33 -O2 < %s | FileCheck %s
;
; Test: ext+ALU 3-operand form
;
; When ext precedes a register-register ALU instruction, the CPU uses
; 3-operand mode: ext imm / op %rd, %rs → rd = rs <op> zero_ext(imm)
; This saves a register copy compared to the 2-operand ext+ri form.

;--- ADD 3-operand: imm > 63 (immZExt6 range) ---

; CHECK-LABEL: add_100:
; CHECK:      ext 100
; CHECK-NEXT: add %r10, %r12
; CHECK-NOT:  ld.w
define i32 @add_100(i32 %a) {
  %r = add i32 %a, 100
  ret i32 %r
}

; 13-bit max (8191): single ext
; CHECK-LABEL: add_8191:
; CHECK:      ext 8191
; CHECK-NEXT: add %r10, %r12
define i32 @add_8191(i32 %a) {
  %r = add i32 %a, 8191
  ret i32 %r
}

; 14-bit (8192): needs 2 exts
; CHECK-LABEL: add_8192:
; CHECK:      ext 1
; CHECK-NEXT: ext 0
; CHECK-NEXT: add %r10, %r12
define i32 @add_8192(i32 %a) {
  %r = add i32 %a, 8192
  ret i32 %r
}

; Small add (≤63) stays 2-operand
; CHECK-LABEL: add_63:
; CHECK:      add %r10, 63
; CHECK-NOT:  ext
define i32 @add_63(i32 %a) {
  %r = add i32 %a, 63
  ret i32 %r
}

;--- SUB 3-operand: imm > 63 ---

; CHECK-LABEL: sub_100:
; CHECK:      ext 100
; CHECK-NEXT: sub %r10, %r12
define i32 @sub_100(i32 %a) {
  %r = sub i32 %a, 100
  ret i32 %r
}

; Small sub stays 2-operand
; CHECK-LABEL: sub_63:
; CHECK:      sub %r10, 63
; CHECK-NOT:  ext
define i32 @sub_63(i32 %a) {
  %r = sub i32 %a, 63
  ret i32 %r
}

; DAGCombiner canonicalizes (add x, -N) → check it also uses 3-operand sub
; CHECK-LABEL: add_neg200:
; CHECK:      ext 200
; CHECK-NEXT: sub %r10, %r12
define i32 @add_neg200(i32 %a) {
  %r = add i32 %a, -200
  ret i32 %r
}

;--- AND 3-operand: imm outside immSExt6 [-32..31] ---

; CHECK-LABEL: and_0xff:
; CHECK:      ext 255
; CHECK-NEXT: and %r10, %r12
define i32 @and_0xff(i32 %a) {
  %r = and i32 %a, 255
  ret i32 %r
}

; AND with small positive (≤31) stays 2-operand
; CHECK-LABEL: and_31:
; CHECK:      and %r10, 31
; CHECK-NOT:  ext
define i32 @and_31(i32 %a) {
  %r = and i32 %a, 31
  ret i32 %r
}

;--- OR 3-operand ---

; CHECK-LABEL: or_0x100:
; CHECK:      ext 256
; CHECK-NEXT: or %r10, %r12
define i32 @or_0x100(i32 %a) {
  %r = or i32 %a, 256
  ret i32 %r
}

;--- XOR 3-operand ---

; CHECK-LABEL: xor_0xff:
; CHECK:      ext 255
; CHECK-NEXT: xor %r10, %r12
define i32 @xor_0xff(i32 %a) {
  %r = xor i32 %a, 255
  ret i32 %r
}

;--- Two-register case: rd ≠ rs ---
; The 3-operand form naturally handles this without a copy.
; CHECK-LABEL: add_two_results:
; CHECK:      ext 1000
; CHECK-NEXT: add %r{{[0-9]+}}, %r12
define i32 @add_two_results(i32 %a) {
  %sum = add i32 %a, 1000
  %prod = mul i32 %a, 3
  %r = add i32 %sum, %prod
  ret i32 %r
}
