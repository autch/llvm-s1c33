; RUN: llc -mtriple=s1c33-none-elf -filetype=obj -o %t %s
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s
;
; Branch relaxation tests for S1C33.
;
; Near branches (within sign8 range ±254 bytes) are kept as 2-byte instructions.
; Far branches (beyond sign8 range) are relaxed to 4-byte ext+branch sequences.

; External function used to pad the then-block with call instructions.
declare void @pad()

;-------------------------------------------------------------------------------
; Near branch: simple if-then-else in a small function.
; The jrlt target is only a few bytes away — fits in sign8 without ext.
; Expect: jrlt as a standalone 2-byte instruction (no preceding ext).
;-------------------------------------------------------------------------------
; CHECK-LABEL: <near_branch>:
; CHECK-NOT:   ext
; CHECK:       jrlt
; CHECK-NOT:   {{ext.*}}
define i32 @near_branch(i32 %x) {
entry:
  %cmp = icmp sgt i32 %x, 0
  br i1 %cmp, label %then, label %else
then:
  ret i32 1
else:
  ret i32 0
}

;-------------------------------------------------------------------------------
; Far branch: the then-block contains 128 calls (256 bytes) plus a ret (2 bytes)
; = 258 bytes.  The conditional branch target (else) is at offset +260 bytes
; from the branch site (2 bytes for jp + 258 bytes for then-block), giving
; sign8 = (260 - 2)/2 = 129 > 127, which overflows the 8-bit field.
;
; Expect: ext <imm> on the line immediately before jrlt.
;-------------------------------------------------------------------------------
; CHECK-LABEL: <far_branch>:
; CHECK:       ext
; CHECK-NEXT:  jrlt
define i32 @far_branch(i32 %x) {
entry:
  %cmp = icmp sgt i32 %x, 0
  br i1 %cmp, label %then, label %else
then:
  ; 128 calls × 2 bytes = 256 bytes, plus ret = 258 bytes total in then-block.
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  call void @pad()
  ret i32 1
else:
  ret i32 0
}
