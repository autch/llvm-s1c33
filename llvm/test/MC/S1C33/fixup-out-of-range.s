; RUN: not llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s
;
; Fixup range errors reported by the AsmBackend at object emission time.
;
; Plain conditional branches relax (EXT0 -> EXT1, +/-2^20 halfwords), so a
; sign8 overflow on them never reaches applyFixup. Delayed conditional
; branches (jrXX.d) are not relaxed, so their raw sign8 fixup must be in
; range. Conditional branches have no EXT2 form, so beyond +/-2MB the EXT1
; fixup itself overflows.
;
; ("absolute address does not fit in 26 bits" in applyFixup is not
; reachable from assembly: an @ah/@al fixup against a symbol always becomes
; a relocation, so the linker performs that check instead.)

; CHECK: :[[@LINE+1]]:9: error: branch target out of range
	jreq.d near
	nop
	.space 300
near:
	nop

; CHECK: :[[@LINE+1]]:7: error: branch target out of ext1 range
	jreq far
	.space 3000000
far:
	nop
