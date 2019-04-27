/*_
 * Copyright (c) 2019 Hirochika Asai <asai@jar.jp>
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

#include "bootinfo.h"

#define VGA_TEXT_COLOR_80x25    0x03

#define PXENV_SIGNATURE0        0x4e455850      /* PXEN */
#define PXENV_SIGNATURE1        0x2b56          /* V+ */
#define PXE_SIGNATURE           0x45585021      /* !PXE */

	.globl	pxeboot

	.text
	.code16

/* Entry point for the PXE boot
 *  %cs:%ip is expected to be 0x0000:0x7c000
 *  %es:%bx points to the PXENV+ structure
 *  %ss:%sp points to a valid stack (at least 1.5 KiB available)
 *  %ss:(%sp+4) contains the segment:offset of the !PXE structure
 */
pxeboot:
	cld		/* Clear direction flag (inc di/si for str ops) */
	cli

	/* Set %ds to %cs */
	movw	%cs,%ax
	movw	%ax,%ds

	/* Store the pointer to the PXENV+ structure */
	movw	%es,(pxenv_seg)
	movw	%bx,(pxenv_off)

	/* Get the segment:offset of the !PXE structure from %ss:(%sp+4) */
	movw	%ss,%ax
	movw	%ax,%es
	movw	%sp,%bx
	addw	$4,%bx
	movl	%es:(%bx),%eax
	movw	%ax,(pxe_off)
	shrl	$16,%eax
	movw	%ax,(pxe_seg)

	sti

	/* Set the video mode to 16-bit color text mode */
	movb	$VGA_TEXT_COLOR_80x25,%al
	movb	$0x00,%ah
	int	$0x10

	/* Verify the PXENV+ structure */
	movw	(pxenv_seg),%ax
	movw	%ax,%es
	movw	(pxenv_off),%bp
	movl	%es:(%bp),%eax
	/* Check the signature */
	cmpl	$PXENV_SIGNATURE0,%eax
	jne	error
	movw	%es:4(%bp),%ax
	cmpw	$PXENV_SIGNATURE1,%ax
	jne	error
	/* Check the checksum */
	movw	%bp,%si
	xorw	%cx,%cx
	movb	%es:8(%bp),%cl	/* Length */
	call	checksum
	testb	%al,%al
	jnz	error
	/* Check the version.  We support >= 2.1 (i.e., !PXE) */
	movw	%es:6(%bp),%ax
	cmpw	$0x21,%ax
	jl	error

	/* Parse the !PXE structure */
	movw	(pxe_seg),%ax
	movw	%ax,%es
	movw	(pxe_off),%bp
	/* Check the signature */
	movl	%es:(%bp),%eax
	cmpl	$PXE_SIGNATURE,%eax
	jne	error
	/* Get the rm_entry from the structure */
	movl	%es:16(%bp),%eax
	movl	%eax,(rm_entry)

	movw	$msg_welcome,%ax
	movw	%ax,%si
	call	putstr

halt:
	hlt
	jmp	halt

/* Error */
error:
	movw	$msg_error,%ax
	movw	%ax,%si
	call	putstr
	jmp	halt

/* Calculate the checksum
 *  %es:%si: Input buffer
 *  %cx: Input bytes
 *  Returns %al
 */
checksum:
	push	%cx
	push	%si
	xorb	%al,%al
1:
	addb	%es:(%si),%al
	incw	%si
	loop	1b
	pop	%si
	pop	%cx
	ret

/* PXE API call
 *  %ds:%di points to the input buffer
 *  %bx specifies the opcode
 */
pxeapi:
	pushw	%ds
	pushw	%di
	pushw	%bx
	lcall	*(rm_entry)
	popw	%bx
	popw	%di
	popw	%ds
	ret



/* Display a null-terminated string
 * Parameters: %ds:%(si)
 * Return value: %ax
 * Preserved Registers:
 */
putstr:
putstr.load:
	lodsb		/* Load %ds:(%si) to %al, then incl %si */
	testb	%al,%al
	jnz	putstr.putc
	ret
putstr.putc:
	call	putc
	jmp	putstr
putc:
	pushw	%bx
	movw	$0x7,%bx
	movb	$0xe,%ah
	int	$0x10
	popw	%bx
	ret

	/* Data segment */
	.data

/* PXENV+ */
pxenv_seg:
	.word	0
pxenv_off:
	.word	0

/* !PXE */
pxe_seg:
	.word	0
pxe_off:
	.word	0

/* Real mode PXE API entry point */
rm_entry:
	.long	0


msg_welcome:
	.asciz	"Welcome to advos"
/* Error message */
msg_error:
	.ascii	"Error on PXE boot. "
	.asciz	"Invalid PXE data structure or unsupport PXE version."
