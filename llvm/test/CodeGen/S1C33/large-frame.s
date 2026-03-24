	.file	"large-frame.ll"
	.text
	.globl	sp_ext_access                   ; -- Begin function sp_ext_access
	.type	sp_ext_access,@function
sp_ext_access:                          ; @sp_ext_access
	.cfi_startproc
; %bb.0:
	sub	%sp, 68
	ld.w	%r4, 0
	ld.w	[%sp+0], %r4
	ext	1
	ld.w	[%sp+64], %r12
	ret.d
	add	%sp, 68
.Lfunc_end0:
	.size	sp_ext_access, .Lfunc_end0-sp_ext_access
	.cfi_endproc
                                        ; -- End function
	.globl	large_array                     ; -- Begin function large_array
	.type	large_array,@function
large_array:                            ; @large_array
	.cfi_startproc
; %bb.0:
	sub	%sp, 128
	ld.w	[%sp+0], %r12
	ret.d
	add	%sp, 128
.Lfunc_end1:
	.size	large_array, .Lfunc_end1-large_array
	.cfi_endproc
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
