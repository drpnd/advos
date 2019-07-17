/*_
 * Copyright (c) 2018-2019 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "const.h"

	.globl	kentry
	.globl	_hlt
	.globl	_sti
	.globl	_cli
	.globl	_pause
	.globl	_kmemset
	.globl	_kmemcmp
	.globl	_kmemcpy
	.globl	_kmemmove
	.globl	_cpuid
	.globl	_rdtsc
	.globl	_rdmsr
	.globl	_wrmsr
	.globl	_mfrd32
	.globl	_mfwr32
	.globl	_inb
	.globl	_inw
	.globl	_inl
	.globl	_outb
	.globl	_outw
	.globl	_outl
	.globl	_lgdt
	.globl	_sgdt
	.globl	_lidt
	.globl	_sidt
	.globl	_lldt
	.globl	_ltr
	.globl	_clts
	.globl	_fxsave64
	.globl	_fxrstor64
	.globl	_xsave64
	.globl	_xrstor64
	.globl	_sys_fork
	.globl	_task_replace
	.globl	_task_switch
	.globl	_task_restart
	.globl	_intr_null
	.globl	_intr_apic_loc_tmr
	.globl	_intr_crash
	.globl	_intr_irq1
	.globl	_asm_ioapic_map_intr
	.globl	_syscall_entry
	.globl	_syscall_setup
	.globl	_spin_lock
	.globl	_spin_unlock

	/* Code segment */
	.text
	.code64

/* Entry point */
kentry:
	cli
	call	_bsp_start
1:
	hlt
	jmp	1b

/* void hlt(void) */
_hlt:
	hlt
	ret

/* void hlt(void) */
_sti:
	sti
	ret

/* void cli(void) */
_cli:
	cli
	ret

/* void pause(void) */
_pause:
	pause
	ret

/* void * kmemset(void *b, int c, size_t len) */
_kmemset:
	pushq	%rdi
	movl	%esi,%eax	/* c */
	movq	%rdx,%rcx	/* len */
	cld			/* Ensure the DF cleared */
	rep	stosb		/* Set %al to (%rdi)-(%rdi+%rcx) */
	popq	%rdi
	movq	%rdi,%rax	/* Restore for the return value */
	ret

/* int kmemcmp(void *s1, void *s2, size_t n) */
_kmemcmp:
	xorq	%rax,%rax
	movq	%rdx,%rcx	/* n */
	cld			/* Ensure the DF cleared */
	repe	cmpsb		/* Compare byte at (%rsi) with byte at (%rdi) */
	jz	1f
	decq	%rdi		/* rollback one */
	decq	%rsi		/* rollback one */
	movb	(%rdi),%al	/* *s1 */
	subb	(%rsi),%al	/* *s1 - *s2 */
1:
	ret

/* int kmemcpy(void *__restrict dst, void *__restrict src, size_t n) */
_kmemcpy:
	movq	%rdi,%rax	/* Return value */
	movq	%rdx,%rcx	/* n */
	cld			/* Ensure the DF cleared */
	rep	movsb		/* Copy byte at (%rsi) to (%rdi) */
	ret

/* int kmemmove(void *dst, void *src, size_t len) */
_kmemmove:
	cmpq	%rdi,%rsi	/* Compare the addresses of dst/src */
	ja	_kmemcpy	/* If %rsi > %rdi, just execute memcpy() */
	/* Copy backwards */
	movq	%rdi,%rax	/* Return value */
	movq	%rdx,%rcx	/* n */
	std			/* Ensure the DF set */
	addq	%rcx,%rdi
	addq	%rcx,%rsi
	rep	movsb		/* Copy byte at (%rsi) to (%rdi), backwards */
	cld			/* Reset DF flag */
	ret

/* uint64_t cpuid(uint64_t rax, uint64_t *rbx, uint64_t *rcx, uint64_t *rdx) */
_cpuid:
	pushq	%rbx
	movq	%rdi,%rax
	movq	%rdx,%rdi
	movq	%rcx,%r8
	cpuid
	movq	%rbx,(%rsi)
	movq	%rcx,(%rdi)
	movq	%rdx,(%r8)
	popq	%rbx
	ret

