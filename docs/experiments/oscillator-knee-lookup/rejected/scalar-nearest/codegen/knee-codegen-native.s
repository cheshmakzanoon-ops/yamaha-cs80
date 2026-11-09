	.file	"knee-codegen.cpp"
	.intel_syntax noprefix
	.text
	.p2align 4
	.globl	_Z7oldKneeddi
	.type	_Z7oldKneeddi, @function
_Z7oldKneeddi:
.LFB2953:
	.cfi_startproc
	vmovsd	xmm6, QWORD PTR .LC0[rip]
	vmovq	xmm5, QWORD PTR .LC1[rip]
	vsubsd	xmm3, xmm0, xmm6
	vandpd	xmm3, xmm3, xmm5
	vmovsd	xmm4, QWORD PTR .LC2[rip]
	vxorps	xmm2, xmm2, xmm2
	vcomisd	xmm4, xmm3
	ja	.L19
	vmovsd	xmm6, QWORD PTR .LC6[rip]
	vsubsd	xmm3, xmm0, xmm6
	vandpd	xmm3, xmm3, xmm5
	vcomisd	xmm4, xmm3
	jbe	.L16
.L19:
	vcvtsi2sd	xmm2, xmm2, edi
	vmovq	xmm0, QWORD PTR .LC5[rip]
	vmulsd	xmm2, xmm2, xmm1
	vpternlogq	xmm0, xmm2, QWORD PTR .LC4[rip]{1to2}, 248
	vaddsd	xmm0, xmm0, xmm6
	ret
.L16:
	vmovsd	xmm3, QWORD PTR .LC7[rip]
	vsubsd	xmm6, xmm0, xmm3
	vandpd	xmm6, xmm6, xmm5
	vcomisd	xmm4, xmm6
	jbe	.L17
.L20:
	vcvtsi2sd	xmm2, xmm2, edi
	vmovq	xmm0, QWORD PTR .LC5[rip]
	vmulsd	xmm2, xmm2, xmm1
	vpternlogq	xmm0, xmm2, QWORD PTR .LC4[rip]{1to2}, 248
	vaddsd	xmm0, xmm0, xmm3
	ret
.L17:
	vmovsd	xmm3, QWORD PTR .LC8[rip]
	vsubsd	xmm6, xmm0, xmm3
	vandpd	xmm5, xmm6, xmm5
	vcomisd	xmm4, xmm5
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
	vcomisd	xmm0, QWORD PTR .LC9[rip]
	jb	.L24
	vmovsd	xmm2, QWORD PTR .LC10[rip]
	vcomisd	xmm2, xmm0
	jb	.L24
	vxorps	xmm3, xmm3, xmm3
	vmovsd	xmm2, QWORD PTR .LC11[rip]
	vfmadd213sd	xmm2, xmm0, QWORD PTR .LC6[rip]
	vcvttsd2si	eax, xmm2
	vcvtsi2sd	xmm2, xmm3, eax
	vmulsd	xmm2, xmm2, QWORD PTR .LC0[rip]
	vsubsd	xmm4, xmm0, xmm2
	vandpd	xmm4, xmm4, XMMWORD PTR .LC1[rip]
	vmovsd	xmm5, QWORD PTR .LC2[rip]
	vcomisd	xmm5, xmm4
	jbe	.L24
	vcvtsi2sd	xmm3, xmm3, edi
	vmovq	xmm0, QWORD PTR .LC5[rip]
	vmulsd	xmm3, xmm3, xmm1
	vpternlogq	xmm0, xmm3, QWORD PTR .LC4[rip]{1to2}, 248
	vaddsd	xmm0, xmm2, xmm0
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
	.set	.LC2,.LC5
	.section	.rodata.cst8
	.align 8
.LC4:
	.long	0
	.long	-2147483648
	.section	.rodata.cst16
	.align 16
.LC5:
	.long	-512138091
	.long	1035337087
	.long	0
	.long	0
	.section	.rodata.cst8
	.align 8
.LC6:
	.long	0
	.long	1071644672
	.align 8
.LC7:
	.long	0
	.long	1072168960
	.align 8
.LC8:
	.long	0
	.long	1072693248
	.align 8
.LC9:
	.long	-1717986918
	.long	1070176665
	.align 8
.LC10:
	.long	-1717986918
	.long	1072798105
	.align 8
.LC11:
	.long	0
	.long	1074790400
	.ident	"GCC: (Debian 14.2.0-19) 14.2.0"
	.section	.note.GNU-stack,"",@progbits
