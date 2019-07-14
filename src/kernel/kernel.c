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

#include "kasm.h"
#include "kernel.h"
#include "kvar.h"
#include "memory.h"
#include <stdint.h>
#include <sys/syscall.h>

/* Global variable for kernel variables */
kvar_t *g_kvar;

/*
 * kvar_init
 */
int
kvar_init(void *buf, size_t size, size_t archsize)
{
    if ( sizeof(kvar_t) + archsize > size ) {
        return -1;
    }
    g_kvar = (kvar_t *)buf;
    kmemset(g_kvar, 0, sizeof(kvar_t) + archsize);
    g_kvar->arch = buf + sizeof(kvar_t);

    return 0;
}

/*
 * Convert a hexdecimal 4-bit value to an ascii code
 */
static int
hex(int c)
{
    if ( c > 9 ) {
        return 'a' + c - 10;
    } else {
        return '0' + c;
    }
}

/*
 * Print out the hexdecimal w-byte value
 */
static int
print_hex(volatile uint16_t *vbase, uint64_t val, int w)
{
    int i;
    uint16_t v;

    for ( i = 0; i < w * 2; i++ ) {
        v = (val >> (w * 8 - 4 - i * 4)) & 0xf;
        *(vbase + i) = 0x0700 | hex(v);
    }

    return i;
}

/*
 * System call handler
 */
void
sys_print_counter(int ln, uint64_t cnt)
{
    volatile uint16_t *base;

    base = (volatile uint16_t *)0xc00b8000;
    base += 80 * ln;
    print_hex(base, cnt, 8);
}
void
sys_hlt(void)
{
    __asm__ __volatile__ ("hlt");
}

/*
 * Initialize the kernel
 */
int
kernel_init(void)
{
    size_t sz;
    int nr;
    void **syscalls;
    int i;
    int ret;

    /* Allocate memory for system calls */
    sz = sizeof(void *) * SYS_MAXSYSCALL;
    nr = (sz + MEMORY_PAGESIZE - 1) >> MEMORY_PAGESIZE_SHIFT;
    syscalls = memory_alloc_pages(&g_kvar->mm, nr, MEMORY_ZONE_KERNEL, 0);
    if ( NULL == syscalls ) {
        return -1;
    }

    /* Setup systemcalls */
    for ( i = 0; i < SYS_MAXSYSCALL; i++ ) {
        syscalls[i] = NULL;
    }
    syscalls[SYS_exit] = sys_exit;
    syscalls[SYS_fork] = sys_fork;
    syscalls[SYS_execve] = sys_execve;
    syscalls[SYS_nanosleep] = sys_nanosleep;
    syscalls[SYS_initexec] = sys_initexec;
    syscalls[766] = sys_print_counter;
    syscalls[767] = sys_hlt;

    /* Set the table to the kernel variable */
    g_kvar->syscalls = syscalls;

    /* Create slab for event */
    ret = kmem_slab_create_cache("timer_event", sizeof(timer_event_t));
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * kprintf -- print a formatted string (up to 1023 characters)
 */
int
kprintf(const char *format, ...)
{
    int ret;
    va_list ap;
    char buf[2048];
    console_dev_t *dev;

    va_start(ap, format);
    ret = kvsnprintf(buf, 2048, format, ap);
    va_end(ap);

    /* Write the string to the console device(s) */
    dev = g_kvar->console.dev;
    while ( NULL != dev ) {
        dev->write(dev, buf, ret);
        dev = dev->next;
    }

    return 0;
}

/*
 * kstrcmp
 */
int
kstrcmp(const char *s1, const char *s2)
{
    size_t i;
    int diff;

    i = 0;
    while ( s1[i] != '\0' || s2[i] != '\0' ) {
        diff = s1[i] - s2[i];
        if ( diff ) {
            return diff;
        }
        i++;
    }

    return 0;
}

/*
 * kstrncmp
 */
int
kstrncmp(const char *s1, const char *s2, size_t n)
{
    size_t i;
    int diff;

    i = 0;
    while ( (s1[i] != '\0' || s2[i] != '\0') && i < n ) {
        diff = s1[i] - s2[i];
        if ( diff ) {
            return diff;
        }
        i++;
    }

    return 0;
}

/*
 * kstrcpy
 */
char *
kstrcpy(char *dst, const char *src)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = src[i];

    return dst;
}

/*
 * kstrncpy
 */
char *
kstrncpy(char *dst, const char *src, size_t n)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' && i < n ) {
        dst[i] = src[i];
        i++;
    }
    for ( ; i < n; i++ ) {
        dst[i] = '\0';
    }

    return dst;
}

/*
 * kstrlcpy
 */
size_t
kstrlcpy(char *dst, const char *src, size_t n)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' && i < n - 1 ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';

    while ( '\0' != src[i] ) {
        i++;
    }

    return i;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
