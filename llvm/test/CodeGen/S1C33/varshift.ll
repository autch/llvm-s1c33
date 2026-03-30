; RUN: llc -march=s1c33 < %s | FileCheck %s

; Variable shifts must expand to a loop because S1C33 shift instructions
; only support amounts 0-8 (both immediate and register-register forms).

define i32 @srl_var(i32 %val, i32 %amt) {
; CHECK-LABEL: srl_var:
; CHECK:       cmp %r{{[0-9]+}}, 8
; CHECK:       jrule
; CHECK:       srl %r{{[0-9]+}}, 8
; CHECK:       sub %r{{[0-9]+}}, 8
; CHECK:       cmp %r{{[0-9]+}}, 8
; CHECK:       jrugt
; CHECK:       srl %r{{[0-9]+}}, %r{{[0-9]+}}
  %r = lshr i32 %val, %amt
  ret i32 %r
}

define i32 @sll_var(i32 %val, i32 %amt) {
; CHECK-LABEL: sll_var:
; CHECK:       cmp %r{{[0-9]+}}, 8
; CHECK:       jrule
; CHECK:       sll %r{{[0-9]+}}, 8
; CHECK:       sub %r{{[0-9]+}}, 8
; CHECK:       jrugt
; CHECK:       sll %r{{[0-9]+}}, %r{{[0-9]+}}
  %r = shl i32 %val, %amt
  ret i32 %r
}

define i32 @sra_var(i32 %val, i32 %amt) {
; CHECK-LABEL: sra_var:
; CHECK:       cmp %r{{[0-9]+}}, 8
; CHECK:       jrule
; CHECK:       sra %r{{[0-9]+}}, 8
; CHECK:       sub %r{{[0-9]+}}, 8
; CHECK:       jrugt
; CHECK:       sra %r{{[0-9]+}}, %r{{[0-9]+}}
  %r = ashr i32 %val, %amt
  ret i32 %r
}

; Constant shifts <= 8 should NOT use the loop.
define i32 @srl_const_small(i32 %val) {
; CHECK-LABEL: srl_const_small:
; CHECK:       srl %r{{[0-9]+}}, 5
; CHECK-NOT:   jrugt
  %r = lshr i32 %val, 5
  ret i32 %r
}

; Constant shifts > 8 should be split into multiple immediate shifts.
define i32 @srl_const_large(i32 %val) {
; CHECK-LABEL: srl_const_large:
; CHECK:       srl %r{{[0-9]+}}, 8
; CHECK:       srl %r{{[0-9]+}}, 8
; CHECK-NOT:   jrugt
  %r = lshr i32 %val, 16
  ret i32 %r
}
