# RUN: llvm-mc --triple=s1c33-none-elf -filetype=obj %s -o %t
# RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s
#
# AsmParser coverage for special-purpose instructions (TODO #10).
#
# Tests:
#   - swap %rd, %rs      (halfword swap, Class 4)
#   - btst/bclr/bset/bnot [%rb], imm3  (bit operations on memory, Class 5)
#   - ld.w %rd, %sp/%psr/%alr/%ahr     (read special registers to GPR)
#   - ld.w %sp/%psr/%alr/%ahr, %rs     (write GPR to special registers)

	.text

# CHECK-LABEL: <test_swap>:
test_swap:
	swap  %r0, %r1
	swap  %r2, %r3
	swap  %r15, %r0
# CHECK: swap	%r0, %r1
# CHECK: swap	%r2, %r3
# CHECK: swap	%r15, %r0

# CHECK-LABEL: <test_bit_ops>:
# Bit operations use [%rb] (memory byte) and an immediate 3-bit index.
test_bit_ops:
	btst  [%r0], 0
	btst  [%r4], 3
	btst  [%r7], 7
	bclr  [%r1], 1
	bclr  [%r4], 3
	bclr  [%r15], 7
	bset  [%r2], 2
	bset  [%r4], 5
	bset  [%r15], 7
	bnot  [%r3], 3
	bnot  [%r4], 7
	bnot  [%r15], 0
# CHECK: btst	[%r0], 0
# CHECK: btst	[%r4], 3
# CHECK: btst	[%r7], 7
# CHECK: bclr	[%r1], 1
# CHECK: bclr	[%r4], 3
# CHECK: bclr	[%r15], 7
# CHECK: bset	[%r2], 2
# CHECK: bset	[%r4], 5
# CHECK: bset	[%r15], 7
# CHECK: bnot	[%r3], 3
# CHECK: bnot	[%r4], 7
# CHECK: bnot	[%r15], 0

# CHECK-LABEL: <test_read_special>:
# Read special registers into GPRs: Class 5, op1=001, op2=00.
# Encoding: 0xA400 | (ss<<4) | rd
#   psr=0: 0xA400|rd,  sp=1: 0xA410|rd,  alr=2: 0xA420|rd,  ahr=3: 0xA430|rd
test_read_special:
	ld.w  %r0,  %sp
	ld.w  %r1,  %psr
	ld.w  %r2,  %alr
	ld.w  %r3,  %ahr
	ld.w  %r15, %sp
	ld.w  %r15, %ahr
# CHECK: ld.w	%r0, %sp
# CHECK: ld.w	%r1, %psr
# CHECK: ld.w	%r2, %alr
# CHECK: ld.w	%r3, %ahr
# CHECK: ld.w	%r15, %sp
# CHECK: ld.w	%r15, %ahr

# CHECK-LABEL: <test_write_special>:
# Write GPRs to special registers: Class 5, op1=000, op2=00.
# Encoding: 0xA000 | (rs<<4) | sd
#   psr=0: 0xA000|(rs<<4),  sp=1: 0xA001|(rs<<4),  alr=2: 0xA002|(rs<<4),  ahr=3: 0xA003|(rs<<4)
test_write_special:
	ld.w  %sp,  %r0
	ld.w  %psr, %r1
	ld.w  %alr, %r2
	ld.w  %ahr, %r3
	ld.w  %sp,  %r15
	ld.w  %ahr, %r15
# CHECK: ld.w	%sp, %r0
# CHECK: ld.w	%psr, %r1
# CHECK: ld.w	%alr, %r2
# CHECK: ld.w	%ahr, %r3
# CHECK: ld.w	%sp, %r15
# CHECK: ld.w	%ahr, %r15