/* uint64_t rdtsc(void) */
_rdtsc:
	xorq	%rax,%rax
	movq	%rax,%rdx
	rdtscp
	shlq	$32,%rdx
	addq	%rdx,%rax
	ret


/* uint64_t rdmsr(uint64_t reg) */
_rdmsr:
	movq	%rdi,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rdx,%rax
	ret

/* void wrmsr(uint64_t reg, uint64_t data) */
_wrmsr:
	movq	%rdi,%rcx
	movq	%rsi,%rax
	movq	%rax,%rdx
	shrq	$32,%rdx
	wrmsr
	ret

/* uint32_t mfrd32(uintptr_t addr) */
_mfrd32:
	mfence
	movl	(%rdi),%eax
	ret

/* void mfwr32(uintptr_t addr, uint32_t data) */
_mfwr32:
	mfence
	movl	%esi,(%rdi)
	ret

/* uint8_t inb(uint16_t port) */
_inb:
	movw	%di,%dx
	xorq	%rax,%rax
	inb	%dx,%al
	ret

/* uint16_t inw(uint16_t port) */
_inw:
	movw	%di,%dx
	xorq	%rax,%rax
	inw	%dx,%ax
	ret

/* uint32_t inl(uint16_t port) */
_inl:
	movw	%di,%dx
	xorq	%rax,%rax
	inl	%dx,%eax
	ret

/* void outb(uint16_t port, uint8_t value) */
_outb:
	movw	%di,%dx
	movb	%sil,%al
	outb	%al,%dx
	ret

/* void outw(uint16_t port, uint16_t value) */
_outw:
	movw	%di,%dx
	movw	%si,%ax
	outw	%ax,%dx
	ret

/* void outl(uint16_t port, uint32_t value) */
_outl:
	movw	%di,%dx
	movl	%esi,%eax
	outl	%eax,%dx
	ret

/* void lgdt(void *gdtr, uint64_t selector) */
_lgdt:
	lgdt	(%rdi)
	/* Reload GDT */
	pushq	%rsi
	movabs	$1f,%rax
	pushq	%rax	/* Just to do ret */
	lretq
1:
	/* Set data selector */
	movq	%rsi,%rax
	addq	$8,%rax
	movq	%rax,%ds
	movq	%rax,%es
	movq	%rax,%ss
	ret

/* void sgdt(void *gdtr) */
_sgdt:
	sgdt	(%rdi)
	ret

/* void lidt(void *idtr) */
_lidt:
	lidt	(%rdi)
	ret

/* void sidt(void *idtr) */
_sidt:
	sidt	(%rdi)
	ret

/* void lldt(uint16_t) */
_lldt:
	lldt	%di
	ret

/* void ltr(uint16_t) */
_ltr:
	ltr	%di
	ret

/* void clts(void) */
_clts:
	clts
	ret

/* void fxsave64(void *) */
_fxsave64:
	fxsave64	(%rdi)
	ret

/* void fxrstor64(void *) */
_fxrstor64:
	fxrstor64	(%rdi)
	ret

/* void xsave64(void *) */
_xsave64:
	xsave64	(%rdi)
	ret

/* void xrstor64(void *) */
_xrstor64:
	xrstor64	(%rdi)
	ret

/* pid_t sys_fork(void) */
_sys_fork:
	pushq	%rbp
	movq	%rsp,%rbp
	subq	$8,%rsp
	movq	%rsp,%rdi
	subq	$8,%rsp
	movq	%rsp,%rsi
	subq	$8,%rsp
	movq	%rsp,%rdx
	call	_sys_fork_c
	addq	$24,%rsp
	cmpl	$0,%eax
	jne	1f
	movq	-8(%rsp),%rdi
	movq	-16(%rsp),%rsi
	movq	-24(%rsp),%rdx
	call	sys_fork_restart
1:
	/* Return on error */
	leaveq
	retq

