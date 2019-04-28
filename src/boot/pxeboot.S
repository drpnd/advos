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

#define VGA_TEXT_COLOR_80x25	0x03
/* Memory map */
#define MME_SIZE		24		/* Memory map entry size */
#define MME_SIGN		0x534d4150	/* MME signature ("SMAP")  */

/* Kernel */
#define KERNEL_OFF		0x00010000

#define TFTP_PORT		69
#define TFTP_PACKETSIZE		512

#define PXENV_SIGNATURE0	0x4e455850	/* PXEN */
#define PXENV_SIGNATURE1	0x2b56		/* V+ */
#define PXE_SIGNATURE		0x45585021	/* !PXE */

#define PXEAPI_OPCODE_TFTP_OPEN		0x0020
#define PXEAPI_OPCODE_TFTP_CLOSE	0x0021
#define PXEAPI_OPCODE_TFTP_READ		0x0022
#define PXEAPI_OPCODE_CACHED_INFO	0x0071

#define CACHED_INFO_CIP			12	/* Client IP address*/
#define CACHED_INFO_YIP			16	/* Your IP address */
#define CACHED_INFO_SIP			20	/* Server IP address */
#define CACHED_INFO_GIP			24	/* Relay agent IP address */
#define CACHED_INFO_MAC			28	/* MAC address */
#define CACHED_INFO_SER			40	/* Server host name (64) */
#define CACHED_INFO_BOO			104	/* Boot file name (128) */

#define PXEAPI_GENERIC_BUFFER_SIZE	192

#define PXEAPI_TFTP_OPEN_STATUS		0
#define PXEAPI_TFTP_OPEN_SIP		2
#define PXEAPI_TFTP_OPEN_GIP		6
#define PXEAPI_TFTP_OPEN_FILENAME	10
#define PXEAPI_TFTP_OPEN_PORT		138
#define PXEAPI_TFTP_OPEN_PACKETSIZE	140
#define PXEAPI_TFTP_OPEN_SIZE		142

#define PXEAPI_TFTP_READ_STATUS		0
#define PXEAPI_TFTP_READ_PACKET_NUM	2
#define PXEAPI_TFTP_READ_BUFFER_SIZE	4
#define PXEAPI_TFTP_READ_BUFFER_OFF	6
#define PXEAPI_TFTP_READ_BUFFER_SEG	8
#define PXEAPI_TFTP_READ_SIZE		10

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
	jne	error_pxe
	movw	%es:4(%bp),%ax
	cmpw	$PXENV_SIGNATURE1,%ax
	jne	error_pxe
	/* Check the checksum */
	movw	%bp,%si
	xorw	%cx,%cx
	movb	%es:8(%bp),%cl	/* Length */
	call	checksum
	testb	%al,%al
	jnz	error_pxe
	/* Check the version.  We support >= 2.1 (i.e., !PXE) */
	movw	%es:6(%bp),%ax
	cmpw	$0x21,%ax
	jl	error_pxe

	/* Parse the !PXE structure */
	movw	(pxe_seg),%ax
	movw	%ax,%es
	movw	(pxe_off),%bp
	/* Check the signature */
	movl	%es:(%bp),%eax
	cmpl	$PXE_SIGNATURE,%eax
	jne	error_pxe
	/* Get the rm_entry from the structure */
	movl	%es:16(%bp),%eax
	movl	%eax,(rm_entry)

	/* Get the cached information, e.g., buffer size */
	subw	$12,%sp
	movw	%sp,%di
	movw	%ss,%ax
	movw	%ax,%es
	call	pxeapi_cached_info
	movw	%es:4(%di),%ax	/* Buffer size */
	movw	%ax,(buffer_size)
	movw	%es:6(%di),%ax	/* Buffer offset */
	movw	%ax,(buffer_off)
	movw	%es:8(%di),%ax	/* Buffer segment */
	movw	%ax,(buffer_seg)
	movw	%es:10(%di),%ax	/* Buffer limit */
	movw	%ax,(buffer_limit)
	addw	$12,%sp
	testw	%ax,%ax
	jnz	error_pxe

	/* Enable A20 */
	call	enable_a20

	/* Load memory map */
	movw	$BI_MM_TABLE_ADDR,%di
	movw	$(BI_MM_SIZE/MME_SIZE),%dx
	call	load_mm
	jc	1f
	movw	%ax,(BI_MM_NENT_ADDR)
	jmp	2f
1:
	movw	$msg_error_mm,%si
	call	putstr
	jmp	halt
2:

	/* Print loading kernel message */
	movw	$msg_loading,%si
	call	putstr
	movw	$filename_kernel,%si
	call	putstr
	movw	$crlf,%si
	call	putstr

	/* Load kernel */
	movw	$filename_kernel,%si
	movl	$KERNEL_OFF,%edi
	call	load_tftp_file
	testw	%ax,%ax
	jnz	error_tftp

	movw	$msg_welcome,%si
	call	putstr

halt:
	hlt
	jmp	halt

