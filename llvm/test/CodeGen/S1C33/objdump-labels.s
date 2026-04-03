; RUN: llvm-mc --triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -d --symbolize-operands %t | FileCheck %s
;
; Test that llvm-objdump --symbolize-operands inserts <LN>: labels at
; branch/call targets and replaces raw immediates with label references.
;
; Verifies MCInstrAnalysis::evaluateBranch() for S1C33 branch/call opcodes.
; Register-indirect branches must NOT produce labels (target unresolvable).

	.text

; --- Conditional forward branch ---
; CHECK-LABEL: <test_cond>:
; CHECK:         jreq{{.*}}<L0>
; CHECK:       <L0>:
; CHECK:         ret
	.global test_cond
test_cond:
	cmp	%r0, %r1
	jreq	2
	add	%r0, 1
	ret
	ret

; --- Unconditional jump ---
; CHECK-LABEL: <test_uncond>:
; CHECK:         jp{{.*}}<L0>
; CHECK:       <L0>:
	.global test_uncond
test_uncond:
	jp	1
	add	%r0, 0
	ret

; --- Call instruction ---
; CHECK-LABEL: <test_call>:
; CHECK:         call{{.*}}<L0>
; CHECK:       <L0>:
	.global test_call
test_call:
	call	1
	add	%r0, 0
	ret

; --- Delay-slot branch variant ---
; CHECK-LABEL: <test_delay>:
; CHECK:         jrne.d{{.*}}<L0>
; CHECK:       <L0>:
	.global test_delay
test_delay:
	cmp	%r0, 0
	jrne.d	2
	add	%r0, 1
	add	%r0, 2
	ret

; --- Backward branch (loop) ---
; The branch target is inside test_loop with no symbol there, so a <LN>
; label is generated.  Just verify the label appears before the instruction.
; CHECK-LABEL: <test_loop>:
; CHECK:       <L{{[0-9]+}}>:
; CHECK:         jrult
	.global test_loop
test_loop:
	add	%r4, 1
	cmp	%r4, %r12
	jrult	-2
	ret

; --- Register-indirect: NO label should appear ---
; CHECK-LABEL: <test_indirect>:
; CHECK-NOT:   <L
; CHECK:         jp	%r4
; CHECK:         ret
	.global test_indirect
test_indirect:
	jp	%r4
	ret
