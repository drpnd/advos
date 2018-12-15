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

#include "../../../boot/bootinfo.h"
#include "arch.h"
#include "kvar.h"
#include "acpi.h"
#include "../../kernel.h"
#include "../../memory.h"
#include <stdint.h>

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
 * invariant_tsc_freq -- resolve the base frequency of invariant TSC
 */
uint64_t
invariant_tsc_freq(void)
{
    uint64_t reg;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t family;
    uint64_t model;

    /* Check Invariant TSC support */
    cpuid(0x80000007, &rbx, &rcx, &rdx);
    if ( !(rdx & 0x100) ) {
        return 0;
    }

    /* Read TSC frequency */
    reg = (rdmsr(MSR_PLATFORM_INFO) & 0xff00) >> 8;

    /* CPUID;EAX=0x01 */
    rax = cpuid(0x01, &rbx, &rcx, &rdx);
    family = ((rax & 0xf00) >> 8) | ((rax & 0xff00000) >> 12);
    model = ((rax & 0xf0) >> 4) | ((rax & 0xf0000) >> 12);
    if ( 0x06 == family ) {
        switch ( model ) {
        case 0x1e: /* Nehalem */
        case 0x1a: /* Nehalem */
        case 0x2e: /* Nehalem */
            /* 133.33 MHz */
            return reg * 133330000;
        case 0x2a: /* SandyBridge */
        case 0x2d: /* SandyBridge-E */
        case 0x3a: /* IvyBridge */
        case 0x3c: /* Haswell */
        case 0x3d: /* Broadwell */
        case 0x46: /* Skylake */
        case 0x4e: /* Skylake */
        case 0x57: /* Xeon Phi */
            /* 100.00 MHz */
            return reg * 100000000;
        default:
            /* Presumedly 100.00 MHz for other processors */
            return reg * 100000000;
        }
    }

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
 * panic -- print an error message and stop everything
 * damn blue screen, lovely green screen
 */
void
panic(const char *s)
{
    uint16_t *video;
    uint16_t val;
    int i;
    int col;
    int ln;

    /* Video RAM */
    video = (uint16_t *)0xb8000;

    /* Fill out with green */
    for ( i = 0; i < 80 * 25; i++ ) {
        video[i] = 0x2f00;
    }

    col = 0;
    ln = 0;
    for ( i = 0; *s; s++  ) {
        switch ( *s ) {
        case '\r':
            video -= col;
            i -= col;
            col = 0;
            break;
        case '\n':
            video += 80;
            i += 80;
            ln++;
            break;
        default:
            *video = 0x2f00 | *s;
            video++;
            i++;
            col++;
        }
    }

    /* Move the cursor */
    val = ((i & 0xff) << 8) | 0x0f;
    outw(0x3d4, val);   /* Low */
    val = (((i >> 8) & 0xff) << 8) | 0x0e;
    outw(0x3d4, val);   /* High */

    /* Stop forever */
    while ( 1 ) {
        hlt();
    }
}

/*
 * Entry point for C code
 */
void
bsp_start(void)
{
    /* Print message */
    uint16_t *base;
    int offset;
    int nr;
    int i;
    sysaddrmap_entry_t *ent;
    phys_memory_t *mem;
    acpi_t *acpi;
    int ret;

    /* Setup and enable the kernel page table */
    setup_kernel_pgt();

    /* Check the size of the data structure first */
    if ( sizeof(phys_memory_t) > MEMORY_PAGESIZE ) {
        /* Must raise an error */
        return;
    }

    /* Initialize the buddy system */
    nr = *(uint16_t *)BI_MM_NENT_ADDR;
    mem = KVAR_PHYSMEM;
    phys_memory_init(mem, nr, (memory_sysmap_entry_t *)BI_MM_TABLE_ADDR,
                     0x100000000ULL);
    if ( sizeof(acpi_t) > MEMORY_PAGESIZE * 4 ) {
        panic("The size of acpi_t exceeds the expected size.");
    }
    acpi = phys_mem_buddy_alloc(mem->czones[MEMORY_ZONE_KERNEL].heads, 2);
    ret = acpi_load(acpi);
    if ( ret < 0 ) {
        panic("Failed to load ACPI configuration.");
    }

    /* Messaging region */
    base = (uint16_t *)0xb8000;
    print_str(base, "Welcome to advos (64-bit)!");
    base += 80;

    /* Print CPU domain */
    nr = 0;
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        if ( acpi->lapic_domain[i].valid ) {
            nr++;
        }
    }
    offset = print_str(base, "# of CPU cores = 0x");
    print_hex(base + offset, nr, 4);
    base += 80;

    /* Print memory information */
    print_str(base, "Base             Length           Domain");
    base += 80;
    for ( i = 0; i < acpi->num_memory_region; i++ ) {
        print_hex(base, (uintptr_t)acpi->memory_domain[i].base, 8);
        print_hex(base + 17, (uintptr_t)acpi->memory_domain[i].length, 8);
        print_hex(base + 34, (uintptr_t)acpi->memory_domain[i].domain, 8);
        base += 80;
    }

    /* Wait 5 seconds */
    acpi_busy_usleep(acpi, 5000000);

    print_str(base, "----------");
    base += 80;

    /* System memory */
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
