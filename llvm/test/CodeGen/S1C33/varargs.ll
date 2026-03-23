; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Variadic function ABI verification (gcc33 / S5U1C33000C):
;   Callee: all args (fixed + variadic) on the stack; no arg registers used.
;   va_list is a plain i32 pointer to the first variadic argument.
;   va_start initialises the pointer to the absolute runtime address (SP+offset).
;   va_copy is a simple 4-byte pointer copy.

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
declare void @llvm.va_copy(ptr, ptr)

;-------------------------------------------------------------------------------
; Test 1: variadic callee — va_start must emit SP+offset, not a literal
;
; Stack layout (1 fixed arg, 1 vararg, 4-byte %va alloca):
;   [SP+0]  %va alloca        (4 bytes local frame)
;   [SP+4]  return address
;   [SP+8]  %n  (CC offset 0 → actual 0 + 4 + 4 = 8)
;   [SP+12] first vararg (CC offset 4 → actual 4 + 4 + 4 = 12)
;
; va_start must store SP+12 (runtime address) into %va, not the literal 12.
;-------------------------------------------------------------------------------

; CHECK-LABEL: varargs_callee:
; Prologue allocates 4 bytes for %va.
; CHECK: sub %sp, 4
; ADJFI materialises the first-vararg address as SP+12 into a scratch register.
; CHECK: ld.w %r{{[0-9]+}}, %sp
; CHECK-NEXT: add %r{{[0-9]+}}, 12
; Store that address into %va (at [SP+0]).
; CHECK: ld.w [%sp+0], %r{{[0-9]+}}
; Load the first vararg through the pointer.
; CHECK: ld.w %r10, [%r{{[0-9]+}}]
; CHECK: ret.d
define i32 @varargs_callee(i32 %n, ...) {
  %va = alloca ptr, align 4
  call void @llvm.va_start(ptr %va)
  %ap = load ptr, ptr %va
  %v = load i32, ptr %ap
  call void @llvm.va_end(ptr %va)
  ret i32 %v
}

;-------------------------------------------------------------------------------
; Test 2: caller site — when calling a variadic function all args go to stack
;
; call vprintf(fmt=null, x, y) with CLI.IsVarArg=true.
; R12–R15 must NOT be used for argument passing; all 3 args go to [SP+N].
;-------------------------------------------------------------------------------

declare i32 @vprintf(ptr %fmt, ...)

; CHECK-LABEL: call_varargs:
; Arguments stored to stack (not to R12/R13/R14/R15).
; CHECK-NOT: ld.w %r12
; CHECK-NOT: ld.w %r13
; CHECK-NOT: ld.w %r14
; CHECK-NOT: ld.w %r15
; All three args end up in SP-relative store locations.
; CHECK: ld.w [%r{{[0-9]+}}
; CHECK: call vprintf
define i32 @call_varargs(i32 %x, i32 %y) {
  %r = call i32 (ptr, ...) @vprintf(ptr null, i32 %x, i32 %y)
  ret i32 %r
}

;-------------------------------------------------------------------------------
; Test 3: va_copy — copies the pointer value, not the pointee
;
; Stack layout (2 alloca ptr = 8 bytes local, 1 fixed arg, 1 vararg):
;   [SP+0]  va1 alloca
;   [SP+4]  va2 alloca
;   [SP+8]  return address
;   [SP+12] %n  (CC offset 0 → actual 0 + 8 + 4 = 12)
;   [SP+16] first vararg (CC offset 4 → actual 4 + 8 + 4 = 16)
;-------------------------------------------------------------------------------

; CHECK-LABEL: varargs_copy:
; Prologue allocates 8 bytes for both %va1 and %va2.
; CHECK: sub %sp, 8
; va_start materialises SP+16 into a register.
; CHECK: ld.w %r{{[0-9]+}}, %sp
; CHECK-NEXT: add %r{{[0-9]+}}, 16
; Store address to va1.
; CHECK: ld.w [%sp+0], %r{{[0-9]+}}
; va_copy stores the same address to va2.
; CHECK: ld.w [%sp+4], %r{{[0-9]+}}
; CHECK: ret.d
define i32 @varargs_copy(i32 %n, ...) {
  %va1 = alloca ptr, align 4
  %va2 = alloca ptr, align 4
  call void @llvm.va_start(ptr %va1)
  call void @llvm.va_copy(ptr %va2, ptr %va1)
  %ap = load ptr, ptr %va2
  %v = load i32, ptr %ap
  call void @llvm.va_end(ptr %va1)
  call void @llvm.va_end(ptr %va2)
  ret i32 %v
}

;-------------------------------------------------------------------------------
; Test 4: realistic va_arg loop — va_list pointer advances by 4 each iteration
;
; Tests that the va_list (an i32*) is incremented correctly as a true pointer.
;-------------------------------------------------------------------------------

; CHECK-LABEL: sum:
; va_start sets va = SP+20 (12 bytes local + 4 retaddr + 4 for %n).
; CHECK: ld.w %r{{[0-9]+}}, %sp
; CHECK-NEXT: add %r{{[0-9]+}}, 20
; In the loop body, the pointer is dereferenced and advanced by 4.
; CHECK: ld.w %r{{[0-9]+}}, [%r{{[0-9]+}}]
; CHECK: add %r{{[0-9]+}}, 4
define i32 @sum(i32 %n, ...) {
entry:
  %va = alloca ptr, align 4
  call void @llvm.va_start(ptr %va)
  %i = alloca i32, align 4
  store i32 0, ptr %i
  %acc = alloca i32, align 4
  store i32 0, ptr %acc
  br label %loop
loop:
  %iv = load i32, ptr %i
  %cmp = icmp slt i32 %iv, %n
  br i1 %cmp, label %body, label %done
body:
  %cur = load ptr, ptr %va
  %val = load i32, ptr %cur
  %next = getelementptr i8, ptr %cur, i32 4
  store ptr %next, ptr %va
  %acv = load i32, ptr %acc
  %sum = add i32 %acv, %val
  store i32 %sum, ptr %acc
  %iv2 = add i32 %iv, 1
  store i32 %iv2, ptr %i
  br label %loop
done:
  %result = load i32, ptr %acc
  call void @llvm.va_end(ptr %va)
  ret i32 %result
}
