; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Basic instruction tests for S1C33 backend — Phase 1/2 minimum requirement:
;   nop (via inline asm), add, sub, cmp, ld.w, ld.b, ext, jr/call/ret.
;
; S5U1C33000C ABI: args R12→R15, return R10, callee-saved R0–R3.

;-------------------------------------------------------------------------------
; ret void
;-------------------------------------------------------------------------------
; CHECK-LABEL: void_return:
; CHECK: ret
define void @void_return() {
  ret void
}

;-------------------------------------------------------------------------------
; ret i32 — immediate materialization
;-------------------------------------------------------------------------------

; CHECK-LABEL: ret_zero:
; CHECK: ret.d
; CHECK-NEXT: ld.w %r10, 0
define i32 @ret_zero() {
  ret i32 0
}

; CHECK-LABEL: ret_neg1:
; CHECK: ret.d
; CHECK-NEXT: ld.w %r10, -1
define i32 @ret_neg1() {
  ret i32 -1
}

; Fits in 6-bit signed (range -32..31)
; CHECK-LABEL: ret_31:
; CHECK: ret.d
; CHECK-NEXT: ld.w %r10, 31
define i32 @ret_31() {
  ret i32 31
}

; Does NOT fit in 6 bits → needs ext 0 to extend the 6-bit field to 19 bits.
; 42 = 0b101010: sign6 field = -22.  ext 0 widens sign-extension to 19 bits: effective = +42.
; The ext-extended ld.w cannot fill the delay slot (ext must precede it),
; so the backend keeps a plain ret.
; CHECK-LABEL: ret_42:
; CHECK: ext 0
; CHECK-NEXT: ld.w %r10, -22
; CHECK-NEXT: ret
define i32 @ret_42() {
  ret i32 42
}

; 12345 = 0x3039 — fits in 19 bits: ext_imm13=192, sign6 field = -7 (0b111001).
; CHECK-LABEL: ret_12345:
; CHECK: ext 192
; CHECK-NEXT: ld.w %r10, -7
; CHECK-NEXT: ret
define i32 @ret_12345() {
  ret i32 12345
}

; 1000000 — needs two ext instructions. sign6 field = 0 (upper bits in ext words).
; CHECK-LABEL: ret_1000000:
; CHECK: ext 1
; CHECK-NEXT: ext 7433
; CHECK-NEXT: ld.w %r10, 0
; CHECK-NEXT: ret
define i32 @ret_1000000() {
  ret i32 1000000
}

;-------------------------------------------------------------------------------
; add — register-register and register-immediate
; The add instruction moves to the delay slot; the ld.w copy stays before ret.d.
;-------------------------------------------------------------------------------

; CHECK-LABEL: add_rr:
; CHECK: ld.w %r10, %r12
; CHECK-NEXT: ret.d
; CHECK-NEXT: add %r10, %r13
define i32 @add_rr(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; add with small immediate (fits in uimm6)
; CHECK-LABEL: add_ri:
; CHECK: ld.w %r10, %r12
; CHECK-NEXT: ret.d
; CHECK-NEXT: add %r10, 5
define i32 @add_ri(i32 %a) {
  %r = add i32 %a, 5
  ret i32 %r
}

;-------------------------------------------------------------------------------
; sub — register-register
;-------------------------------------------------------------------------------

; CHECK-LABEL: sub_rr:
; CHECK: ld.w %r10, %r12
; CHECK-NEXT: ret.d
; CHECK-NEXT: sub %r10, %r13
define i32 @sub_rr(i32 %a, i32 %b) {
  %r = sub i32 %a, %b
  ret i32 %r
}

;-------------------------------------------------------------------------------
; ld.w — register-indirect load
; Memory access cannot fill delay slot → keep plain ret.
;-------------------------------------------------------------------------------

; CHECK-LABEL: ldw_ri:
; CHECK: ld.w %r10, [%r12]
; CHECK-NEXT: ret
define i32 @ldw_ri(ptr %p) {
  %v = load i32, ptr %p
  ret i32 %v
}

;-------------------------------------------------------------------------------
; ld.b — sign-extending byte load
;-------------------------------------------------------------------------------

; CHECK-LABEL: ldb_ri:
; CHECK: ld.b %r10, [%r12]
; CHECK-NEXT: ret
define i32 @ldb_ri(ptr %p) {
  %v = load i8, ptr %p
  %ext = sext i8 %v to i32
  ret i32 %ext
}

;-------------------------------------------------------------------------------
; ld.ub — zero-extending byte load
;-------------------------------------------------------------------------------

; CHECK-LABEL: ldub_ri:
; CHECK: ld.ub %r10, [%r12]
; CHECK-NEXT: ret
define i32 @ldub_ri(ptr %p) {
  %v = load i8, ptr %p
  %ext = zext i8 %v to i32
  ret i32 %ext
}
