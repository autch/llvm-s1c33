; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Interrupt handler attribute: __attribute__((interrupt_handler)).
; Prologue must save all registers with pushn %r15.
; Epilogue must restore with popn %r15 and return with reti (not ret).
; reti restores PSR and PC from the stack (pushed by hardware on interrupt).

; CHECK-LABEL: my_isr:
; CHECK:       pushn %r15
; CHECK-NOT:   ret{{^i}}
; CHECK:       popn %r15
; CHECK-NEXT:  reti
define void @my_isr() #0 {
  ret void
}

; Normal function must NOT use reti.
; CHECK-LABEL: normal_fn:
; CHECK-NOT:   reti
; CHECK:       ret
define void @normal_fn() {
  ret void
}

attributes #0 = { "interrupt_handler" }