/* void sys_fork_restart(void *task, pid_t ret0, pid_t ret1) */
sys_fork_restart:
	movq	%rdx,%rax
	leaveq			/* Restore the stack (_sys_fork) */
	addq	$8,%rsp		/* Pop the return point (before _sys_fork) */
	pushq	%rdi
	pushq	%rsi
	/* Setup the stackframe for the forked task; N.B., preserved registers:
	   rbx, rsp, rbp, r12, r13, r14, r15  */
	movq	TASK_RP(%rdi),%rdx
	addq	$164,%rdx	/* Size of stackframe */
	movq	$GDT_RING0_DATA_SEL,%rcx
	movq	%rcx,-8(%rdx)	/* %ss */
	movq	%rbp,-16(%rdx)	/* %rsp */
	pushfq
	popq	%rcx
	movq	%rcx,-24(%rdx)	/* %rflags */
	movq	$GDT_RING0_CODE_SEL,%rcx
	movq	%rcx,-32(%rdx)	/* %cs */
	movabs	$1f,%rcx
	movq	%rcx,-40(%rdx)	/* %rip */
	movq	%rsi,-48(%rdx)	/* %rax (return value) */
	movq	-24(%rbp),%rcx
	movq	%rcx,-56(%rdx)	/* %rbx */
	movq	-8(%rbp),%rcx
	movq	%rcx,-64(%rdx)	/* %rcx */
	xorq	%rcx,%rcx
	movq	%rcx,-72(%rdx)	/* %rdx */
	movq	%rcx,-80(%rdx)	/* %r8 */
	movq	%rcx,-88(%rdx)	/* %r9 */
	movq	%rcx,-96(%rdx)	/* %r10 */
	movq	-16(%rbp),%rcx
	movq	%rcx,-104(%rdx)	/* %r11 */
	movq	%r12,-112(%rdx)	/* %r12 */
	movq	%r13,-120(%rdx)	/* %r13 */
	movq	%r14,-128(%rdx)	/* %r14 */
	movq	%r15,-136(%rdx)	/* %r15 */
	movq	-40(%rbp),%rcx
	movq	%rcx,-144(%rdx)	/* %rsi */
	movq	-32(%rbp),%rcx
	movq	%rcx,-152(%rdx)	/* %rdi */
	movq	0(%rbp),%rcx
	movq	%rcx,-160(%rdx)	/* %rbp */
	movw	$(GDT_RING3_CODE64_SEL+3),%cx
	movw	%cx,-162(%rdx)	/* %fs */
	movw	%cx,-164(%rdx)	/* %gs */
	/* Restore */
	popq	%rsi		/* _sys_fork */
	popq	%rdi		/* _sys_fork */
	popq	%rbx		/* _syscall_entry */
	popq	%r11		/* _syscall_entry */
	popq	%rcx		/* _syscall_entry */
	movq	%rbp,%rsp
	popq	%rbp
	sysretq
1:
	popq	%rbp
	sysretq

/* Replace the currently running task (for execve) */
_task_replace:
	/* Get the APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	0x020(%rdx),%eax	/* Local APIC ID */
	shrl	$24,%eax
	shlq	$CPU_DATA_SIZE_SHIFT,%rax
	movq	%rax,%rcx
	/* Task base address (struct arch_task *) */
	movq	$CPU_TASK_BASE,%rbp
	addq	%rcx,%rbp
	/* Get the restart point from the current task */
	movq	TASK_RP(%rdi),%rsp	/* task->rp */
	/* Change page table */
	movq	TASK_CR3(%rdi),%rax
	movq	%rax,%cr3
	/* Setup sp0 in TSS */
	movq	TASK_SP0(%rdi),%rdx	/* task->sp0 */
	movq	$CPU_TSS_BASE,%rax	/* TSS */
	addq	%rcx,%rax
	movq	%rdx,TSS_SP0(%rax)
	/* Pop all registers from the stackframe */
	popw	%gs
	popw	%fs
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
	iretq

