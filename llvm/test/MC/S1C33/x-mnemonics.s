; RUN: llvm-mc -triple=s1c33-none-elf -show-encoding %s 2>&1 | FileCheck %s
;
; Verify that x-prefixed extended mnemonics are accepted by the assembler
; and silently lowered to their base instructions.
; Source: S5U1C33000C manual Chapter 10 (sections 10.6.1–10.6.9).
;
; NOTE: xoor = extended or  (not xor!)
;       xxor = extended xor (the base xor instruction also exists)

; 10.6.1 Arithmetic
; CHECK: add %r1, %r2
xadd %r1, %r2
; CHECK: sub %r1, %r2
xsub %r1, %r2

; 10.6.2 Compare
; CHECK: cmp %r1, %r2
xcmp %r1, %r2

; 10.6.3 Logical
; CHECK: and %r1, %r2
xand %r1, %r2
; CHECK: or %r1, %r2
xoor %r1, %r2
; CHECK: xor %r1, %r2
xxor %r1, %r2
; CHECK: not %r1, %r2
xnot %r1, %r2

; 10.6.4 Shift & rotate
; CHECK: srl %r1, 1
xsrl %r1, 1
; CHECK: sll %r1, 1
xsll %r1, 1
; CHECK: sra %r1, 1
xsra %r1, 1
; CHECK: sla %r1, 1
xsla %r1, 1
; CHECK: rr %r1, 1
xrr %r1, 1
; CHECK: rl %r1, 1
xrl %r1, 1

; 10.6.5–10.6.7 Memory
; CHECK: ld.w %r1, [%r2]
xld.w %r1, [%r2]
; CHECK: ld.b %r1, [%r2]
xld.b %r1, [%r2]
; CHECK: ld.ub %r1, [%r2]
xld.ub %r1, [%r2]
; CHECK: ld.h %r1, [%r2]
xld.h %r1, [%r2]
; CHECK: ld.uh %r1, [%r2]
xld.uh %r1, [%r2]

; 10.6.8 Bit operations ([%rb] memory-indirect — brackets mandatory per CPU manual §2.5.5)
; CHECK: btst [%r1], 0
xbtst [%r1], 0
; CHECK: bclr [%r1], 0
xbclr [%r1], 0
; CHECK: bset [%r1], 0
xbset [%r1], 0
; CHECK: bnot [%r1], 0
xbnot [%r1], 0

; 10.6.9 Branch instructions
; CHECK: call
xcall some_func
; CHECK: jp
xjp some_label
; CHECK: jreq
xjreq some_label
; CHECK: jrne
xjrne some_label
; CHECK: jrgt
xjrgt some_label
; CHECK: jrge
xjrge some_label
; CHECK: jrlt
xjrlt some_label
; CHECK: jrle
xjrle some_label
; CHECK: jrugt
xjrugt some_label
; CHECK: jruge
xjruge some_label
; CHECK: jrult
xjrult some_label
; CHECK: jrule
xjrule some_label
