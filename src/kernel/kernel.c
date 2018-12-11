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

#include "../boot/bootinfo.h"
#include "kasm.h"
#include "memory.h"
#include <stdint.h>

/* To be architecture independent */
#include "arch/x86_64/kvar.h"

#define set_cr3(cr3)    __asm__ __volatile__ ("movq %%rax,%%cr3" :: "a"((cr3)))

/*
 * System memory map entry
 */
typedef struct {
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t attr;
} __attribute__ ((packed)) sysaddrmap_entry_t;

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
print_hex(uint16_t *vbase, uint64_t val, int w)
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
 * Print out the specified string
 */
static int
print_str(uint16_t *vbase, char *s)
{
    int offset;

    offset = 0;
    while ( s[offset] ) {
        *(vbase + offset) = 0x0700 | s[offset];
        offset++;
    }

    return offset;
}

/*
 * Setup the kernel page table
 */
static void
setup_kernel_pgt(void)
{
    int i;
    uint64_t base;
    uint64_t *pml4;
    uint64_t *e;

    /* Setup the kernel base page table */
    base = 0x00069000ULL;
    pml4 = (uint64_t *)base;

    /* Zero (memset() may work) */
    for ( i = 0; i < 512 * 5; i++ ) {
        *(pml4 + i) = 0;
    }
    /* First 512 GiB */
    *pml4 = (base + 0x1000) | 0x007;

    /* PDPT */
    e = (uint64_t *)(base + 0x1000);
    /* 0-1 GiB */
    *(e + 0) = (base + 0x2000) | 0x007;
    /* 3-4 GiB */
    *(e + 3) = (base + 0x3000) | 0x007;
    /* 4-5 GiB */
    *(e + 4) = (base + 0x4000) | 0x007;

    /* 0-1 GiB */
    e = (uint64_t *)(base + 0x2000);
    for ( i = 0; i < 512; i++ ) {
        *(e + i) = (0x00200000 * i) | 0x83;
    }
    /* 3-4 GiB (first 2 MiB) */
    e = (uint64_t *)(base + 0x3000);
    *(e + 0) = (0x00000000ULL) | 0x83;
    /* 4-5 GiB (first 64 MiB) */
    e = (uint64_t *)(base + 0x4000);
    for ( i = 0; i < 32; i++ ) {
        *(e + i) = (0x0ULL + 0x00200000 * i) | 0x83;
    }

    set_cr3(base);
}

/*
 * Entry point for C code
 */
void
kstart(void)
{
    /* Print message */
    uint16_t *base;
    int offset;
    int nr;
    int i;
    sysaddrmap_entry_t *ent;

    /* Setup and enable the kernel page table */
    setup_kernel_pgt();

    /* Check the size of the data structure first */
    if ( sizeof(phys_memory_t) > MEMORY_PAGESIZE ) {
        /* Must raise an error */
        return;
    }

    /* Initialize the buddy system */
    nr = *(uint16_t *)BI_MM_NENT_ADDR;
    phys_memory_init(KVAR_PHYSMEM, nr,
                     (memory_sysmap_entry_t *)BI_MM_TABLE_ADDR, 0x100000000ULL);

    /* Messaging region */
    base = (uint16_t *)0xb8000;
    print_str(base, "Welcome to advos (64-bit)!");
    base += 80;

    nr = *(uint16_t *)BI_MM_NENT_ADDR;
    offset = print_str(base, "System memory map; # of entries = 0x");
    print_hex(base + offset, nr, 2);
    base += 80;

    print_str(base, "Base             Length           Type     Attribute");
    base += 80;

    ent = (sysaddrmap_entry_t *)BI_MM_TABLE_ADDR;
    for ( i = 0; i < nr; i++ ) {
        print_hex(base, ent->base, 8);
        print_hex(base + 17, ent->len, 8);
        print_hex(base + 34, ent->type, 4);
        print_hex(base + 43, ent->attr, 4);
        base += 80;
        ent++;
    }

    /* Sleep forever */
    for ( ;; ) {
        hlt();
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
