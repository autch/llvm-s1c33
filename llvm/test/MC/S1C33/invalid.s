; RUN: not llvm-mc -triple=s1c33-none-elf %s 2>&1 | FileCheck %s
;
; Parse/match-time error diagnostics: unknown mnemonics, operand kind
; mismatches, out-of-range immediates, and memory-operand syntax errors.
; Each case pins the exact message and location.

frobnicate %r0 ; CHECK: :[[@LINE]]:1: error: unrecognized instruction mnemonic
add 1, %r0 ; CHECK: :[[@LINE]]:5: error: invalid operand for instruction
add %r0 ; CHECK: :[[@LINE]]:1: error: too few operands for instruction
add %r0, %r1 %r2 ; CHECK: :[[@LINE]]:14: error: unexpected token in instruction operands

; uimm6: add/sub %rd, imm6 is zero-extended; negative values are invalid.
add %r0, 64 ; CHECK: :[[@LINE]]:10: error: immediate must be an integer in the range [0, 63]
add %r0, -1 ; CHECK: :[[@LINE]]:10: error: immediate must be an integer in the range [0, 63]

; sign6: and/or/xor/not/cmp/ld.w %rd, imm is sign-extended.
and %r0, 32 ; CHECK: :[[@LINE]]:10: error: immediate must be an integer in the range [-32, 31]
cmp %r0, -33 ; CHECK: :[[@LINE]]:10: error: immediate must be an integer in the range [-32, 31]
ld.w %r0, 100 ; CHECK: :[[@LINE]]:11: error: immediate must be an integer in the range [-32, 31]

; sign8: constant PC-relative branch displacements are range-checked at
; parse time (labels are deferred to fixup/relaxation instead).
jreq 300 ; CHECK: :[[@LINE]]:6: error: branch displacement must be an integer in the range [-128, 127] or a symbol
call 200 ; CHECK: :[[@LINE]]:6: error: branch displacement must be an integer in the range [-128, 127] or a symbol
jp -129 ; CHECK: :[[@LINE]]:4: error: branch displacement must be an integer in the range [-128, 127] or a symbol

; imm10: add/sub %sp, imm10 (byte count, zero-extended).
add %sp, 1024 ; CHECK: :[[@LINE]]:10: error: immediate must be an integer in the range [0, 1023]
sub %sp, -1 ; CHECK: :[[@LINE]]:10: error: immediate must be an integer in the range [0, 1023]

; imm13: ext (would otherwise truncate silently to 13 bits).
ext 8192 ; CHECK: :[[@LINE]]:5: error: immediate must be an integer in the range [0, 8191]
ext -1 ; CHECK: :[[@LINE]]:5: error: immediate must be an integer in the range [0, 8191]

; imm3: bit operations btst/bclr/bset/bnot [%rb], imm3.
btst [%r0], 8 ; CHECK: :[[@LINE]]:13: error: immediate must be an integer in the range [0, 7]

; Shift amount is 1..8 (0 is not encodable; >8 must be split by the
; compiler, see varshift.ll).
sll %r0, 9 ; CHECK: :[[@LINE]]:10: error: shift amount must be an integer in the range [1, 8]
sll %r0, 0 ; CHECK: :[[@LINE]]:10: error: shift amount must be an integer in the range [1, 8]

; SP-relative offset is uimm6 in scaled units.
ld.w %r0, [%sp+64] ; CHECK: :[[@LINE]]:16: error: immediate must be an integer in the range [0, 63]

; Register and memory-operand syntax errors.
add %rx, %r0 ; CHECK: :[[@LINE]]:5: error: unknown register name '%rx'
ld.w %r0, [%rx] ; CHECK: :[[@LINE]]:12: error: unknown register name '%rx'
ld.w %r0, [4] ; CHECK: :[[@LINE]]:12: error: expected '%' after '['
add %r0, %1 ; CHECK: :[[@LINE]]:11: error: expected register name after '%'
; [%rb+imm] is as33 extended syntax (handled by asm33conv), not accepted
; here; the displacement must come from a preceding ext.
ld.w %r0, [%r1+4] ; CHECK: :[[@LINE]]:15: error: expected ']'
