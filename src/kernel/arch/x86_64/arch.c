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
#include "apic.h"
#include "desc.h"
#include "i8254.h"
#include "pgt.h"
#include "const.h"
#include "../../kernel.h"
#include "../../memory.h"
#include <stdint.h>

#define P2V_OFFSET      0x100000000ULL

#define set_cr3(cr3)    __asm__ __volatile__ ("movq %%rax,%%cr3" :: "a"((cr3)))
#define invlpg(addr)    __asm__ __volatile__ ("invlpg (%%rax)" :: "a"((addr)))

/* For trampoline code */
void trampoline(void);
void trampoline_end(void);


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
 * Error handler with error code
 */
void
isr_exception_werror(uint32_t vec, uint64_t error, uint64_t rip, uint64_t cs,
                     uint64_t rflags, uint64_t rsp)
{
    char buf[4096];

    ksnprintf(buf, sizeof(buf), "Exception: vec=%llx, error=%llx, rip=%llx, "
              "cs=%llx, rflags=%llx, rsp=%llx", vec, error, rip, cs, rflags,
              rsp);
    panic(buf);
}

/*
 * Add a region to buddy system
 */
static void
_add_region_to_numa_zones(phys_memory_t *mem, acpi_t *acpi, uintptr_t base,
                          uintptr_t next)
{
    int i;
    uintptr_t s;
    uintptr_t t;
    int dom;

    for ( i = 0; i < acpi->num_memory_region; i++ ) {
        s = acpi->memory_domain[i].base;
        t = s + acpi->memory_domain[i].length;
        dom = acpi->memory_domain[i].domain;
        if ( base >= s && next <= t ) {
            /* Within the domain, then add this region to the buddy system */
            phys_mem_buddy_add_region(mem->numazones[dom].heads,
                                      base + mem->p2v, next + mem->p2v);
        } else if ( base >= s ) {
            /* s <= base <= t < next */
            phys_mem_buddy_add_region(mem->numazones[dom].heads,
                                      base + mem->p2v, t + mem->p2v);
        } else if ( next <= t ) {
            /* base < s < next <= t */
            phys_mem_buddy_add_region(mem->numazones[dom].heads,
                                      s + mem->p2v, next + mem->p2v);
        }

    }
}

/*
 * Setup temporary kernel page table
 */
static int
_init_temporary_pgt(void)
{
    pgt_t tmppgt;
    int i;
    int ret;

    /* Setup and enable the kernel page table */
    pgt_init(&tmppgt, (void *)0x69000, 0);
    for ( i = 1; i < 6; i++ ) {
        pgt_push(&tmppgt, (void *)0x69000 + 4096 * i);
    }
    /* 0-1 GiB */
    for ( i = 0; i < 512; i++ ) {
        ret = pgt_map(&tmppgt, i * MEMORY_SUPERPAGESIZE,
                      i * MEMORY_SUPERPAGESIZE,
                      1, 0, 1, 0);
        if ( ret < 0 ) {
            return -1;
        }
    }
    /* 3-4 GiB (first 2 and the tail MiB) */
    for ( i = 0; i < 1; i++ ) {
        ret = pgt_map(&tmppgt,
                      (uintptr_t)KERNEL_RELOCBASE + i * MEMORY_SUPERPAGESIZE,
                      i * MEMORY_SUPERPAGESIZE, 1, 0, 1, 0);
        if ( ret < 0 ) {
            return -1;
        }
    }
    for ( i = 502; i < 512; i++ ) {
        ret = pgt_map(&tmppgt,
                      (uintptr_t)KERNEL_RELOCBASE + i * MEMORY_SUPERPAGESIZE,
                      (uintptr_t)KERNEL_RELOCBASE + i * MEMORY_SUPERPAGESIZE,
                      1, 0, 1, 0);
        if ( ret < 0 ) {
            return -1;
        }
    }
    /* 4-5 GiB (first 64 MiB) */
    for ( i = 0; i < 32; i++ ) {
        ret = pgt_map(&tmppgt,
                      (uintptr_t)KERNEL_LMAP + i * MEMORY_SUPERPAGESIZE,
                      i * MEMORY_SUPERPAGESIZE, 1, 0, 1, 0);
        if ( ret < 0 ) {
            return -1;
        }
    }
    pgt_set_cr3(&tmppgt);

    return 0;
}

/*
 * Initialize the kernel page table
 */
