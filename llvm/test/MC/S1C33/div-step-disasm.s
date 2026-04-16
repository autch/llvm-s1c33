; Verify that llvm-objdump prints the canonical operand forms for the
; step-division instructions. The class-4 encoding hardwires rd=0 for
; div0*/div1/div2s and rs=rd=0 for div3s, but those implicit zero fields are
; not assembly operands.
;
; RUN: llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s

; CHECK:       div0s   %r13
div0s %r13
; CHECK:       div0u   %r13
div0u %r13
; CHECK:       div1    %r13
div1 %r13
; CHECK:       div2s   %r13
div2s %r13
; CHECK:       div3s
; CHECK-NOT:   div3s   %r0, %r0
div3s
