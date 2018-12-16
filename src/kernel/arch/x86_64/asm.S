/*_
 * Copyright (c) 2018 Hirochika Asai <asai@jar.jp>
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
	.globl	_pause
	.globl	_kmemset
	.globl	_kmemcmp
	.globl	_kmemcpy
	.globl	_cpuid
	.globl	_rdtsc
	.globl	_rdmsr
	.globl	_wrmsr
	.globl	_mfrd32
	.globl	_mfwr32
	.globl	_inl
	.globl	_outw
	.globl	_intr_null
	.globl	_intr_crash

	.set	APIC_EOI,0x0b0

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

/* uint32_t inl(uint16_t port) */
_inl:
	movw	%di,%dx
	xorq	%rax,%rax
	inl	%dx,%eax
	ret

/* void outw(uint16_t port, uint16_t value) */
_outw:
	movw	%di,%dx
	movw	%si,%ax
	outw	%ax,%dx
	ret

/* Null interrupt handler (do nothing) */
_intr_null:
	iretq
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	/* APIC EOI */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP */
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	movl	$0,APIC_EOI(%rdx)	/* EOI */
	popq	%rdx
	popq	%rcx
	popq	%rax
	iretq

/* Crash interrupt (hang here) */
_intr_crash:
1:
	hlt
	jmp	1b