static int
_init_kernel_pgt(kvar_t *kvar, size_t nr, memory_sysmap_entry_t *map)
{
    size_t i;
    uintptr_t addr;
    uintptr_t maxaddr;
    size_t npg;
    void *pages;

    /* Get the maximum address of the system memory */
    maxaddr = 0;
    for ( i = 0; i < nr; i++ ) {
        addr = map[i].base + map[i].len;
        if ( addr > maxaddr ) {
            maxaddr = addr;
        }
    }
    /* # of pages */
    npg = ((maxaddr + 0x1fffff) >> 21);

    /* Allocate 512 pages for page tables */
    pages = phys_mem_buddy_alloc(kvar->phys.czones[MEMORY_ZONE_KERNEL].heads,
                                 9);
    if ( NULL == pages ) {
        return -1;
    }

    /* Initialize the kernel page table */
    pgt_init(&kvar->pgt, pages, KERNEL_LMAP);
    for ( i = 1; i < (1 << 9); i++ ) {
        pgt_push(&kvar->pgt, pages + i * 4096);
    }

    /* 0-1 GiB */
    for ( i = 0; i < 512; i++ ) {
        pgt_map(&kvar->pgt, i * MEMORY_SUPERPAGESIZE, i * MEMORY_SUPERPAGESIZE,
                1, 0, 1, 0);
    }
    /* 3-4 GiB (first 2 and the tail MiB) */
    for ( i = 0; i < 1; i++ ) {
        pgt_map(&kvar->pgt,
                (uintptr_t)KERNEL_RELOCBASE + i * MEMORY_SUPERPAGESIZE,
                i * MEMORY_SUPERPAGESIZE, 1, 0, 1, 0);
    }
    for ( i = 502; i < 512; i++ ) {
        pgt_map(&kvar->pgt,
                (uintptr_t)KERNEL_RELOCBASE + i * MEMORY_SUPERPAGESIZE,
                (uintptr_t)KERNEL_RELOCBASE + i * MEMORY_SUPERPAGESIZE,
                1, 0, 1, 0);
    }
    /* 4-5 GiB (first 64 MiB) */
    for ( i = 0; i < 32; i++ ) {
        pgt_map(&kvar->pgt, (uintptr_t)KERNEL_LMAP + i * MEMORY_SUPERPAGESIZE,
                i * MEMORY_SUPERPAGESIZE, 1, 0, 1, 0);
    }

    /* Linear mapping */
    for ( i = 0; i < npg; i++ ) {
        pgt_map(&kvar->pgt, (uintptr_t)KERNEL_LMAP + i * MEMORY_SUPERPAGESIZE,
                i * MEMORY_SUPERPAGESIZE, 1, 0, 1, 0);
    }

    /* Activate the page table */
    pgt_set_cr3(&kvar->pgt);

    return 0;
}

/*
 * Initialize NUMA-aware zones
 */
