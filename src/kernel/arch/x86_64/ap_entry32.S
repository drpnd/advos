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

#define MSR_APIC_BASE           0x1b
#define APIC_LAPIC_ID           0x020

	.globl	ap_entry32

	.text
	.code32

/*
 * Entry point to the 32-bit protected mode for application processors
 */
ap_entry32:
	cli

	/* %cs is automatically set after the long jump operation */
	/* Setup other segment registers */
	movl	$AP_GDT_DATA64_SEL,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Enable PAE */
	movl	$0x40220,%eax	/* CR4[bit 5] = PAE */
	movl	%eax,%cr4	/* CR4[bit 9] = OSFXSR */
				/* CR4[bit 18] = OSXSAVE */

	/* Setup page table register */
	movl	$PGT_BOOT,%ebx
	movl	%ebx,%cr3

	/* Enable long mode */
	movl	$0xc0000080,%ecx	/* EFER MSR number */
	rdmsr			/* Read from 64bit-specific register */
	btsl	$8,%eax		/* LME bit = 1 */
	wrmsr			/* Write to 64bit-specific register */

	/* Activate page translation and long mode */
	movl	$0x80000001,%eax
	movl	%eax,%cr0
	jmp	hlt

	ljmpl	$AP_GDT_CODE32_SEL,$ap_reentry32

/* To jump to relocated address */
ap_reentry32:
	/* Get the local APIC ID */
	movl    $MSR_APIC_BASE,%ecx
	rdmsr
	andl	$0xfffffffffffff000,%eax	/* APIC Base */
	movl	APIC_LAPIC_ID(%eax),%eax
	shrl	$24,%eax
	movl	%eax,%edx
	incl	%edx
	shll	$12,%edx	/* One page per core */

	/* Check the Local APIC ID */
	cmpl	$MAX_PROCESSORS,%eax
	jge	hlt

	/* Setup page table register */
	movl	(APVAR_CR3),%ebx
	movl	%ebx,%cr3

	/* Setup stack */
	movl	(APVAR_SP),%ebx
	addl	%edx,%ebx
	movl	%ebx,%esp

	/* Load code64 descriptor */
	pushl	$AP_GDT_CODE64_SEL
	pushl	$ap_entry64
	lret

hlt:
1:
	hlt
	jmp	1b
