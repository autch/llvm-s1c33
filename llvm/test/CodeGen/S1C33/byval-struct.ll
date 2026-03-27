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
; CHECK: sub %sp, 28
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
; CHECK: sub %sp, 8
; CHECK: ld.w [%r{{[0-9]+}}],
; CHECK: call takes_s8
  call void @takes_s8(ptr byval(%struct.S8) align 4 %p)
  ret void
}

; Mixed: scalar i32 in R12, struct on stack, scalar i32 in R13
define void @call_mixed(i32 %a, ptr %p, i32 %b) {
; CHECK-LABEL: call_mixed:
; CHECK: sub %sp, 8
; Struct words stored to stack; scalar b moves to R13 (was R14)
; Scheduler may reorder the mov and stores.
; CHECK-DAG: ld.w %r13, %r14
; CHECK: call takes_mixed
  call void @takes_mixed(i32 %a, ptr byval(%struct.S8) align 4 %p, i32 %b)
  ret void
}
