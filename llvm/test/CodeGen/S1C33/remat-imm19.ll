; RUN: llc -mtriple=s1c33-none-elf -O1 -o - %s | FileCheck %s
;
; MOV_ri19 (2-instruction immediate load: ext imm13 + ld.w %rd, sign6) is
; marked isReMaterializable + isAsCheapAsAMove so the register allocator
; re-creates the constant at each use instead of spilling it to the stack
; when pressure is high.  The break-even analysis: spilling a 2-insn
; constant costs 2 (initial materialize) + 1 (store) + N (reloads) = 3 + N,
; while rematerializing costs 2N.  Remat wins at N = 1..2, and the common
; N = 1 case is where rematerialization matters most (the allocator would
; otherwise waste a stack slot on a single-use constant).

declare i32 @work(i32, i32, i32, i32)

; Keep four values live (occupying the callee-saved register class) so the
; allocator cannot park the constant 4096 in a callee-saved register.  With
; remat disabled it would spill 4096 to the stack; with the hints it
; re-emits "ext 64 / ld.w %rN, 0" before the single use.
define i32 @remat_imm19_single_use(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: remat_imm19_single_use:
; The 4096 (= 0x1000) is materialized as ext 64 / ld.w 0 at its use site,
; not reloaded from the stack.
; CHECK:      ext 64
; CHECK-NEXT: ld.w {{%r[0-9]+}}, 0
; CHECK-NEXT: call work
  %r = call i32 @work(i32 %a, i32 %b, i32 %c, i32 4096)
  %s = add i32 %r, %a
  %t = add i32 %s, %b
  %u = add i32 %t, %c
  %v = add i32 %u, %d
  ret i32 %v
}