/* Enable A20 address line */
enable_a20:
	cli
	pushw	%ax
	pushw	%cx
	xorw	%cx,%cx
1:
	incw	%cx		/* Try until %cx overflows (2^16 times) */
	jz	3f		/*  Failed to enable a20 */
	inb	$0x64,%al	/* Get status from the keyboard controller */
	testb	$0x2,%al	/* Busy? */
	jnz	1b		/* Yes, busy.  Then try again */
	movb	$0xd1,%al	/* Command: Write output port (0x60 to P2) */
	outb	%al,$0x64	/* Write the command to the command register */
2:
	inb	$0x64,%al	/* Get status from the keyboard controller */
	testb	$0x2,%al	/* Busy? */
	jnz	2b		/* Yes, busy.  Then try again */
	movb	$0xdf,%al	/* Command: Enable A20 */
	outb	%al,$0x60	/* Write to P2 via 0x60 output port */
3:
	popw	%cx
	popw	%ax
	sti
	ret

/*
 * Load memory map entries from BIOS
 * Parameters:
 *   %es:%di: destination
 *   %dx: the maximum number of entries
 * Return values:
 *   %ax: the number of entries
 *   CF: set if an error occurs
 */
load_mm:
	/* Save registers */
	pushl	%ebx
	pushl	%ecx
	pushw	%di
	pushw	%bp

	xorl	%ebx,%ebx	/* Continuation value for int 0x15 */
	xorw	%bp,%bp		/* Counter */
load_mm.1:
	movl	$0x1,%ecx	/* Write 1 once */
	movl	%ecx,%es:20(%di)	/*  to check support ACPI >=3.x? */
	/* Read the system address map */
	movl	$0xe820,%eax
	movl	$MME_SIGN,%edx	/* Set the signature */
	movl	$MME_SIZE,%ecx	/* Set the buffer size */
	int	$0x15		/* Query system address map */
	jc	load_mm.error	/* Error */
	cmpl	$MME_SIGN,%eax	/* Check the signature SMAP */
	jne	load_mm.error

	cmpl	$24,%ecx	/* Check the read buffer size */
	je	load_mm.2	/*  %ecx==24 */
	cmpl	$20,%ecx
	je	load_mm.3	/*  %ecx==20 */
	jmp	load_mm.error	/* Error otherwise */
load_mm.2:
	/* 24-byte entry */
	testl	$0x1,%es:20(%di)	/* 1 must be present in the attribute */
	jz	load_mm.error	/*  error if it's overwritten */
load_mm.3:
	/* 20-byte entry or 24-byte entry coming from above */
	incw	%bp		/* Increment the number of entries */
	testl	%ebx,%ebx	/* %ebx=0: No remaining info */
	jz	load_mm.done	/* jz/je */
	cmpw	%bp,%dx
	je	load_mm.error	/* Exceeded the maximum number to read */
load_mm.4:
	addw	$MME_SIZE,%di	/* Next entry */
	jmp	load_mm.1	/* Load remaining entries */
load_mm.error:
	stc			/* Set CF */
load_mm.done:
	movw	%bp,%ax		/* Return value */
	popw	%bp
	popw	%di
	popl	%ecx
	popl	%ebx
	ret

/* Get cached info
 *  %es:%di: Input buffer
 *  Returns the status in %ax
 */
pxeapi_cached_info:
	pushw	%bx
	movw	$0,%es:0(%di)	/* Status */
	movw	$2,%es:2(%di)	/* Packet type */
	movw	$0,%es:4(%di)	/* Buffer size */
	movw	$0,%es:6(%di)	/* Buffer offset */
	movw	$0,%es:8(%di)	/* Buffer segment */
	movw	$0,%es:10(%di)	/* Buffer limit */
	movw	$PXEAPI_OPCODE_CACHED_INFO,%bx
	call	pxeapi
	movw	%es:0(%di),%ax	/* Status */
	popw	%bx
	ret

/* Load the content of a file specified by a null-terminated string starting
 * from %ds:(%si) to (%edi). */
load_tftp_file:
	pushw	%bx
	pushw	%cx
	pushl	%edx
	pushw	%es
	pushw	%fs
	pushw	%bp
	pushw	%si
	pushl	%edi

	/* Allocate the stack for the command buffer */
	subw	$PXEAPI_GENERIC_BUFFER_SIZE,%sp
	movw	%sp,%bp
	movw	%ss,%bx
	movw	%bx,%es
	/* Reset the data structure first */
	movw	%bp,%di
	movw	$PXEAPI_GENERIC_BUFFER_SIZE,%cx
	xorb	%al,%al
	rep	stosb
	/* Set the cached info to build a bootph structure */
	movw	(buffer_size),%cx
	movw	(buffer_off),%bx
	movw	(buffer_seg),%dx

	/* Prepare the input buffer to open a file */
	movw	%dx,%fs
	movl	%fs:CACHED_INFO_SIP(%bx),%eax
	movl	%eax,%es:PXEAPI_TFTP_OPEN_SIP(%bp)	/* Server */
	movl	%fs:CACHED_INFO_GIP(%bx),%eax
	movl	%eax,%es:PXEAPI_TFTP_OPEN_GIP(%bp)	/* Relay agen */
	movw	$TFTP_PORT,%ax
	xchgb	%al,%ah
	movw	%ax,%es:PXEAPI_TFTP_OPEN_PORT(%bp)	/* TFTP port (69) */
	movw	$TFTP_PACKETSIZE,%ax
	movw	%ax,%es:PXEAPI_TFTP_OPEN_PACKETSIZE(%bp)
	/* Copy the null-terminated string */
	leaw	PXEAPI_TFTP_OPEN_FILENAME(%bp),%di
