# RUN: llvm-mc --triple=s1c33-none-elf -filetype=obj %s -o %t
# RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s
#
# Data access instruction encoding and addressing mode coverage (TODO #9).
#
# S1C33 has three addressing classes for memory operations:
#   Class 1: [%rb]    — register-indirect (no offset)
#   Class 2: [%sp+N]  — SP-relative with 6-bit signed offset (ext-extendable)
#   Class 4: ld.w %rd, imm6 — immediate load (fits in 6-bit signed imm)
#
# The ext instruction extends operands: one ext gives 19-bit, two exts give
# full 28-bit range.  For symbol addresses from codegen, the LDW_SYM_EXT0/1/2
# relaxation chain handles this; see global-relocs.ll for the EXT2 path.

	.text

# CHECK-LABEL: <sp_loads>:
sp_loads:
	ld.w  %r0, [%sp+0]
	ld.w  %r1, [%sp+4]
	ld.ub %r2, [%sp+8]
	ld.b  %r3, [%sp+12]
	ld.uh %r4, [%sp+16]
	ld.h  %r5, [%sp+20]
# CHECK: ld.w	%r0, [%sp+0]
# CHECK: ld.w	%r1, [%sp+4]
# CHECK: ld.ub	%r2, [%sp+8]
# CHECK: ld.b	%r3, [%sp+12]
# CHECK: ld.uh	%r4, [%sp+16]
# CHECK: ld.h	%r5, [%sp+20]

# CHECK-LABEL: <sp_stores>:
sp_stores:
	ld.w  [%sp+4],  %r0
	ld.b  [%sp+8],  %r1
	ld.h  [%sp+12], %r2
# CHECK: ld.w	[%sp+4], %r0
# CHECK: ld.b	[%sp+8], %r1
# CHECK: ld.h	[%sp+12], %r2

# CHECK-LABEL: <sp_ext>:
# With EXT, SP-relative class-2 uses a byte displacement directly.
# ext 4 / ld.w [%sp+0] encodes byte offset 256.
sp_ext:
	ext   4
	ld.w  %r0, [%sp+0]
	ext   4
	ld.w  [%sp+0], %r1
# CHECK: ext	4
# CHECK-NEXT: ld.w	%r0, [%sp+0]
# CHECK: ext	4
# CHECK-NEXT: ld.w	[%sp+0], %r1

# CHECK-LABEL: <ri_loads>:
ri_loads:
	ld.w  %r0, [%r1]
	ld.ub %r0, [%r1]
	ld.b  %r0, [%r1]
	ld.uh %r0, [%r1]
	ld.h  %r0, [%r1]
# CHECK: ld.w	%r0, [%r1]
# CHECK: ld.ub	%r0, [%r1]
# CHECK: ld.b	%r0, [%r1]
# CHECK: ld.uh	%r0, [%r1]
# CHECK: ld.h	%r0, [%r1]

# CHECK-LABEL: <ri_stores>:
ri_stores:
	ld.w  [%r1], %r0
	ld.b  [%r1], %r0
	ld.h  [%r1], %r0
# CHECK: ld.w	[%r1], %r0
# CHECK: ld.b	[%r1], %r0
# CHECK: ld.h	[%r1], %r0

# CHECK-LABEL: <ri_postinc>:
ri_postinc:
	ld.w  %r0, [%r1]+
	ld.ub %r0, [%r1]+
	ld.b  %r0, [%r1]+
	ld.h  %r0, [%r1]+
	ld.uh %r0, [%r1]+
	ld.w  [%r1]+, %r0
	ld.b  [%r1]+, %r0
	ld.h  [%r1]+, %r0
# CHECK: ld.w	%r0, [%r1]+
# CHECK: ld.ub	%r0, [%r1]+
# CHECK: ld.b	%r0, [%r1]+
# CHECK: ld.h	%r0, [%r1]+
# CHECK: ld.uh	%r0, [%r1]+
# CHECK: ld.w	[%r1]+, %r0
# CHECK: ld.b	[%r1]+, %r0
# CHECK: ld.h	[%r1]+, %r0

# CHECK-LABEL: <imm_loads>:
# Small immediate loads (isInt<6>: range [-32, 31]) use ld.w %rd, simm6 directly.
imm_loads:
	ld.w  %r0, 0
	ld.w  %r1, 7
	ld.w  %r2, 31
	ld.w  %r3, -1
	ld.w  %r4, -32
# CHECK: ld.w	%r0, 0
# CHECK: ld.w	%r1, 7
# CHECK: ld.w	%r2, 31
# CHECK: ld.w	%r3, -1
# CHECK: ld.w	%r4, -32

# CHECK-LABEL: <imm_with_ext>:
# ext + ld.w %rd, simm6 — the disassembler shows the combined value in a comment.
# ext(N) * 64 + sign_extend(simm6)
imm_with_ext:
	ext   64
	ld.w  %r0, 0
	ext   1
	ld.w  %r0, 0
# CHECK: ext	64
# CHECK-NEXT: ld.w	%r0, 0{{.*}}0x1000
# CHECK: ext	1
# CHECK-NEXT: ld.w	%r0, 0{{.*}}0x40