static int
_init_numa_zones(phys_memory_t *mem, acpi_t *acpi, int nr,
                 memory_sysmap_entry_t *map)
{
    int i;
    uint32_t max_domain;
    size_t sz;
    phys_memory_zone_t *zones;
    int order;
    uintptr_t base;
    uintptr_t next;

    /* Get the maximum domain number */
    max_domain = 0;
    for ( i = 0; i < acpi->num_memory_region; i++ ) {
        if ( acpi->memory_domain[i].domain > max_domain ) {
            max_domain = acpi->memory_domain[i].domain;
        }
    }

    /* Allocate for the NUMA-aware zones */
    sz = sizeof(phys_memory_zone_t) * (max_domain + 1);
    sz = (sz - 1) >> MEMORY_PAGESIZE_SHIFT;
    order = 0;
    while ( sz ) {
        sz >>= 1;
        order++;
    }
    zones = phys_mem_buddy_alloc(mem->czones[MEMORY_ZONE_KERNEL].heads, order);
    if ( NULL == zones ) {
        return -1;
    }
    kmemset(zones, 0, sizeof(1 << (order + MEMORY_PAGESIZE_SHIFT)));
    mem->numazones = zones;
    mem->max_domain = max_domain;

    for ( i = 0; i < nr; i++ ) {
        /* Base address and the address of the next block (base + length) */
        base = map[i].base;
        next = base + map[i].len;

        /* Ignore Kernel zone */
        if ( base < MEMORY_ZONE_NUMA_AWARE_LB ) {
            base = MEMORY_ZONE_NUMA_AWARE_LB;
        }
        if ( next < MEMORY_ZONE_NUMA_AWARE_LB ) {
            next = MEMORY_ZONE_NUMA_AWARE_LB;
        }

        /* Round up for 4 KiB page alignment */
        base = (base + (MEMORY_PAGESIZE - 1)) & ~(MEMORY_PAGESIZE - 1);
        next = next & ~(MEMORY_PAGESIZE - 1);

        if ( base != next ) {
            /* Add this region to the buddy system */
            _add_region_to_numa_zones(mem, acpi, base, next);
        }
    }

    return 0;
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
    kvar_t *kvar;
    acpi_t *acpi;
    int ret;
    size_t sz;
    struct gdtr *gdtr;
    struct idtr *idtr;

    /* Kernel variables */
    kvar = KVAR;

    /* Setup and enable the kernel page table */
    _init_temporary_pgt();

    /* Initialize global descriptor table (GDT) */
    gdtr = gdt_init();
    lgdt(gdtr, GDT_RING0_CODE_SEL);

    /* Initialize interrupt descriptor table (IDT)  */
    idtr = idt_init();
    lidt(idtr);

    /* Load LDT */
    lldt(0);

    /* Initialize TSS and load BSP's task register */
    tss_init();
    tr_load(0);

    /* Ensure the i8254 timer is stopped */
    i8254_stop_timer();

    /* Check the size of the data structure first */
    if ( sizeof(phys_memory_t) > MEMORY_PAGESIZE ) {
        /* Must raise an error */
        panic("phys_memory_t exceeds the expected size.");
    }

    /* Initialize the buddy system for the core memory */
    nr = *(uint16_t *)BI_MM_NENT_ADDR;
    phys_memory_init(&kvar->phys, nr, (memory_sysmap_entry_t *)BI_MM_TABLE_ADDR,
                     P2V_OFFSET);

    /* Initialize the kernel table */
    ret = _init_kernel_pgt(kvar, nr, (memory_sysmap_entry_t *)BI_MM_TABLE_ADDR);
    if ( ret < 0 ) {
        panic("Failed to setup linear mapping page table.");
    }

    /* Allocate memory for ACPI parser */
    if ( sizeof(acpi_t) > MEMORY_PAGESIZE * 4 ) {
        panic("The size of acpi_t exceeds the expected size.");
    }
    acpi = phys_mem_buddy_alloc(kvar->phys.czones[MEMORY_ZONE_KERNEL].heads, 2);
    if ( NULL == acpi ) {
        panic("Memory allocation failed for acpi_t.");
    }

    /* Load ACPI information */
    ret = acpi_load(acpi);
    if ( ret < 0 ) {
        panic("Failed to load ACPI configuration.");
    }

    /* Initialize the NUMA-aware zones */
    ret = _init_numa_zones(&kvar->phys, acpi, nr,
                           (memory_sysmap_entry_t *)BI_MM_TABLE_ADDR);
    if ( ret < 0 ) {
        panic("Failed to initialize the NUMA-aware zones.");
    }

    /* Load trampoline code */
    sz = (uint64_t)trampoline_end - (uint64_t)trampoline;
    if ( sz > TRAMPOLINE_MAX_SIZE ) {
        panic("Trampoline code is too large to load.");
    }
    kmemcpy((void *)(TRAMPOLINE_VEC << 12), trampoline, sz);

    /* Send INIT IPI */
    lapic_send_init_ipi();

    /* Wait 10 ms */
    acpi_busy_usleep(acpi, 10000);

    /* Send a Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(acpi, 200);

    /* Send another Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(acpi, 200);

    /* Initialiez I/O APIC */
    ioapic_init();
    ioapic_map_intr(0x21, 1, acpi->ioapic_base);

    /* Test interrupt */
    idt_setup_trap_gate(13, intr_gpf);
    idt_setup_intr_gate(0x21, intr_irq1);

    /* Messaging region */
    base = (uint16_t *)0xb8000;
    print_str(base, "Welcome to advos (64-bit)!");
    base += 80;

    /* Testing memory allocator */
    void *ptr;
    ptr = phys_mem_buddy_alloc(kvar->phys.numazones[1].heads, 1);
    print_hex(base, (uintptr_t)ptr, 8);
    base += 80;
    phys_mem_buddy_free(kvar->phys.numazones[1].heads, ptr, 1);
    ptr = phys_mem_buddy_alloc(kvar->phys.numazones[1].heads, 1);
    print_hex(base, (uintptr_t)ptr, 8);
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

    /* Enable interrupt */
    sti();

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
