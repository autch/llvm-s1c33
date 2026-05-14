; RUN: llc -march=s1c33 -verify-machineinstrs < %s | FileCheck %s

; Test that byval struct arguments go entirely on the stack per the
; S5U1C33000C ABI (gcc33 convention).  Registers are NOT used for struct words.

%struct.S28 = type { i32, i32, i32, i32, i32, i32, i32 }
%struct.S8 = type { i32, i32 }

declare void @takes_s28(ptr byval(%struct.S28) align 4)
declare void @takes_s8(ptr byval(%struct.S8) align 4)
declare void @takes_mixed(i32, ptr byval(%struct.S8) align 4, i32)

; 28-byte struct (7 words): ALL on stack, none in registers
define void @call_s28(ptr %p) {
; CHECK-LABEL: call_s28:
; CHECK: sub %sp, 7
; CHECK-NOT: ld.w %r12, [%r
; CHECK-NOT: ld.w %r13, [%r
; CHECK-NOT: ld.w %r14, [%r
; CHECK-NOT: ld.w %r15, [%r
; CHECK: call takes_s28
  call void @takes_s28(ptr byval(%struct.S28) align 4 %p)
  ret void
}

; 8-byte struct (2 words): ALL on stack
define void @call_s8(ptr %p) {
; CHECK-LABEL: call_s8:
; CHECK: sub %sp, 2
; CHECK: ld.w [%sp+{{[0-9]+}}], %r
; CHECK: call takes_s8
  call void @takes_s8(ptr byval(%struct.S8) align 4 %p)
  ret void
}

; Mixed: scalar i32 in R12, struct on stack, scalar i32 in R13
define void @call_mixed(i32 %a, ptr %p, i32 %b) {
; CHECK-LABEL: call_mixed:
; CHECK: sub %sp, 2
; Struct words stored to stack; scalar b moves to R13 (was R14)
; Scheduler may reorder the mov and stores.
; CHECK-DAG: ld.w %r13, %r14
; CHECK: call takes_mixed
  call void @takes_mixed(i32 %a, ptr byval(%struct.S8) align 4 %p, i32 %b)
  ret void
}

; Callee side — a function that RECEIVES a byval struct must read it from the
; incoming stack area, NOT expect a pointer in an argument register.  The
; caller (call_s8 above) copies the struct words onto the stack, so the
; callee's byval pointer is the address of that incoming stack slot.  This is
; the case the old codegen got wrong: LowerFormalArguments ran the byval
; pointer through the normal CC and expected it in R12, while LowerCall put
; the struct words on the stack — caller and callee disagreed.

; Single byval struct: field 1 read via SP-relative addressing, not [%r12].
define i32 @recv_s8(ptr byval(%struct.S8) align 4 %s) {
; CHECK-LABEL: recv_s8:
; CHECK-NOT: ld.w %r{{[0-9]+}}, [%r12]
; CHECK: ld.w %r10, [%sp+{{[0-9]+}}]
; CHECK: ret
  %p1 = getelementptr %struct.S8, ptr %s, i32 0, i32 1
  %v = load i32, ptr %p1
  ret i32 %v
}

; Mixed: scalar %a in R12, byval struct on the stack, scalar %b in R13.
; The struct must not be expected in R13/R14.
define i32 @recv_mixed(i32 %a, ptr byval(%struct.S8) align 4 %s, i32 %b) {
; CHECK-LABEL: recv_mixed:
; CHECK-NOT: ld.w %r{{[0-9]+}}, [%r13]
; CHECK-NOT: ld.w %r{{[0-9]+}}, [%r14]
; CHECK: ld.w %r{{[0-9]+}}, [%sp+{{[0-9]+}}]
; CHECK: ret
  %p0 = getelementptr %struct.S8, ptr %s, i32 0, i32 0
  %v = load i32, ptr %p0
  %s1 = add i32 %v, %a
  %s2 = add i32 %s1, %b
  ret i32 %s2
}

; --- gcc33 single-element-struct register passing (DESIGN_SPEC §3.5) ----------
;
; A "single-element struct" of <= 32 bits is NOT passed on the stack like every
; other struct — gcc33 passes it in an argument register exactly as its scalar
; element would be passed.  clang coerces such a struct to an i8/i16/i32 marked
; `inreg`; the 8- and 16-bit forms carry their value in the HIGH bits of the
; register (a leftover from gcc33's MIPS big-endian origin).  The `inreg` flag
; is what tells a coerced struct apart from an ordinary i8/i16 argument — only
; the former is high-bit-packed.

declare void @take_se16(i16 inreg)
declare void @take_se8(i8 inreg)

; Callee: a 16-bit single-element struct arrives in the high half of R12 and is
; shifted down by 16 before use.
define i32 @recv_se16(i16 inreg %v) {
; CHECK-LABEL: recv_se16:
; CHECK: srl %r{{[0-9]+}}, 8
; CHECK: srl %r{{[0-9]+}}, 8
  %z = zext i16 %v to i32
  ret i32 %z
}

; Callee: an 8-bit single-element struct sits in the top byte — shifted down 24.
define i32 @recv_se8(i8 inreg %v) {
; CHECK-LABEL: recv_se8:
; CHECK: srl %r{{[0-9]+}}, 8
; CHECK: srl %r{{[0-9]+}}, 8
; CHECK: srl %r{{[0-9]+}}, 8
  %z = zext i8 %v to i32
  ret i32 %z
}

; Callee: a 32-bit single-element struct needs no repacking — it arrives in the
; register exactly like a plain i32.
define i32 @recv_se32(i32 inreg %v) {
; CHECK-LABEL: recv_se32:
; CHECK-NOT: srl
; CHECK-NOT: sll
  ret i32 %v
}

; An ordinary i16 argument (NO inreg) must NOT be high-bit-packed — it is a
; plain value in the low bits, not a coerced struct.
define i32 @recv_plain16(i16 %v) {
; CHECK-LABEL: recv_plain16:
; CHECK-NOT: srl %r{{[0-9]+}}, 8
  %z = zext i16 %v to i32
  ret i32 %z
}

; Caller: passing a 16-bit single-element struct shifts the value up into the
; high half before the call.
define void @pass_se16(i32 %x) {
; CHECK-LABEL: pass_se16:
; CHECK: sll %r12, 8
; CHECK: sll %r12, 8
; CHECK: call take_se16
  %t = trunc i32 %x to i16
  call void @take_se16(i16 inreg %t)
  ret void
}

; Caller: an 8-bit single-element struct is shifted up by 24.
define void @pass_se8(i32 %x) {
; CHECK-LABEL: pass_se8:
; CHECK: sll %r12, 8
; CHECK: sll %r12, 8
; CHECK: sll %r12, 8
; CHECK: call take_se8
  %t = trunc i32 %x to i8
  call void @take_se8(i8 inreg %t)
  ret void
}
