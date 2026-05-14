; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Variadic function ABI verification (gcc33 / S5U1C33000C):
;   Every named argument EXCEPT the last is passed per the normal CC
;   (R12-R15, overflow to stack); the LAST named argument and all variadic
;   arguments are passed on the stack, contiguously, so the callee's
;   va_start(ap, lastnamed) finds the variadic area right after &lastnamed.
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
; Prologue allocates 1 word (4 bytes) for %va.
; CHECK: sub %sp, 1
; ADJFI materialises the first-vararg address as SP+12 into a scratch register.
; CHECK: ld.w %r{{[0-9]+}}, %sp
; CHECK-NEXT: add %r{{[0-9]+}}, 12
; Store that address into %va (at [SP+0]).
; CHECK: ld.w [%sp+0], %r{{[0-9]+}}
; Load the first vararg through the pointer.
; CHECK: ld.w %r10, [%r{{[0-9]+}}]
; CHECK: ret
define i32 @varargs_callee(i32 %n, ...) {
  %va = alloca ptr, align 4
  call void @llvm.va_start(ptr %va)
  %ap = load ptr, ptr %va
  %v = load i32, ptr %ap
  call void @llvm.va_end(ptr %va)
  ret i32 %v
}

;-------------------------------------------------------------------------------
; Test 2: caller site, single named param — all args go to stack
;
; call vprintf(fmt=null, x, y) with CLI.IsVarArg=true.  vprintf's only named
; parameter (%fmt) IS the last named one, so it and both varargs go on the
; stack; R12–R15 must NOT be used for argument passing.
;-------------------------------------------------------------------------------

declare i32 @vprintf(ptr %fmt, ...)

; CHECK-LABEL: call_varargs:
; Arguments stored to stack (not to R12/R13/R14/R15).
; CHECK-NOT: ld.w %r12
; CHECK-NOT: ld.w %r13
; CHECK-NOT: ld.w %r14
; CHECK-NOT: ld.w %r15
; All three args end up in SP-relative Class-2 store locations.
; CHECK: ld.w [%sp+{{[0-9]+}}], %r
; CHECK: call vprintf
define i32 @call_varargs(i32 %x, i32 %y) {
  %r = call i32 (ptr, ...) @vprintf(ptr null, i32 %x, i32 %y)
  ret i32 %r
}

;-------------------------------------------------------------------------------
; Test 2b: caller site, TWO named params — first named arg stays in R12
;
; call vsprintf_like(buf, fmt, x) with CLI.IsVarArg=true.  Here %buf is a
; named parameter that is NOT the last named one, so it must be passed in
; R12 (normal CC), exactly as the gcc33-built kernel/SDK sprintf expects.
; Only %fmt (the last named param) and %x (variadic) go on the stack.
;
; This is the case the old "all args on the stack" convention got wrong:
; the gcc33 sprintf reads its destination buffer from R12, so passing %buf
; on the stack corrupted every sprintf-built string (e.g. game data
; filenames), which is why large P/ECE apps failed to load resources.
;-------------------------------------------------------------------------------

declare i32 @vsprintf_like(ptr %buf, ptr %fmt, ...)

; The parameters are deliberately declared in an order that does NOT already
; place %buf in R12, so the backend must emit a real move into R12.
; CHECK-LABEL: call_two_named:
; The first named arg (%buf) is moved into R12; the last named arg (%fmt) and
; the variadic arg (%x) go on the stack.  The three arg-setup stores may be
; scheduled in any order, so match them order-independently.
; CHECK-DAG: ld.w %r12, %r
; CHECK-DAG: ld.w [%sp+{{[0-9]+}}], %r
; CHECK-DAG: ld.w [%sp+{{[0-9]+}}], %r
; CHECK: call vsprintf_like
define i32 @call_two_named(i32 %x, ptr %fmt, ptr %buf) {
  %r = call i32 (ptr, ptr, ...) @vsprintf_like(ptr %buf, ptr %fmt, i32 %x)
  ret i32 %r
}

;-------------------------------------------------------------------------------
; Test 2c: variadic callee with TWO named params
;
; varargs_callee2(i32 %a, i32 %b, ...): %a (first named) arrives in R12;
; %b (last named) arrives on the stack at CC offset 0; the first variadic
; argument follows at CC offset 4.
;
; Stack layout (4-byte %va alloca):
;   [SP+0]  %va alloca
;   [SP+4]  return address
;   [SP+8]  %b               (CC offset 0 → 0 + 4 + 4)
;   [SP+12] first vararg     (CC offset 4 → 4 + 4 + 4)
;
; va_start must therefore materialise SP+12.
;-------------------------------------------------------------------------------

; CHECK-LABEL: varargs_callee2:
; CHECK: sub %sp, 1
; va_start materialises the first-vararg address as SP+12.  The scheduler may
; interleave the %b stack load and the %a register copy between the two halves.
; CHECK: ld.w [[VA:%r[0-9]+]], %sp
; CHECK: add [[VA]], 12
; CHECK: ret
define i32 @varargs_callee2(i32 %a, i32 %b, ...) {
  %va = alloca ptr, align 4
  call void @llvm.va_start(ptr %va)
  %ap = load ptr, ptr %va
  %v = load i32, ptr %ap
  call void @llvm.va_end(ptr %va)
  %s1 = add i32 %a, %b
  %s2 = add i32 %s1, %v
  ret i32 %s2
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
; Prologue allocates 2 words (8 bytes) for both %va1 and %va2.
; CHECK: sub %sp, 2
; va_start materialises SP+16 into a register.
; CHECK: ld.w %r{{[0-9]+}}, %sp
; CHECK-NEXT: add %r{{[0-9]+}}, 16
; va1 and va2 receive the same address; the two stores are independent and
; the scheduler may emit them in either order.
; CHECK-DAG: ld.w [%sp+0], %r{{[0-9]+}}
; CHECK-DAG: ld.w [%sp+1], %r{{[0-9]+}}
; CHECK: ret
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
; The "ld.w %rN, %sp" + "add %rN, 20" pair must address the same register;
; the scheduler may interleave an independent prologue load between them.
; CHECK: ld.w [[VA:%r[0-9]+]], %sp
; CHECK: add [[VA]], 20
; In the loop body, the pointer is dereferenced and advanced by 4 via
; post-increment load (load + GEP+4 are fused into ld.w [%rb]+).
; CHECK: ld.w %r{{[0-9]+}}, [%r{{[0-9]+}}]+
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

;-------------------------------------------------------------------------------
; Test 5: va_arg instruction — exercises ISD::VAARG expansion
;
; Uses the IR-level va_arg instruction directly (as Clang emits for va_arg()
; in C code).  The expansion loads the current pointer from va_list, bumps it
; by sizeof(type), stores back, and loads the value.
;-------------------------------------------------------------------------------

; CHECK-LABEL: vaarg_direct:
; CHECK: sub %sp,
; va_start
; CHECK: ld.w %r{{[0-9]+}}, %sp
; va_arg expansion: load va_list ptr, bump by 4, store back, load value
; CHECK: add %r{{[0-9]+}}, 4
; CHECK: ld.w %r10, [%r{{[0-9]+}}]
; CHECK: ret
define i32 @vaarg_direct(i32 %n, ...) {
entry:
  %va = alloca ptr, align 4
  call void @llvm.va_start(ptr %va)
  %v = va_arg ptr %va, i32
  call void @llvm.va_end(ptr %va)
  ret i32 %v
}
