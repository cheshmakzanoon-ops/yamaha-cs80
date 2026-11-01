	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 15, 0	sdk_version 15, 5
	.globl	_production_probe               ## -- Begin function production_probe
	.p2align	4, 0x90
_production_probe:                      ## @production_probe
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	xorps	%xmm1, %xmm1
	cmpneqss	%xmm0, %xmm1
	movd	%xmm1, %eax
	andl	$1, %eax
                                        ## kill: def $al killed $al killed $eax
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	_integer_observation_probe      ## -- Begin function integer_observation_probe
	.p2align	4, 0x90
_integer_observation_probe:             ## @integer_observation_probe
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	movss	%xmm0, -4(%rbp)
	testl	$2147483647, -4(%rbp)           ## imm = 0x7FFFFFFF
	setne	%al
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.globl	_main                           ## -- Begin function main
	.p2align	4, 0x90
_main:                                  ## @main
	.cfi_startproc
## %bb.0:
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	subq	$72, %rsp
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	stmxcsr	-104(%rbp)
	movl	-104(%rbp), %eax
	movl	%eax, -92(%rbp)                 ## 4-byte Spill
                                        ## kill: def $eax killed $eax def $rax
	andl	$-32833, %eax                   ## imm = 0xFFFF7FBF
	movq	%rax, -88(%rbp)                 ## 8-byte Spill
	movl	%eax, -72(%rbp)
	ldmxcsr	-72(%rbp)
	movl	$1, -64(%rbp)
	movl	$-2147483647, -60(%rbp)         ## imm = 0x80000001
	movl	$0, -56(%rbp)
	movl	$-2147483648, -52(%rbp)         ## imm = 0x80000000
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movl	%eax, %r15d
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -68(%rbp)                  ## 1-byte Spill
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -46(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -47(%rbp)                  ## 1-byte Spill
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %r12d
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %r14d
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %r13d
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	stmxcsr	-44(%rbp)
	movb	%al, -45(%rbp)                  ## 1-byte Spill
	movl	-44(%rbp), %esi
	movzbl	%r15b, %r15d
	movzbl	%r12b, %r9d
	leaq	L_.str(%rip), %rbx
	movq	%rbx, %rdi
	xorl	%edx, %edx
	movl	$1, %ecx
	movl	%r15d, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %r15b
	stmxcsr	-44(%rbp)
	movzbl	%r15b, %r12d
	movl	-44(%rbp), %esi
	movzbl	-68(%rbp), %r15d                ## 1-byte Folded Reload
	movzbl	%r14b, %r9d
	movq	%rbx, %rdi
	movl	$1, %edx
	movl	$1, %ecx
	movl	%r15d, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %r15b
	movzbl	%r15b, %eax
	stmxcsr	-44(%rbp)
	addl	%r12d, %eax
	movl	%eax, -68(%rbp)                 ## 4-byte Spill
	movl	-44(%rbp), %esi
	movzbl	-46(%rbp), %r14d                ## 1-byte Folded Reload
	movzbl	%r13b, %r9d
	movq	%rbx, %rdi
	movl	$2, %edx
	xorl	%ecx, %ecx
	movl	%r14d, %r8d
	xorl	%eax, %eax
	callq	_printf
	stmxcsr	-44(%rbp)
	movl	-44(%rbp), %esi
	movzbl	-47(%rbp), %r15d                ## 1-byte Folded Reload
	movzbl	-45(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%rbx, %rdi
	movq	%rbx, %r13
	movl	$3, %edx
	xorl	%ecx, %ecx
	movl	%r15d, %r8d
	xorl	%eax, %eax
	callq	_printf
	movq	-88(%rbp), %rax                 ## 8-byte Reload
	addl	$64, %eax
	movl	%eax, -72(%rbp)
	ldmxcsr	-72(%rbp)
	addl	%r15d, %r14d
	movl	$1, -64(%rbp)
	movl	$-2147483647, -60(%rbp)         ## imm = 0x80000001
	movl	$0, -56(%rbp)
	movl	$-2147483648, -52(%rbp)         ## imm = 0x80000000
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movl	%eax, %r12d
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -45(%rbp)                  ## 1-byte Spill
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -46(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -47(%rbp)                  ## 1-byte Spill
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %ebx
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movb	%al, -73(%rbp)                  ## 1-byte Spill
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movb	%al, -74(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	stmxcsr	-44(%rbp)
	movb	%al, -75(%rbp)                  ## 1-byte Spill
	movl	-44(%rbp), %esi
	movzbl	%r12b, %r12d
	movzbl	%bl, %r9d
	movq	%r13, %rdi
	xorl	%edx, %edx
	movl	$1, %ecx
	movl	%r12d, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %r12b
	movzbl	%r12b, %r15d
	addl	%r14d, %r15d
	stmxcsr	-44(%rbp)
	addl	-68(%rbp), %r15d                ## 4-byte Folded Reload
	movl	-44(%rbp), %esi
	movzbl	-45(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-73(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r13, %rdi
	movl	$1, %edx
	movl	$1, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %bl
	stmxcsr	-44(%rbp)
	movzbl	%bl, %r12d
	movl	-44(%rbp), %esi
	movzbl	-46(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-74(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r13, %rdi
	movl	$2, %edx
	xorl	%ecx, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	stmxcsr	-44(%rbp)
	addl	%ebx, %r12d
	movl	-44(%rbp), %esi
	movzbl	-47(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-75(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r13, %rdi
	movl	$3, %edx
	xorl	%ecx, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	movq	-88(%rbp), %rax                 ## 8-byte Reload
	orl	$32768, %eax                    ## imm = 0x8000
	movl	%eax, -72(%rbp)
	ldmxcsr	-72(%rbp)
	addl	%ebx, %r12d
	addl	%r15d, %r12d
	movl	$1, -64(%rbp)
	movl	$-2147483647, -60(%rbp)         ## imm = 0x80000001
	movl	$0, -56(%rbp)
	movl	$-2147483648, -52(%rbp)         ## imm = 0x80000000
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movl	%eax, %r14d
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -46(%rbp)                  ## 1-byte Spill
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -68(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -88(%rbp)                  ## 1-byte Spill
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %ebx
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %r15d
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movb	%al, -45(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	stmxcsr	-44(%rbp)
	movb	%al, -47(%rbp)                  ## 1-byte Spill
	movl	-44(%rbp), %esi
	movzbl	%r14b, %r14d
	movzbl	%bl, %r9d
	movq	%r13, %rdi
	xorl	%edx, %edx
	movl	$1, %ecx
	movl	%r14d, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %r14b
	stmxcsr	-44(%rbp)
	movzbl	%r14b, %r14d
	movl	-44(%rbp), %esi
	movzbl	-46(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	%r15b, %r9d
	movq	%r13, %rdi
	movq	%r13, %r15
	movl	$1, %edx
	movl	$1, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %bl
	movzbl	%bl, %r13d
	stmxcsr	-44(%rbp)
	addl	%r14d, %r13d
	movl	-44(%rbp), %esi
	movzbl	-68(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-45(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r15, %rdi
	movl	$2, %edx
	xorl	%ecx, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	stmxcsr	-44(%rbp)
	addl	%ebx, %r13d
	movl	-44(%rbp), %esi
	movzbl	-88(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-47(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r15, %rdi
	movl	$3, %edx
	xorl	%ecx, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	movl	-92(%rbp), %eax                 ## 4-byte Reload
	orl	$32832, %eax                    ## imm = 0x8040
	movl	%eax, -72(%rbp)
	ldmxcsr	-72(%rbp)
	addl	%ebx, %r13d
	addl	%r12d, %r13d
	movl	$1, -64(%rbp)
	movl	$-2147483647, -60(%rbp)         ## imm = 0x80000001
	movl	$0, -56(%rbp)
	movl	$-2147483648, -52(%rbp)         ## imm = 0x80000000
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movl	%eax, %r14d
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -46(%rbp)                  ## 1-byte Spill
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -68(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_production_probe
	movb	%al, -88(%rbp)                  ## 1-byte Spill
	movss	-64(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %ebx
	movss	-60(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movl	%eax, %r12d
	movss	-56(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	movb	%al, -45(%rbp)                  ## 1-byte Spill
	movss	-52(%rbp), %xmm0                ## xmm0 = mem[0],zero,zero,zero
	callq	_integer_observation_probe
	stmxcsr	-44(%rbp)
	movb	%al, -47(%rbp)                  ## 1-byte Spill
	movl	-44(%rbp), %esi
	movzbl	%r14b, %r14d
	movzbl	%bl, %r9d
	movq	%r15, %rdi
	xorl	%edx, %edx
	movl	$1, %ecx
	movl	%r14d, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %r14b
	stmxcsr	-44(%rbp)
	movzbl	%r14b, %r14d
	movl	-44(%rbp), %esi
	movzbl	-46(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	%r12b, %r9d
	movq	%r15, %rdi
	movl	$1, %edx
	movl	$1, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	xorb	$1, %bl
	movzbl	%bl, %r12d
	stmxcsr	-44(%rbp)
	addl	%r14d, %r12d
	movl	-44(%rbp), %esi
	movzbl	-68(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-45(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r15, %rdi
	movl	$2, %edx
	xorl	%ecx, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	addl	%ebx, %r12d
	stmxcsr	-44(%rbp)
	addl	%r13d, %r12d
	movl	-44(%rbp), %esi
	movzbl	-88(%rbp), %ebx                 ## 1-byte Folded Reload
	movzbl	-47(%rbp), %r9d                 ## 1-byte Folded Reload
	movq	%r15, %rdi
	movl	$3, %edx
	xorl	%ecx, %ecx
	movl	%ebx, %r8d
	xorl	%eax, %eax
	callq	_printf
	movl	-92(%rbp), %eax                 ## 4-byte Reload
	movl	%eax, -100(%rbp)
	ldmxcsr	-100(%rbp)
	stmxcsr	-96(%rbp)
	xorl	%edx, %edx
	cmpl	%eax, -96(%rbp)
	sete	%dl
	leaq	L_.str.1(%rip), %rdi
	xorl	%r14d, %r14d
	addl	%ebx, %r12d
	setne	%r14b
	movl	%r12d, %esi
	xorl	%eax, %eax
	callq	_printf
	movl	%r14d, %eax
	addq	$72, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	retq
	.cfi_endproc
                                        ## -- End function
	.section	__TEXT,__cstring,cstring_literals
L_.str:                                 ## @.str
	.asciz	"mxcsr=%08x item=%u expected=%u production=%u candidate=%u\n"

L_.str.1:                               ## @.str.1
	.asciz	"Original production predicate failures=%u; original MXCSR restored=%u\n"

.subsections_via_symbols