/* Switch the task */
_task_switch:
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rbx
	pushq	%rcx
	pushq	%rdx
	pushq	%rdi
	pushq	%rsi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	/* Get the APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	0x020(%rdx),%eax	/* Local APIC ID */
	shrl	$24,%eax
	shlq	$CPU_DATA_SIZE_SHIFT,%rax
	movq	%rax,%r15
	/* Task base address (struct arch_cpu_data *) */
	movq	$CPU_TASK_BASE,%rbp
	addq	%r15,%rbp

	/* If the current task is not set, then do nothing */
	cmpq	$0,TASK_CUR(%rbp)
	jz	3f

	/* Save the stackframe of the current task */
	movq	%rsp,%rdx
	movabs	$3f,%rax	/* %rip */
	pushq	$GDT_RING0_DATA_SEL	/* %ss */
	pushq	%rdx		/* %rsp */
	pushfq			/* %rflags */
	pushq	$GDT_RING0_CODE_SEL	/* %cs */
	pushq	%rax		/* %rip */
	pushq	$0		/* %rax */
	pushq	%rbx
	pushq	%rcx
	pushq	$0		/* %rdx */
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushq	%rsi
	pushq	%rdi
	pushq	%rbp
	pushw	%fs
	pushw	%gs

	/* If the next task is not scheduled, run the idle task */
	cmpq	$0,TASK_NEXT(%rbp)
	jnz	1f
	movq	TASK_IDLE(%rbp),%rbx	/* idle task */
	jmp	2f
1:
	movq	TASK_NEXT(%rbp),%rbx	/* next task */
2:
	/* Save the stack pointer to the restart pointer */
	movq	TASK_CUR(%rbp),%rax
	movq	%rsp,TASK_RP(%rax)
	/* Set TS */
	movq	%cr0,%rax
	btsq	$3,%rax
	movq	%rax,%cr0
	/* Notify that the current task is switched (to the kernel) */
	movq	TASK_CUR(%rbp),%rdi
	movq	%rbx,%rsi
	/* to implement */

	/* Task switch (set the stack frame of the new task) */
	movq	%rbx,TASK_CUR(%rbp)	/* cur_task */
	movq	TASK_RP(%rbx),%rsp
	movq	$0,TASK_NEXT(%rbp)	/* next_task */
	/* Change page table */
	movq	TASK_CR3(%rbx),%rdx
	movq	%rdx,%cr3
	/* Setup sp0 in TSS */
	movq	TASK_SP0(%rbx),%rdx	/* cur_task->sp0 */
	movq	$CPU_TSS_BASE,%rax	/* TSS */
	addq	%r15,%rax
	movq	%rdx,TSS_SP0(%rax)

	/* Pop all registers from the stackframe */
	popw	%gs
	popw	%fs
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
	iretq
3:
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rsi
	popq	%rdi
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rbp
	retq

/* Restart a task */
_task_restart:
	/* Get the APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	0x020(%rdx),%eax	/* Local APIC ID */
	shrl	$24,%eax
	shlq	$CPU_DATA_SIZE_SHIFT,%rax
	movq	%rax,%r15
	/* Task base address (struct arch_cpu_data *) */
	movq	$CPU_TASK_BASE,%rbp
	addq	%r15,%rbp
	/* If the next task is not scheduled, immediately restart this task. */
	cmpq	$0,TASK_NEXT(%rbp)	/* next_task */
	jz	2f
	movq	TASK_NEXT(%rbp),%rax
	/* If the current task is null, then do not need to save anything. */
	cmpq	$0,TASK_CUR(%rbp)	/* cur_task */
	jz	1f
	/* Save the stack pointer (restart point) */
	movq	TASK_CUR(%rbp),%rax
	movq	%rsp,TASK_RP(%rax)	/* cur_task->rp */
	/* Set TS */
	movq	%cr0,%rax
	btsq	$3,%rax
	movq	%rax,%cr0
1:
	/* Notify that the current task is switched (to the kernel) */
	movq	TASK_CUR(%rbp),%rdi
	movq	TASK_NEXT(%rbp),%rsi
	/* to implement */
	/* Task switch (set the stack frame of the new task) */
	movq	TASK_NEXT(%rbp),%rax	/* next_task */
	movq	%rax,TASK_CUR(%rbp)	/* cur_task */
	movq	TASK_RP(%rax),%rsp	/* next_task->rp */
	movq	$0,TASK_NEXT(%rbp)	/* next_task */
	/* Change page table */
	movq	TASK_CR3(%rax),%rdx
	movq	%rdx,%cr3
	/* Setup sp0 in TSS */
	movq	TASK_CUR(%rbp),%rax	/* cur_task */
	movq	TASK_SP0(%rax),%rdx	/* cur_task->sp0 */
	movq	$CPU_TSS_BASE,%rax	/* TSS */
	addq	%r15,%rax
	movq	%rdx,TSS_SP0(%rax)
