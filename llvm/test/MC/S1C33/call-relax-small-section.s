; Regression: a `call` or `call.d` with an external target placed in a small
; per-function section (`.text.<name>`, as emitted under -ffunction-sections
; or LTO) used to trigger an assertion in MCAssembler::writeSectionData
; because the two-step relaxation chain CALL_* → CALL_EXT1 → CALL_EXT2
; exceeded the relaxOnce inner loop's MaxIter budget, leaving fragment
; offsets stale.  CALL_* now relaxes directly to CALL_EXT2, matching the
; LDW_SYM_EXT0 → LDW_SYM_EXT2 pattern.
;
; RUN: llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s --check-prefix=DIS
; RUN: llvm-readobj -r %t | FileCheck %s --check-prefix=RELOC

        .section        .text.foo,"ax",@progbits
foo:
        call    extfunc
        ret

        .section        .text.bar,"ax",@progbits
bar:
        call.d  extfuncd
        ret

; The call is relaxed to the 6-byte EXT2 form (ext + ext + call/call.d).
; DIS:      ext     0
; DIS-NEXT: ext     0
; DIS-NEXT: call    0
; DIS-NEXT: ret
; DIS:      ext     0
; DIS-NEXT: ext     0
; DIS-NEXT: call.d  0
; DIS-NEXT: ret

; Each call emits the REL_H/M/L triple against its external target.
; RELOC:      Relocations [
; RELOC:        Section {{.*}} .rela.text.foo {
; RELOC-NEXT:     0x0 R_S1C33_REL_H extfunc 0x0
; RELOC-NEXT:     0x2 R_S1C33_REL_M extfunc 0x0
; RELOC-NEXT:     0x4 R_S1C33_REL_L extfunc 0x0
; RELOC-NEXT:   }
; RELOC:        Section {{.*}} .rela.text.bar {
; RELOC-NEXT:     0x0 R_S1C33_REL_H extfuncd 0x0
; RELOC-NEXT:     0x2 R_S1C33_REL_M extfuncd 0x0
; RELOC-NEXT:     0x4 R_S1C33_REL_L extfuncd 0x0
; RELOC-NEXT:   }
