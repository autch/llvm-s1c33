; Verify that llvm-objdump annotates ext-extended immediates with a hex comment
; on the TARGET instruction line (not on the ext instruction line).
; For non-PC-relative operands the instruction shows the RAW decoded value
; (sign-extended from the native field width) and the comment shows the
; ext-resolved value.  For PC-relative branches the operand shows the
; ext-RESOLVED displacement (so MCInstrAnalysis can symbolize the real
; target), followed by the resolved target label and the hex comment.
;
; RUN: llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s

; ---- single ext: non-PC-relative ----------------------------------------
; ext 0 + ld.w %r10, raw 0b101010 → decodeSimm6 stores SignExtend6(42) = -22
; Extended = (0<<6)|42 = 42 = 0x2a.  Displayed: -22 (raw sign6); comment: # 0x2a.
; CHECK:       ext     0
; CHECK-NEXT:  ld.w    %r10, -22  {{.*}}# 0x2a
.2byte 0xc000    ; ext 0
.2byte 0x6eaa    ; ld.w %r10, raw 0b101010

; ext 192 + ld.w %r10, raw 0b111001=57 → decodeSimm6 stores SignExtend6(57) = -7
; Extended = (192<<6)|57 = 12345 = 0x3039.  Displayed: -7 (raw sign6); comment: # 0x3039.
; CHECK:       ext     192
; CHECK-NEXT:  ld.w    %r10, -7  {{.*}}# 0x3039
.2byte 0xc0c0    ; ext 192
.2byte 0x6f9a    ; ld.w %r10, raw 0b111001

; ---- double ext: non-PC-relative -----------------------------------------
; ext 1 + ext 7433 + ld.w %r10, raw 0 → extended = (1<<19)|(7433<<6)|0 = 0xf4240
; Displayed operand = 0 (raw); comment = # 0xf4240.
; CHECK:       ext     1
; CHECK-NEXT:  ext     7433
; CHECK-NEXT:  ld.w    %r10, 0  {{.*}}# 0xf4240
.2byte 0xc001    ; ext 1
.2byte 0xdd09    ; ext 7433
.2byte 0x6c0a    ; ld.w %r10, raw=0

; ---- single ext: PC-relative branch -------------------------------------
; ext 0 + jreq raw disp=2 → extended = (0<<8)|2 = 2 = 0x2
; Displayed operand = 2 (resolved); target = 0x10 + 2*2 = 0x14; comment = # 0x2.
; CHECK:       ext     0
; CHECK-NEXT:  jreq    2 <.text+0x14>  {{.*}}# 0x2
.2byte 0xc000    ; ext 0
.2byte 0x1802    ; jreq raw disp=2

; ext 1 + jreq raw disp=0x80 (sign8=-128) → extended = (1<<8)|128 = 384 = 0x180
; Displayed operand = 384 (ext-resolved, NOT the raw sign8 -128); target =
; 0x14 + 2*384 = 0x314; comment = # 0x180.
; CHECK:       ext     1
; CHECK-NEXT:  jreq    384 <.text+0x314>  {{.*}}# 0x180
.2byte 0xc001    ; ext 1
.2byte 0x1880    ; jreq raw disp=0x80 (sign8=-128)
