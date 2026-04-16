; Verify that the assembler accepts PC-relative delayed calls with both
; literal displacements and labels.
;
; RUN: llvm-mc -triple=s1c33-none-elf -filetype=obj %s -o %t
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s

; CHECK:       call.d  4
call.d 4

; CHECK:       call.d  -3
call.d -3

; CHECK:       call.d  1 <target>
call.d target

target:
; CHECK:       ret
ret
