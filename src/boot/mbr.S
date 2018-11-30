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

#define VGA_TEXT_COLOR_80x25    0x03

	.globl	start

	/* Code segment */
	.text
	.code16

start:
	cld		/* Clear direction flag (inc di/si for str ops) */
	cli

	/* Setup the stack (below $start = 0x7c00) */
	xorw	%ax,%ax
	movw	%ax,%ss
	movw	$start,%sp
	/* Reset data segment registers */
	movw	%ax,%ds
	movw	%ax,%es

	sti

	/* Save drive information */
	movb	%dl,drive

	/* Set the video mode to 16-bit color text mode */
	movb	$VGA_TEXT_COLOR_80x25,%al
	movb	$0x00,%ah
	int	$0x10

	movw	$msg_welcome,%ax
	movw	%ax,%si
	call	putstr

halt:
	hlt
	jmp	halt


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

drive:
	.byte	0
msg_welcome:
	.asciz	"Welcome to advos"