2:
	/* Pop all registers from the stackframe */
	popw	%gs
	popw	%fs
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
	iretq


/* Null interrupt handler (do nothing) */
_intr_null:
	iretq

/* Timer interrupt of Local APIC */
_intr_apic_loc_tmr:
	/* Push all registers to the stackframe */
	pushq	%rax
	pushq	%rbx
	pushq	%rcx
	pushq	%rdx
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushq	%rsi
	pushq	%rdi
	pushq	%rbp
	pushw	%fs
	pushw	%gs
	/* Call a function */
	call	_ksignal_clock
	/* APIC EOI */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP */
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx        /* APIC Base */
	movl	$0,0x0b0(%rdx)       /* EOI */
	jmp	_task_restart

/* Crash interrupt (hlt forever) */
_intr_crash:
1:
	hlt
	jmp	1b

.macro	intr_exception_prolog
	/* Save registers*/
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rax
	pushq	%rdi
	pushq	%rsi
	pushq	%rdx
	pushq	%rcx
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%rbx
.endm
.macro	intr_exception_epilog
	/* Restore registers */
	popq	%rbx
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rcx
	popq	%rdx
	popq	%rsi
	popq	%rdi
	popq	%rax
	popq	%rbp
.endm

/* Macro for generic exception handlers */
.macro  intr_exception_generic name vec
	.globl	_intr_\name
_intr_\name:
	intr_exception_prolog
	/* Call isr_exception_generic() */
	movq	$\vec,%rdi
	movq	8(%rbp),%rsi	/* rip */
	movq	16(%rbp),%rdx	/* cs */
	movq	24(%rbp),%rcx	/* rflags */
	movq	32(%rbp),%r8	/* rsp */
	/* 40(%rbp): ss */
	call	_isr_exception
	intr_exception_epilog
	iretq
.endm

/* Macro for generic exception handlers */
.macro  intr_exception_werror name vec
	.globl	_intr_\name
_intr_\name:
	intr_exception_prolog
	/* Call isr_exception_werror() */
	movq	$\vec,%rdi
	movq	8(%rbp),%rsi	/* error code */
	movq	16(%rbp),%rdx	/* rip */
	movq	24(%rbp),%rcx	/* cs */
	movq	32(%rbp),%r8	/* rflags */
	movq	40(%rbp),%r9	/* rsp */
	/* 48(%rbp): ss */
	call	_isr_exception_werror
	intr_exception_epilog
	addq	$0x8,%rsp
	iretq
.endm

/* Divide-by-zero Error (#DE) */
	intr_exception_generic de 0x00
/* Debug fault or trap */
	intr_exception_generic db 0x01
/* Non-maskable Interrupt */
	intr_exception_generic nmi 0x02
/* Breakpoint (#BP) */
	intr_exception_generic bp 0x03
/* Overflow (#OF) */
	intr_exception_generic of 0x04
/* Bound Range Exceeded (#BR) */
	intr_exception_generic br 0x05
/* Invalid Opcode (#UD) */
	intr_exception_generic ud 0x06
/* Device Not Available (#NM) */
	.globl	_intr_nm
_intr_nm:
	intr_exception_prolog
	call	_isr_device_not_available
	movq	8(%rbp),%rdi	/* rip */
	movq	16(%rbp),%rsi	/* cs */
	movq	24(%rbp),%rdx	/* rflags */
	movq	32(%rbp),%rcx	/* rsp */
	intr_exception_epilog
	iretq
/* Double Fault (#DF) */
	intr_exception_werror df 0x08
/* Coprocessor Segment Overrun */
	intr_exception_generic cso 0x09