1:
	movb	(%si),%al
	testb	%al,%al
	jz	2f
	movb	%al,%es:(%di)
	incw	%si
	incw	%di
	jmp	1b
2:

	/* Open the TFTP session */
	movw	%bp,%di
	movw	$PXEAPI_OPCODE_TFTP_OPEN,%bx
	call	pxeapi
	movw	%es:PXEAPI_TFTP_OPEN_STATUS(%bp),%ax
	testw	%ax,%ax
	jnz	load_tftp_file.error

	/* Reset the data structure first */
	movw	%bp,%di
	movw	$PXEAPI_GENERIC_BUFFER_SIZE,%cx
	xorb	%al,%al
	rep	stosb
	/* Read */
	movw	%bp,%di
	movw	$PXEAPI_OPCODE_TFTP_READ,%bx
	movl	%es:PXEAPI_GENERIC_BUFFER_SIZE(%bp),%edx	/* arg %edi */
	xorw	%cx,%cx						/* packet # */
1:
	movw	%dx,%ax
	andw	$0xf,%ax
	movw	%ax,%es:PXEAPI_TFTP_READ_BUFFER_OFF(%bp)
	movl	%edx,%eax
	shrl	$4,%eax
	movw	%ax,%es:PXEAPI_TFTP_READ_BUFFER_SEG(%bp)
	call	pxeapi
	movw	%es:PXEAPI_TFTP_READ_STATUS(%bp),%ax
	testw	%ax,%ax
	jnz	load_tftp_file.error
	/* Check the packet number */
	incw	%cx
	movw	$0xffff,%ax			/* pseudo error code */
	cmpw	%es:PXEAPI_TFTP_READ_PACKET_NUM(%bp),%cx
	jne	load_tftp_file.error
	/* Check the size */
	xorl	%eax,%eax
	movw	%es:PXEAPI_TFTP_READ_BUFFER_SIZE(%bp),%ax
	addl	%eax,%edx
	cmpl	$0x100000,%edx
	jge	load_tftp_file.error
	/* Check if it is the last packet */
	cmpw	$TFTP_PACKETSIZE,%ax
	jge	1b

	/* Reset the data structure first */
	movw	%bp,%di
	movw	$PXEAPI_GENERIC_BUFFER_SIZE,%cx
	xorb	%al,%al
	rep	stosb
	/* Close */
	movw	%bp,%di
	movw	$PXEAPI_OPCODE_TFTP_CLOSE,%bx
	call	pxeapi
	movw	%es:PXEAPI_TFTP_OPEN_STATUS(%bp),%ax
	testw	%ax,%ax
	jnz	load_tftp_file.error

load_tftp_file.success:
	xorw	%ax,%ax
load_tftp_file.error:
	addw	$PXEAPI_GENERIC_BUFFER_SIZE,%sp
	popl	%edi
	popw	%si
	popw	%bp
	popw	%fs
	popw	%es
	popl	%edx
	popw	%cx
	popw	%bx
	ret

/* Error */
error_pxe:
	movw	$msg_error_pxe,%ax
	movw	%ax,%si
	call	putstr
	jmp	halt
error_tftp:
	movw	$msg_error_tftp,%ax
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
 *  %es:%di points to the input buffer
 *  %bx specifies the opcode
 */
pxeapi:
	pushw	%es
	pushw	%di
	pushw	%bx
	lcall	*(rm_entry)
	popw	%bx
	popw	%di
	popw	%es
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

/* Buffer size and offset */
buffer_size:
	.word	0
buffer_off:
	.word	0
buffer_seg:
	.word	0
buffer_limit:
	.word	0

msg_welcome:
	.asciz	"Welcome to advos"
msg_loading:
	.asciz	"Loading "
crlf:
	.asciz	"\r\n"
/* Error message */
msg_error_pxe:
	.ascii	"Error on PXE boot.\r\n"
	.asciz	"Invalid PXE data structure or unsupport PXE version."
msg_error_tftp:
	.ascii	"Error on PXE boot.\r\n"
	.asciz	"Cannot load the kernel."
msg_error_mm:
	.asciz	"Error on loading the system memory map."

/* TFTP filename */
filename_kernel:
	.asciz	"kernel"
