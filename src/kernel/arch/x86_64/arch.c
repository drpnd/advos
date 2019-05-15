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
 * FITNESS FOR A PARTICULAR PURPSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../../../boot/bootinfo.h"
#include "arch.h"
#include "arch_var.h"
#include "acpi.h"
#include "apic.h"
#include "desc.h"
#include "i8254.h"
#include "pgt.h"
#include "const.h"
#include "../../kernel.h"
#include "../../memory.h"
#include "../../kvar.h"
#include <stdint.h>
#include <sys/syscall.h>

/* For trampoline code */
void trampoline(void);
void trampoline_end(void);

/* Prototype declarations */
int arch_memory_map(void *, uintptr_t, page_t *);
int arch_memory_unmap(void *, uintptr_t, page_t *);

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
 * panic -- print an error message and stop everything
 * damn blue screen, lovely green screen
 */
void
panic(const char *fmt, ...)
{
    va_list ap;
    uint16_t *video;
    uint16_t val;
    int i;
    int col;
    int ln;
    char buf[80 * 25];
    char *s;

    /* Format */
    va_start(ap, fmt);
    kvsnprintf(buf, 80 * 25, fmt, ap);
    va_end(ap);

    /* Disable interrupt */
    cli();

    if ( ((arch_var_t *)g_kvar->arch)->mp_enable ) {
        /* Notify other processors to halt */
        /* Broadcast IPI with IV_CRASH */
        lapic_bcast_fixed_ipi(IV_CRASH);
    }

    /* Video RAM */
    video = (uint16_t *)VIDEO_RAM_80X25;

    /* Fill out with green */
    for ( i = 0; i < 80 * 25; i++ ) {
        video[i] = 0x2f00;
    }

    col = 0;
    ln = 0;
    s = buf;
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
 * Error handler without error code
 */
void
isr_exception(uint32_t vec, uint64_t rip, uint64_t cs, uint64_t rflags,
              uint64_t rsp)
{
    char buf[4096];

    ksnprintf(buf, sizeof(buf), "Exception: vec=%llx, rip=%llx, "
              "cs=%llx, rflags=%llx, rsp=%llx", vec, rip, cs, rflags, rsp);
    panic(buf);
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
 * Error handler for page fault (#PF)
 */
void
isr_page_fault(uint64_t virtual, uint64_t error, uint64_t rip, uint64_t cs,
               uint64_t rflags, uint64_t rsp)
{
    char buf[4096];

    ksnprintf(buf, sizeof(buf), "#PF: virtual=%llx, error=%llx, rip=%llx, "
              "cs=%llx, rflags=%llx, rsp=%llx", virtual, error, rip, cs, rflags,
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

    if ( acpi->num_memory_region > 1 ) {
        for ( i = 0; i < acpi->num_memory_region; i++ ) {
            s = acpi->memory_domain[i].base;
            t = s + acpi->memory_domain[i].length;
            dom = acpi->memory_domain[i].domain;
            if ( base >= s && next <= t ) {
                /* Within the domain, then add this region to the buddy
                   system */
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
    } else {
        /* Non-NUMA (UMA) */
        phys_mem_buddy_add_region(mem->numazones[0].heads,
                                  base + mem->p2v, next + mem->p2v);
    }
}

/*
 * Setup temporary kernel page table
 * Linear mapping summary:
 *   Virtual address     | Physical address
 *   ------------------- | -------------------
 *   0000 0000 0000 0000 | 0000 0000 0000 0000
 *   0000 0000 4000 0000 | N/A
 *   0000 0000 c000 0000 | 0000 0000 0000 0000
 *   0000 0000 c020 0000 | N/A
 *   0000 0000 fec0 0000 | 0000 0000 fec0 0000
 *   0000 0001 0000 0000 | 0000 0000 0000 0000
 *   0000 0001 0400 0000 | N/A
 */
static int
_init_temporary_pgt(void)
{
    pgt_t tmppgt;
    int i;
    int ret;

    /* Setup and enable the kernel page table */
    pgt_init(&tmppgt, (void *)PGT_BOOT, 0);
    for ( i = 1; i < 6; i++ ) {
        pgt_push(&tmppgt, (void *)PGT_BOOT + 4096 * i);
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
    int ret;
    pgt_t *pgt;

    /* Get the maximum address of the system memory */
    maxaddr = 0;
    for ( i = 0; i < nr; i++ ) {
        addr = map[i].base + map[i].len;
        if ( addr > maxaddr ) {
            maxaddr = addr;
        }
    }
    /* # of superpages */
    npg = ((maxaddr + 0x1fffff) >> 21);

    /* Allocate 512 pages for page tables */
    pages = phys_mem_buddy_alloc(kvar->phys.czones[MEMORY_ZONE_KERNEL].heads,
                                 9);
    if ( NULL == pages ) {
        return -1;
    }

    /* Initialize the kernel page table */
    pgt = &((arch_var_t *)kvar->arch)->pgt;
    pgt_init(pgt, pages, KERNEL_LMAP);
    for ( i = 1; i < (1 << 9); i++ ) {
        pgt_push(pgt, pages + i * 4096);
    }

    /* Initialize the virtual memory management */
    ret = memory_init(&kvar->mm, &kvar->phys, pgt, KERNEL_LMAP,
                      arch_memory_map, arch_memory_unmap);
    if ( ret < 0 ) {
        panic("Failed to initialize the memory manager.");
    }
    ret = memory_block_add(&kvar->mm, 0xc0000000ULL, 0xffffffffULL);
    if ( ret < 0 ) {
        panic("Failed to add kernel memory block.");
    }

    /* Map the first 2 MiB */
    ret = memory_wire(&kvar->mm, 0xc0000000ULL, 512, 0x00000000ULL);
    if ( ret < 0 ) {
        panic("Failed to wire kernel memory (lower).");
    }
    /* Map the APIC region */
    ret = memory_wire(&kvar->mm, 0xfec00000ULL, 5120, 0xfec00000ULL);
    if ( ret < 0 ) {
        panic("Failed to wire kernel memory (upper).");
    }

    /* Linear mapping */
    ret = memory_block_add(&kvar->mm, (uintptr_t)KERNEL_LMAP,
                           (uintptr_t)KERNEL_LMAP
                           + npg * MEMORY_SUPERPAGESIZE - 1);
    if ( ret < 0 ) {
        panic("Failed to add linear mapping memory block.");
    }
    ret = memory_wire(&kvar->mm, (uintptr_t)KERNEL_LMAP,
                      npg << (MEMORY_SUPERPAGESIZE_SHIFT
                              - MEMORY_PAGESIZE_SHIFT),
                      0x00000000ULL);
    if ( ret < 0 ) {
        panic("Failed to wire linear mapping region.");
    }

    /* Activate the page table */
    pgt_set_cr3(pgt);

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
 * Estimate bus frequency
 */
static uint64_t
_estimate_bus_freq(acpi_t *acpi)
{
    uint32_t t0;
    uint32_t t1;
    uint32_t probe;
    uint64_t ret;

    /* Start timer */
    t0 = 0xffffffff;
    lapic_set_timer(0xffffffff, APIC_TMRDIV_X16);

    /* Set probe timer (100 ms) */
    probe = 100000;

    /* Sleep probing time */
    acpi_busy_usleep(acpi, probe);

    t1 = lapic_stop_and_read_timer();

    /* Calculate the APIC bus frequency */
    ret = (uint64_t)(t0 - t1) << 4;
    ret = ret * 1000000 / probe;

    return ret;
}

/*
 * Map virtual address to physical address
 */
int
arch_memory_map(void *arch, uintptr_t virtual, page_t *page)
{
    pgt_t *pgt;
    int ret;
    int superpage;
    int global;
    int rw;
    int user;
    int i;
    int nr;
    uintptr_t pagesize;

    /* Check the size of the page */
    if ( page->order >= (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT) ) {
        /* Superpage */
        superpage = 1;
        pagesize = MEMORY_SUPERPAGESIZE;
        nr = page->order - (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT);
    } else {
        /* Page */
        superpage = 0;
        pagesize = MEMORY_PAGESIZE;
        nr = page->order;
    }
    /* Global (temporary) */
    global = 1;
    /* Check the read-write bit */
    if ( page->flags & MEMORY_PGF_RW ) {
        rw = 1;
    } else {
        rw = 0;
    }
    /* User allowed (temporary) */
    user = 1;

    pgt = (pgt_t *)arch;
    for ( i = 0; i < (1LL << nr); i++ ) {
        ret = pgt_map(pgt, virtual + pagesize * i,
                      page->physical + pagesize * i, superpage, global, rw,
                      user);
        if ( ret < 0 ) {
            break;
        }
    }
    if ( ret < 0 ) {
        for ( i = i - 1; i >= 0; i-- ) {
            pgt_unmap(pgt, virtual + pagesize * i, superpage);
        }
        return -1;
    }

    return 0;
}

/*
 * Map virtual address to physical address
 */
int
arch_memory_unmap(void *arch, uintptr_t virtual, page_t *page)
{
    pgt_t *pgt;
    int superpage;
    int i;
    int nr;
    uintptr_t pagesize;

    pgt = (pgt_t *)arch;

    /* Check the size of the page */
    if ( page->order >= (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT) ) {
        /* Superpage */
        pagesize = MEMORY_SUPERPAGESIZE;
        superpage = 1;
        nr = page->order - (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT);
    } else {
        /* Page */
        pagesize = MEMORY_PAGESIZE;
        superpage = 0;
        nr = page->order;
    }

    for ( i = 0; i < (1LL << nr); i++ ) {
        pgt_unmap(pgt, virtual + pagesize * i, superpage);
    }

    return 0;
}

/*
 * Local APIC timer handler
 */
struct arch_task *taska;
struct arch_task *taskb;
struct arch_task *taski;
void
ksignal_clock(void)
{
    uint16_t *base;
    static uint64_t cnt = 0;

    if ( lapic_id() == 0 ) {
        base = (uint16_t *)VIDEO_RAM_80X25;
        base += 80 * 24;
        print_hex(base, cnt / 100, 8);
        cnt++;

        /* Schedule next task */
        struct arch_cpu_data *cpu;
        cpu = (struct arch_cpu_data *)CPU_TASK(0);
        if ( cpu->cur_task == taska ) {
            cpu->next_task = taskb;
        } else if ( cpu->cur_task == taskb ) {
            cpu->next_task = taski;
        } else {
            cpu->next_task = taska;
        }
    }
}

/*
 * Idle task
 */
void
task_idle(void)
{
    uint16_t *base;
    uint64_t cnt;

    base = (uint16_t *)VIDEO_RAM_80X25;
    cnt = 0;
    while ( 1 ) {
        if ( (cnt / 10) & 1 )  {
            *(base + 80 * lapic_id() + 79) = 0x0700 | '!';
        } else {
            *(base + 80 * lapic_id() + 79) = 0x0700 | ' ';
        }
        cnt++;
        hlt();
    }
}

/*
 * Task A
 */
void
task_a(void)
{
    uint64_t cnt = 0;

    while ( 1 ) {
        syscall(766, 22, cnt);
        cnt++;
    }
}

/*
 * Task B
 */
void
task_b(void)
{
    uint64_t cnt = 0;

    while ( 1 ) {
        syscall(766, 23, cnt);
        cnt++;
    }
}

/*
 * Create tasks
 */
static int
_prepare_multitasking(void)
{
    struct arch_cpu_data *cpu;

    /* Task A */
    taska = kmalloc(sizeof(struct arch_task));
    if ( NULL == taska ) {
        return -1;
    }
    taska->kstack = kmalloc(4096);
    if ( NULL == taska->kstack ) {
        return -1;
    }
    taska->ustack = kmalloc(4096);
    if ( NULL == taska->ustack ) {
        return -1;
    }
    taska->rp = taska->kstack + 4096 - 16 - sizeof(struct stackframe64);
    kmemset(taska->rp, 0, sizeof(struct stackframe64));
    taska->sp0 = (uint64_t)taska->kstack + 4096 - 16;
    taska->rp->sp = (uint64_t)taska->ustack + 4096 - 16;
    taska->rp->ip = (uint64_t)task_a;
    taska->rp->cs = GDT_RING3_CODE64_SEL + 3;
    taska->rp->ss = GDT_RING3_DATA64_SEL + 3;
    taska->rp->fs = GDT_RING3_DATA64_SEL + 3;
    taska->rp->gs = GDT_RING3_DATA64_SEL + 3;
    taska->rp->flags = 0x202;
    /* Task B */
    taskb = kmalloc(sizeof(struct arch_task));
    if ( NULL == taskb ) {
        return -1;
    }
    taskb->kstack = kmalloc(4096);
    if ( NULL == taskb->kstack ) {
        return -1;
    }
    taskb->ustack = kmalloc(4096);
    if ( NULL == taskb->ustack ) {
        return -1;
    }
    taskb->rp = taskb->kstack + 4096 - 16 - sizeof(struct stackframe64);
    kmemset(taskb->rp, 0, sizeof(struct stackframe64));
    taskb->sp0 = (uint64_t)taskb->kstack + 4096 - 16;
    taskb->rp->sp = (uint64_t)taskb->ustack + 4096 - 16;
    taskb->rp->ip = (uint64_t)task_b;
    taskb->rp->cs = GDT_RING3_CODE64_SEL + 3;
    taskb->rp->ss = GDT_RING3_DATA64_SEL + 3;
    taskb->rp->fs = GDT_RING3_DATA64_SEL + 3;
    taskb->rp->gs = GDT_RING3_DATA64_SEL + 3;
    taskb->rp->flags = 0x202;

    /* Idle task */
    taski = kmalloc(sizeof(struct arch_task));
    if ( NULL == taski ) {
        return -1;
    }
    taski->kstack = kmalloc(4096);
    if ( NULL == taski->kstack ) {
        return -1;
    }
    taski->ustack = kmalloc(4096);
    if ( NULL == taski->ustack ) {
        return -1;
    }
    taski->rp = taski->kstack + 4096 - 16 - sizeof(struct stackframe64);
    kmemset(taski->rp, 0, sizeof(struct stackframe64));
    taski->sp0 = (uint64_t)taski->kstack + 4096 - 16;
    taski->rp->sp = (uint64_t)taski->ustack + 4096 - 16;
    taski->rp->ip = (uint64_t)task_idle;
    taski->rp->cs = GDT_RING0_CODE_SEL;
    taski->rp->ss = GDT_RING0_DATA_SEL;
    taski->rp->fs = GDT_RING0_DATA_SEL;
    taski->rp->gs = GDT_RING0_DATA_SEL;
    taski->rp->flags = 0x202;

    /* Set the task A as the initial task */
    cpu = (struct arch_cpu_data *)CPU_TASK(0);
    cpu->cur_task = NULL;
    cpu->next_task = taska;
    cpu->idle_task = taski;

    return 0;
}

/*
 * syscall_setup
 */
void
syscall_init(void *table, int nr)
{
    uint64_t val;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;

    /* Check CPUID.0x80000001 for syscall support */
    rax = cpuid(0x80000001, &rbx, &rcx, &rdx);
    if ( !((rdx >> 11) & 1) || !((rdx >> 29) & 1) ) {
        panic("syscall is not supported.");
    }

    /* EFLAG mask */
    wrmsr(MSR_IA32_FMASK, 0x0002);

    /* Entry point to the system call */
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    /* Syscall/sysret segments */
    val = GDT_RING0_CODE_SEL | ((GDT_RING3_CODE32_SEL + 3) << 16);
    wrmsr(MSR_IA32_STAR, val << 32);

    /* Enable syscall */
    val = rdmsr(MSR_IA32_EFER);
    val |= 1;
    wrmsr(MSR_IA32_EFER, val);

    /* Call assembly syscall_setup() */
    syscall_setup((uint64_t)table, (uint64_t)nr);
}

/*
 * Entry point for C code
 */
void
bsp_start(void)
{
    int nr;
    kvar_t *kvar;
    int ret;
    struct gdtr *gdtr;
    struct idtr *idtr;
    acpi_t *acpi;
    size_t sz;
    sysaddrmap_entry_t *ent;
    int i;
    uint64_t busfreq;
    console_dev_t *dev;

    /* Kernel variables */
    kvar = (kvar_t *)KVAR_ADDR;
    ret = kvar_init(kvar, KVAR_SIZE, sizeof(arch_var_t));
    if ( ret < 0 ) {
        panic("kvar_t exceeds the expected size.");
    }

    /* Setup and enable the kernel page table */
    _init_temporary_pgt();

    /* Initialize global descriptor table (GDT) */
    gdtr = gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table (IDT)  */
    idtr = idt_init();
    idt_load();

    /* Load LDT */
    lldt(0);

    /* Initialize TSS and load BSP's task register */
    tss_init();
    tr_load(lapic_id());

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
                     KERNEL_LMAP);

    /* Initialize the kernel table */
    ret = _init_kernel_pgt(kvar, nr, (memory_sysmap_entry_t *)BI_MM_TABLE_ADDR);
    if ( ret < 0 ) {
        panic("Failed to setup linear mapping page table.");
    }

    /* Allocate memory for ACPI parser */
    if ( sizeof(acpi_t) > MEMORY_PAGESIZE * 4 ) {
        panic("The size of acpi_t exceeds the expected size.");
    }
    acpi = memory_alloc_pages(&kvar->mm, 4, MEMORY_ZONE_KERNEL, 0);
    if ( NULL == acpi ) {
        panic("Memory allocation failed for acpi_t.");
    }

    /* Load ACPI information */
    ret = acpi_load(acpi, KERNEL_LMAP);
    if ( ret < 0 ) {
        panic("Failed to load ACPI configuration.");
    }

    /* Initialize the NUMA-aware zones */
    ret = _init_numa_zones(&kvar->phys, acpi, nr,
                           (memory_sysmap_entry_t *)(BI_MM_TABLE_ADDR
                                                     + KERNEL_LMAP));
    if ( ret < 0 ) {
        panic("Failed to initialize the NUMA-aware zones.");
    }

    /* Initialize the slab allocator */
    ret = memory_slab_init(&kvar->slab, &kvar->mm);
    if ( ret < 0 ) {
        panic("Failed to initialize the slab allocator.");
    }

    /* Initialize kmmaloc */
    ret = kmalloc_init(&kvar->slab);
    if ( ret < 0 ) {
        panic("Failed to initialize the kmalloc slab.");
    }

    /* Initialize the console */
    dev = vconsole_init();
    if ( NULL == dev ) {
        panic("Cannot initialize the video console.");
    }
    g_kvar->console.dev = dev;

    /* Prepare multitasking */
    ret = _prepare_multitasking();
    if ( ret < 0 ) {
        panic("Failed to prepare multitasking");
    }

    /* Initialiez I/O APIC */
    ioapic_init();
    /* Map IRQ */
    for ( i = 0; i < 16; i++ ) {
        ioapic_map_intr(0x20 + i, i, acpi->ioapic_base);
    }

    /* Initialize the kernel in C code */
    ret = kernel_init();
    if ( ret < 0 ) {
        panic("Failed to initialize the kernel.");
    }

    /* Setup system call */
    syscall_init(g_kvar->syscalls, SYS_MAXSYSCALL);

    /* Setup trap gates */
    idt_setup_intr_gate(IV_LOC_TMR, intr_apic_loc_tmr);
    idt_setup_intr_gate(IV_CRASH, intr_apic_loc_tmr);
    idt_setup_trap_gate(0, intr_de);
    idt_setup_trap_gate(1, intr_db);
    idt_setup_trap_gate(2, intr_nmi);
    idt_setup_trap_gate(3, intr_bp);
    idt_setup_trap_gate(4, intr_of);
    idt_setup_trap_gate(5, intr_br);
    idt_setup_trap_gate(6, intr_ud);
    idt_setup_trap_gate(7, intr_nm);
    idt_setup_trap_gate(8, intr_df);
    idt_setup_trap_gate(9, intr_cso);
    idt_setup_trap_gate(10, intr_ts);
    idt_setup_trap_gate(11, intr_np);
    idt_setup_trap_gate(12, intr_ss);
    idt_setup_trap_gate(13, intr_gp);
    idt_setup_trap_gate(14, intr_pf);
    idt_setup_trap_gate(16, intr_mf);
    idt_setup_trap_gate(17, intr_ac);
    idt_setup_trap_gate(18, intr_mc);
    idt_setup_trap_gate(19, intr_xm);
    idt_setup_trap_gate(20, intr_ve);
    idt_setup_trap_gate(30, intr_sx);
    idt_setup_intr_gate(0x21, intr_irq1);

    /* Prepare stack for appliation processors.  N.B., the stack must be in the
       kernel zone so that 32-bit code can refer to it. */
    void *bstack;
    bstack = memory_alloc_pages(&kvar->mm, MAX_PROCESSORS,
                                MEMORY_ZONE_KERNEL, 0);
    if ( NULL == bstack ) {
        panic("Cannot allocate boot stack for application processors.");
    }
    kmemset(bstack, 0, MAX_PROCESSORS * MEMORY_PAGESIZE);
    *(uintptr_t *)(APVAR_CR3 + KERNEL_LMAP)
        = ((arch_var_t *)kvar->arch)->pgt.cr3;
    *(uintptr_t *)(APVAR_SP + KERNEL_LMAP) = (uintptr_t)bstack;

    /* ACPI */
    ((arch_var_t *)kvar->arch)->acpi = acpi;

    /* Load trampoline code for multicore support */
    sz = (uint64_t)trampoline_end - (uint64_t)trampoline;
    if ( sz > TRAMPOLINE_MAX_SIZE ) {
        panic("Trampoline code is too large to load.");
    }
    kmemcpy((void *)(TRAMPOLINE_VEC << 12) + KERNEL_RELOCBASE, trampoline, sz);

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

    /* Messaging region */
    kprintf("Welcome to advos (64-bit)!\r\n");
    busfreq = _estimate_bus_freq(acpi);
    kprintf("Estimated bus frequency: %lld Hz\r\n", busfreq);

    /* Print CPU domain */
    nr = 0;
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        if ( acpi->lapic_domain[i].valid ) {
            nr++;
        }
    }
    kprintf("# of CPU cores: %lld\r\n", nr);

    /* Print memory information */
    kprintf("Base             Length           Domain\r\n");
    for ( i = 0; i < acpi->num_memory_region; i++ ) {
        kprintf("%016llx %016llx %016llx\r\n",
                (uintptr_t)acpi->memory_domain[i].base,
                (uintptr_t)acpi->memory_domain[i].length,
                (uintptr_t)acpi->memory_domain[i].domain);
    }
    kprintf("----------\r\n");

    /* System memory */
    nr = *(uint16_t *)(BI_MM_NENT_ADDR + KERNEL_LMAP);
    kprintf("System memory map; # of entries = %lld\r\n", nr);

    kprintf("Base             Length           Type     Attribute\r\n");

    ent = (sysaddrmap_entry_t *)(BI_MM_TABLE_ADDR + KERNEL_LMAP);
    for ( i = 0; i < nr; i++ ) {
        kprintf("%016llx %016llx %016llx %016llx\r\n", ent->base, ent->len,
                ent->type, ent->attr);
        ent++;
    }

    lapic_start_timer(busfreq, HZ, IV_LOC_TMR);
    task_restart();

    /* The following code will never be reached... */
    /* Enable interrupt */
    sti();

    /* Sleep forever */
    for ( ;; ) {
        hlt();
    }
}

/*
 * Create tasks
 */
static int
_prepare_idle_task(int lapic_id)
{
    struct arch_cpu_data *cpu;
    struct arch_task *idle;

    /* Idle task */
    idle = kmalloc(sizeof(struct arch_task));
    if ( NULL == idle ) {
        return -1;
    }
    idle->kstack = kmalloc(4096);
    if ( NULL == idle->kstack ) {
        return -1;
    }
    idle->ustack = kmalloc(4096);
    if ( NULL == idle->ustack ) {
        return -1;
    }
    idle->rp = idle->kstack + 4096 - 16 - sizeof(struct stackframe64);
    kmemset(idle->rp, 0, sizeof(struct stackframe64));
    idle->sp0 = (uint64_t)idle->kstack + 4096 - 16;
    idle->rp->sp = (uint64_t)idle->ustack + 4096 - 16;
    idle->rp->ip = (uint64_t)task_idle;
    idle->rp->cs = GDT_RING0_CODE_SEL;
    idle->rp->ss = GDT_RING0_DATA_SEL;
    idle->rp->fs = GDT_RING0_DATA_SEL;
    idle->rp->gs = GDT_RING0_DATA_SEL;
    idle->rp->flags = 0x202;

    /* Set the task A as the initial task */
    cpu = (struct arch_cpu_data *)(uintptr_t)CPU_TASK(lapic_id);
    cpu->cur_task = NULL;
    cpu->next_task = idle;
    cpu->idle_task = idle;

    return 0;
}

/*
 * Entry point for application processors
 */
void
ap_start(void)
{
    uint16_t *base;
    int ret;
    uint64_t busfreq;

    base = (uint16_t *)VIDEO_RAM_80X25;
    *(base + 80 * lapic_id() + 79) = 0x0700 | '!';

    /* Load GDT and IDT */
    gdt_load();
    idt_load();

    /* Load LDT */
    lldt(0);

    /* Load TSS */
    tr_load(lapic_id());

    /* Estimate bus frequency */
    busfreq = _estimate_bus_freq(((arch_var_t *)g_kvar->arch)->acpi);

    /* Prepare per-core data */
    ret = _prepare_idle_task(lapic_id());
    if ( ret < 0 ) {
        panic("Cannot initialize the idle task.");
    }

    lapic_start_timer(busfreq, HZ, IV_LOC_TMR);
    task_restart();

    /* The following code will never be reached... */
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