/* Invalid TSS (#TS) */
	intr_exception_werror ts 0x0a
/* Segment Not Present (#NP) */
	intr_exception_werror np 0x0b
/* Stack-Segment Fault (#ss) */
	intr_exception_werror ss 0x0c
/* General Protection Fault (#GP) */
	intr_exception_werror gp 0x0d
/* Page Fault (#PF) */
	.globl	_intr_pf
_intr_pf:
	intr_exception_prolog
	/* Call isr_page_fault() */
	movq	%cr2,%rdi	/* virtual address */
	movq	8(%rbp),%rsi	/* error code */
	movq	16(%rbp),%rdx	/* rip */
	movq	24(%rbp),%rcx	/* cs */
	movq	32(%rbp),%r8	/* rflags */
	movq	40(%rbp),%r9	/* rsp */
	/* 48(%rbp): ss */
	call	_isr_page_fault
	intr_exception_epilog
	addq	$0x8,%rsp
	iretq
/* x87 Floating-Point Exception (#MF) */
	intr_exception_generic mf 0x10
/* Alignment Check (#AC) */
	intr_exception_werror ac 0x11
/* Machine Check (#MC) */
	intr_exception_generic mc 0x12
/* SIMD Floating-Point Exception (#XM/#XF) */
	intr_exception_generic xm 0x13
/* Virtualization Exception (#VE) */
	intr_exception_generic ve 0x14
/* Security Exception (#SX) */
	intr_exception_werror sx 0x1e

/* Interrupt handler for IRQ1 */
_intr_irq1:
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	/* Print the key to the bottom right */
	xorl	%eax,%eax
	inb	$0x60,%al
	testb	$0x80,%al
	jnz	1f		/* Key released */
	movl	$keymap_base,%edx	/* Use base keymap */
	addl	%eax,%edx
	movb	(%edx),%al
	movb	$0x07,%ah
	movabs	$0xc00b8000+80*25*2-2,%rcx
	movw	%ax,(%rcx)
1:
	/* APIC EOI */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP */
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx        /* APIC Base */
	movl	$0,0x0b0(%rdx)       /* EOI */
	popq	%rdx
	popq	%rcx
	popq	%rax
	iretq

	/* Data section */
	.data

/* Entry point to the syscall */
_syscall_entry:
	/* N.B., rip and rflags are stored in rcx and r11, respectively. */
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rcx
	pushq	%r11
	pushq	%rbx

	/* Check the max number of the syscall table */
	movabs	$syscall_nr,%rbx
	cmpq	(%rbx),%rax
	jge	1f

	/* Lookup the system call table and call the corresponding to %rax */
	movabs	$syscall_table,%rcx
	movq	(%rcx),%rbx
	shlq	$3,%rax		/* 8-byte per pointer */
	addq	%rax,%rbx
	cmpq	$0,(%rbx)
	je	1f
	movq	%r10,%rcx	/* Replace the 4th argument with %r10 */
	callq	*(%rbx)
1:
	popq	%rbx
	popq	%r11
	popq	%rcx
	popq	%rbp
	sysretq

/* void syscall_setup(void *, uint64_t) */
_syscall_setup:
	pushq	%rbx
	movabs	$syscall_table,%rbx
	movq	%rdi,(%rbx)
	movabs	$syscall_nr,%rbx
	movq	%rsi,(%rbx)
	popq	%rbx
	ret

/* void spin_lock(int *) */
_spin_lock:
	xorl	%ecx,%ecx
	incl	%ecx
1:
	xorl	%eax,%eax
	lock cmpxchgl	%ecx,(%rdi)
	jnz	1b
	ret

/* void spin_unlock(int *) */
_spin_unlock:
	xorl	%eax,%eax
	lock xchgl	(%rdi),%eax
	ret

	.data
	.align	16

/* Data for system calls */
syscall_table:
	.quad	0
syscall_nr:
	.quad	0

/* Keyboard mapping (to be removed) */
keymap_base:
	.ascii	"  1234567890-=  qwertyuiop[]  as"
	.ascii	"dfghjkl;'` \\zxcvbnm,./          "
	.ascii	"                                "
	.ascii	"                                "
