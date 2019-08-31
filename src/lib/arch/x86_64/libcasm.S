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

	.text
	.code64
	.globl	_syscall
	.globl	_memcpy
	.globl	_memmove
	.globl	_memset

/* int syscall(arg0, ..., arg5) */
_syscall:
	pushq	%rbp
	movq	%rdi,%rax
	movq	%rsi,%rdi
	movq	%rdx,%rsi
	movq	%rcx,%rdx
	movq 	%r8,%r10
	movq	%r9,%r8
	movq	-8(%rsp),%r9
	syscall
	popq	%rbp
	ret

/* void * memcpy(void *__restrict dst, const void *__restrict src, size_t n) */
_memcpy:
	movq	%rdi,%rax	/* Return value */
	movq	%rdx,%rcx	/* n */
	cld			/* Ensure the DF cleared */
	rep	movsb		/* Copy byte at (%rsi) to (%rdi) */
	ret

/* int memmove(void *dst, void *src, size_t len) */
_memmove:
	cmpq	%rdi,%rsi	/* Compare the addresses of dst/src */
	ja	_memcpy		/* If %rsi > %rdi, just execute memcpy() */
	/* Copy backwards */
	movq	%rdi,%rax	/* Return value */
	movq	%rdx,%rcx       /* n */
	std			/* Ensure the DF set */
	addq	%rcx,%rdi
	addq	%rcx,%rsi
	rep	movsb		/* Copy byte at (%rsi) to (%rdi), backwards */
	cld			/* Reset DF flag */
	ret

/* void * memset(void *b, int c, size_t len) */
_memset:
	pushq	%rdi
	movl	%esi,%eax	/* c */
	movq	%rdx,%rcx	/* len */
	cld			/* Ensure the DF cleared */
	rep	stosb		/* Set %al to (%rdi)-(%rdi+%rcx) */
	popq	%rdi
	movq	%rdi,%rax	/* Restore for the return value */
	ret
