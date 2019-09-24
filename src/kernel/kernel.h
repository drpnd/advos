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

#ifndef _ADVOS_KERNEL_H
#define _ADVOS_KERNEL_H

#include <stdint.h>
#include <time.h>

/* Variable length argument support */
typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))

/* Define NULL */
#if !defined(NULL)
#define NULL    ((void *)0)
#endif

#ifndef __BYTE_ORDER__
#error "__BYTE_ORDER__ is not defined."
#endif

#define HZ      100

/* Maximum bytes in the path name */
#define PATH_MAX                1024

#define KSTACK_SIZE             8192
#define KSTACK_GUARD            16

/* Defined in arch/<architecture>/{arch.c,asm.S} */
void panic(const char *, ...);
void hlt(void);
void spin_lock(int *);
void spin_unlock(int *);
uint8_t in8(uint16_t);
uint16_t in16(uint16_t);
uint32_t in32(uint16_t);
void out8(uint16_t, uint8_t);
void out16(uint16_t, uint16_t);
void out32(uint16_t, uint32_t);

#define kassert(cond)        do {                                       \
        char buf[4096];                                                 \
        if ( !(cond) ) {                                                \
            ksnprintf(buf, 4096, "Assertion failed. %s:%d",             \
                      __FILE__, __LINE__);                              \
            panic(buf);                                                 \
        }                                                               \
    } while( 0 )

/* Defined in arch/x86_64/asm.S */
void * kmemset(void *, int, size_t);
int kmemcmp(void *, void *, size_t);
int kmemcpy(void *__restrict, const void *__restrict, size_t);
int kmemmove(void *, void *, size_t);

/* Defined in kernel.c */
int kvar_init(void *, size_t, size_t);
int kernel_init(void);
int kprintf(const char *, ...);
size_t kstrlen(const char *);
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, size_t);
char * kstrcpy(char *, const char *);
char * kstrncpy(char *, const char *, size_t);
size_t kstrlcpy(char *, const char *, size_t);

/* Defined in strfmt.c */
int kvsnprintf(char *, size_t, const char *, va_list);
int ksnprintf(char *, size_t, const char *, ...);

/* Defined in syscall.c */
void sys_exit(int);
pid_t sys_fork(void);
int sys_execve(const char *, char *const [], char *const []);
int sys_open(const char *, int, ...);
void * sys_mmap(void *, size_t, int, int, int, off_t);
int sys_nanosleep(const struct timespec *, struct timespec *);
int sys_initexec(const char *, char *const[], char *const[]);
int sys_driver(int, void *);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
