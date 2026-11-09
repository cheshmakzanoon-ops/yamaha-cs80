	.file	"knee-codegen.cpp"
	.intel_syntax noprefix
	.text
	.p2align 4
	.globl	_Z7oldKneeddi
	.type	_Z7oldKneeddi, @function
_Z7oldKneeddi:
.LFB2953:
	.cfi_startproc
	movsd	xmm5, QWORD PTR .LC0[rip]
	movapd	xmm2, xmm0
	movq	xmm4, QWORD PTR .LC1[rip]
	movsd	xmm3, QWORD PTR .LC2[rip]
	subsd	xmm2, xmm5
	andpd	xmm2, xmm4
	comisd	xmm3, xmm2
	ja	.L19
	movsd	xmm5, QWORD PTR .LC5[rip]
	movapd	xmm2, xmm0
	subsd	xmm2, xmm5
	andpd	xmm2, xmm4
	comisd	xmm3, xmm2
	jbe	.L16
.L19:
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, edi
	mulsd	xmm0, xmm1
	andpd	xmm0, XMMWORD PTR .LC3[rip]
	orpd	xmm0, XMMWORD PTR .LC4[rip]
	addsd	xmm0, xmm5
	ret
.L16:
	movsd	xmm2, QWORD PTR .LC6[rip]
	movapd	xmm5, xmm0
	subsd	xmm5, xmm2
	andpd	xmm5, xmm4
	comisd	xmm3, xmm5
	jbe	.L17
.L20:
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, edi
	mulsd	xmm0, xmm1
	andpd	xmm0, XMMWORD PTR .LC3[rip]
	orpd	xmm0, XMMWORD PTR .LC4[rip]
	addsd	xmm0, xmm2
	ret
.L17:
	movsd	xmm2, QWORD PTR .LC7[rip]
	movapd	xmm5, xmm0
	subsd	xmm5, xmm2
	andpd	xmm4, xmm5
	comisd	xmm3, xmm4
	ja	.L20
	ret
	.cfi_endproc
.LFE2953:
	.size	_Z7oldKneeddi, .-_Z7oldKneeddi
	.p2align 4
	.globl	_Z7newKneeddi
	.type	_Z7newKneeddi, @function
_Z7newKneeddi:
.LFB2954:
	.cfi_startproc
	comisd	xmm0, QWORD PTR .LC8[rip]
	jb	.L24
	movsd	xmm2, QWORD PTR .LC9[rip]
	comisd	xmm2, xmm0
	jb	.L24
	movsd	xmm2, QWORD PTR .LC10[rip]
	movapd	xmm3, xmm0
	movsd	xmm4, QWORD PTR .LC2[rip]
	mulsd	xmm2, xmm0
	addsd	xmm2, QWORD PTR .LC5[rip]
	cvttsd2si	eax, xmm2
	pxor	xmm2, xmm2
	cvtsi2sd	xmm2, eax
	mulsd	xmm2, QWORD PTR .LC0[rip]
	subsd	xmm3, xmm2
	andpd	xmm3, XMMWORD PTR .LC1[rip]
	comisd	xmm4, xmm3
	jbe	.L24
	pxor	xmm0, xmm0
	cvtsi2sd	xmm0, edi
	mulsd	xmm1, xmm0
	movq	xmm0, QWORD PTR .LC3[rip]
	andpd	xmm0, xmm1
	orpd	xmm0, XMMWORD PTR .LC4[rip]
	addsd	xmm0, xmm2
.L24:
	ret
	.cfi_endproc
.LFE2954:
	.size	_Z7newKneeddi, .-_Z7newKneeddi
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC0:
	.long	0
	.long	1070596096
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC1:
	.long	-1
	.long	2147483647
	.long	0
	.long	0
	.set	.LC2,.LC4
	.align 16
.LC3:
	.long	0
	.long	-2147483648
	.long	0
	.long	0
	.align 16
.LC4:
	.long	-512138091
	.long	1035337087
	.long	0
	.long	0
	.section	.rodata.cst8
	.align 8
.LC5:
	.long	0
	.long	1071644672
	.align 8
.LC6:
	.long	0
	.long	1072168960
	.align 8
.LC7:
	.long	0
	.long	1072693248
	.align 8
.LC8:
	.long	-1717986918
	.long	1070176665
	.align 8
.LC9:
	.long	-1717986918
	.long	1072798105
	.align 8
.LC10:
	.long	0
	.long	1074790400
	.ident	"GCC: (Debian 14.2.0-19) 14.2.0"
	.section	.note.GNU-stack,"",@progbits
