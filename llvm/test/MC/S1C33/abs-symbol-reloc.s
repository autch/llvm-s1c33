; RUN: llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-readobj -r %t | FileCheck %s
;
; Regression test: a relocation specifier on a local ABSOLUTE symbol used
; to crash the generic ELFObjectWriter (it tried to convert the symbol to
; its section's begin symbol, but an absolute symbol has no section).
; The symbol must be kept in the relocation as-is (SHN_ABS).

bigsym = 0x4000000

	ext bigsym@ah
	ext bigsym@al
	ld.w %r0, [%r8]

; CHECK:      Relocations [
; CHECK-NEXT:   Section ({{[0-9]+}}) .rela.text {
; CHECK-NEXT:     0x0 R_S1C33_REL_AH bigsym 0x0
; CHECK-NEXT:     0x2 R_S1C33_REL_AL bigsym 0x0
; CHECK-NEXT:   }
; CHECK-NEXT: ]
