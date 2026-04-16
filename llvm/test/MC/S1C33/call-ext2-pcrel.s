; Verify that call/call.d can use a two-ext PC-relative sequence when the
; target is beyond REL21 range, and that unresolved symbols emit split REL_H/M/L
; relocations for the 6-byte form.
;
; RUN: llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s --check-prefix=DIS
; RUN: llvm-readobj -r %t | FileCheck %s --check-prefix=RELOC

        call far_call
        call.d far_calld
        call extfunc
        call.d extfuncd

        .equ far_call,  0x200004
        .equ far_calld, 0x20000a

; DIS:      ext     0
; DIS-NEXT: ext     0
; DIS-NEXT: call    0  {{.*}}# 0x0
; DIS-NEXT: ext     0
; DIS-NEXT: ext     0
; DIS-NEXT: call.d  0  {{.*}}# 0x0
; DIS-NEXT: ext     0
; DIS-NEXT: ext     0
; DIS-NEXT: call    0  {{.*}}# 0x0
; DIS-NEXT: ext     0
; DIS-NEXT: ext     0
; DIS-NEXT: call.d  0  {{.*}}# 0x0

; RELOC:      Relocations [
; RELOC-NEXT:   Section {{.*}} .rela.text {
; RELOC-NEXT:     0x0 R_S1C33_REL_H - 0x200004
; RELOC-NEXT:     0x2 R_S1C33_REL_M - 0x200004
; RELOC-NEXT:     0x4 R_S1C33_REL_L - 0x200004
; RELOC-NEXT:     0x6 R_S1C33_REL_H - 0x20000A
; RELOC-NEXT:     0x8 R_S1C33_REL_M - 0x20000A
; RELOC-NEXT:     0xA R_S1C33_REL_L - 0x20000A
; RELOC-NEXT:     0xC R_S1C33_REL_H extfunc 0x0
; RELOC-NEXT:     0xE R_S1C33_REL_M extfunc 0x0
; RELOC-NEXT:     0x10 R_S1C33_REL_L extfunc 0x0
; RELOC-NEXT:     0x12 R_S1C33_REL_H extfuncd 0x0
; RELOC-NEXT:     0x14 R_S1C33_REL_M extfuncd 0x0
; RELOC-NEXT:     0x16 R_S1C33_REL_L extfuncd 0x0
; RELOC-NEXT:   }
; RELOC-NEXT: ]
