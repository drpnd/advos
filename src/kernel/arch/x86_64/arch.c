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
#include "../../sched.h"
#include <stdint.h>
#include <sys/syscall.h>

#define VIRT_MEMORY_SLAB_NAME       "virt_memory"
#define VIRT_MEMORY_SLAB_DATA_NAME  "virt_memory_data"
#define PGT_SLAB_NAME               "pgt"
#define ARCH_TASK_NAME              "arch_task"

/* For trampoline code */
void trampoline(void);
void trampoline_end(void);

/* Prototype declarations */
int arch_memory_map(void *, uintptr_t, page_t *, int);
int arch_memory_unmap(void *, uintptr_t, page_t *);
int arch_memory_prepare(void *, uintptr_t, size_t);
int arch_memory_refer(void *, void *, uintptr_t, size_t);
virt_memory_t * arch_memory_new(void);
int arch_memory_ctxsw(void *);
int arch_memory_copy(void *, uintptr_t, uintptr_t, size_t);

static int _prepare_idle_task(int);

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
    volatile uint16_t *video;
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
    video = (volatile uint16_t *)VIDEO_RAM_80X25;

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
 * Error handler for device not available (#NM)
 */
void
isr_device_not_available(uint64_t rip, uint64_t cs, uint64_t rflags,
                         uint64_t rsp)
{
    task_t *t;
    struct arch_task *at;
    struct arch_cpu_data *cpu;

    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;

    (void)rip;
    (void)cs;
    (void)rflags;
    (void)rsp;

    cpu = (struct arch_cpu_data *)(uintptr_t)CPU_TASK(lapic_id());

    /* CPU */
    rax = cpuid(1, &rbx, &rcx, &rdx);
    if ( (rcx >> 26) & 1 ) {
        /* XSAVE: To implement */
    } else if ( (rdx >> 24) & 1 ) {
        /* FXSR */
        t = this_task();
        at = t->arch;
        /* Clear TS */
        clts();
        if ( cpu->fpu_task != at ) {
            if ( NULL != cpu->fpu_task ) {
                fxsave64(cpu->fpu_task->xregs);
            }
            fxrstor64(at->xregs);
            cpu->fpu_task = at;
        }
    }
}

/*
 * Error handler for page fault (#PF)
 */
void
isr_page_fault(uint64_t virtual, uint64_t error, uint64_t rip, uint64_t cs,
               uint64_t rflags, uint64_t rsp)
{
    char buf[4096];
    task_t *t;

    t = this_task();
    ksnprintf(buf, sizeof(buf), "#PF: task=%llx, virtual=%llx, error=%llx, "
              "rip=%llx, cs=%llx, rflags=%llx, rsp=%llx", t, virtual,
              error, rip, cs, rflags,
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
    pgt_init(&tmppgt, (void *)PGT_BOOT, 6, 0);
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
    memory_arch_interfaces_t ifs;
    virt_memory_block_t *b;

    /* Get the maximum address of the system memory */
    maxaddr = 0;
    for ( i = 0; i < nr; i++ ) {
        addr = map[i].base + map[i].len;
        if ( addr > maxaddr ) {
            maxaddr = addr;
        }
    }
    /* # of PDPT */
    npg = ((maxaddr + 0x3fffffff) >> 30);

    /* Allocate 512 pages for page tables */
    pages = phys_mem_buddy_alloc(kvar->phys.czones[MEMORY_ZONE_KERNEL].heads,
                                 9);
    if ( NULL == pages ) {
        return -1;
    }

    /* Initialize the kernel page table */
    pgt = &((arch_var_t *)kvar->arch)->pgt;
    pgt_init(pgt, pages, 1 << 9, KERNEL_LMAP);

    /* Initialize the virtual memory management */
    ifs.map = arch_memory_map;
    ifs.unmap = arch_memory_unmap;
    ifs.prepare = arch_memory_prepare;
    ifs.refer = arch_memory_refer;
    ifs.new = arch_memory_new;
    ifs.ctxsw = arch_memory_ctxsw;
    ifs.copy = arch_memory_copy;
    ret = memory_init(&kvar->mm, &kvar->phys, pgt, KERNEL_LMAP, &ifs);
    if ( ret < 0 ) {
        panic("Failed to initialize the memory manager.");
    }
    b = virt_memory_block_add(&kvar->mm.kmem, 0xc0000000ULL, 0xffffffffULL);
    if ( NULL == b ) {
        panic("Failed to add kernel memory block.");
    }

    /* Map the first 2 MiB */
    ret = virt_memory_wire(&kvar->mm.kmem, 0xc0000000ULL, 512, 0x00000000ULL);
    if ( ret < 0 ) {
        panic("Failed to wire kernel memory (lower).");
    }
    /* Map the APIC region */
    ret = virt_memory_wire(&kvar->mm.kmem, 0xfec00000ULL, 5120, 0xfec00000ULL);
    if ( ret < 0 ) {
        panic("Failed to wire kernel memory (upper).");
    }

    /* Linear mapping */
    b = virt_memory_block_add(&kvar->mm.kmem, (uintptr_t)KERNEL_LMAP,
                                (uintptr_t)KERNEL_LMAP + npg * 0x40000000 - 1);
    if ( NULL == b ) {
        panic("Failed to add linear mapping memory block.");
    }
    ret = virt_memory_wire(&kvar->mm.kmem, (uintptr_t)KERNEL_LMAP,
                           npg << (30 - MEMORY_PAGESIZE_SHIFT), 0x00000000ULL);
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
arch_memory_map(void *arch, uintptr_t virtual, page_t *page, int flags)
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
    /* Global */
    if ( MEMORY_VMF_GLOBAL & flags ) {
        global = 1;
    } else {
        global = 0;
    }
    /* Check the read-write bit */
    if ( (page->flags & MEMORY_PGF_RW) && !(flags & MEMORY_VMF_COW) ) {
        rw = 1;
    } else {
        rw = 0;
    }

    /* User allowed */
    if ( MEMORY_MAP_USER & flags ) {
        user = 1;
    } else {
        user = 0;
    }

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
 * Prepare memory block (page table directory table)
 */
int
arch_memory_prepare(void *arch, uintptr_t virtual, size_t size)
{
    pgt_t *pgt;
    int ret;
    int i;

    pgt = (pgt_t *)arch;

    /* Only available for 1 GiB mapping  */
    if ( size & ((1ULL << 30) - 1) ) {
        return -1;
    }
    for ( i = 0; i < (int)(size >> 30); i++ ) {
        ret = pgt_prepare(pgt, virtual);
        if ( ret < 0 ) {
            return -1;
        }
    }

    return 0;
}

/*
 * Add a reference to the page table directory
 */
int
arch_memory_refer(void *arch, void *tgtarch, uintptr_t virtual, size_t size)
{
    pgt_t *pgt;
    pgt_t *tgt;
    int ret;
    int i;

    pgt = (pgt_t *)arch;
    tgt = (pgt_t *)tgtarch;

    /* Only available for 1 GiB mapping  */
    if ( size & ((1ULL << 30) - 1) ) {
        return -1;
    }

    for ( i = 0; i < (int)(size >> 30); i++ ) {
        ret = pgt_refer(pgt, tgt, virtual + ((uintptr_t)i << 30));
        if ( ret < 0 ) {
            return -1;
        }
    }

    return 0;
}

/*
 * Switch the page table
 */
int
arch_memory_ctxsw(void *arch)
{
    pgt_set_cr3((pgt_t *)arch);

    return 0;
}

/*
 * Copy physical pages
 */
int
arch_memory_copy(void *arch, uintptr_t dst, uintptr_t src, size_t size)
{
    (void)arch;
    kmemcpy((void *)dst + KERNEL_LMAP, (void *)src + KERNEL_LMAP, size);

    return 0;
}

/*
 * Local APIC timer handler
 */
void
ksignal_clock(void)
{
    task_t *t;

    if ( lapic_id() == 0 ) {
        /* Increment jiffies */
        g_kvar->jiffies++;

        /* Schedule next task (and context switch) */
        struct arch_cpu_data *cpu;
        cpu = (struct arch_cpu_data *)CPU_TASK(0);

        if ( NULL != cpu->cur_task ) {
            t = this_task();
            if ( NULL != t ) {
                /* Idle tasks do not have task_t... */
                t->credit--;
                if ( t->credit > 0 ) {
                    /* Keep this task */
                    return;
                }
            }
        }

        /* Execute high-level scheduler */
        if ( NULL == g_kvar->runqueue ) {
            sched_schedule();
        }
        if ( NULL != cpu->cur_task ) {
            if ( NULL != cpu->cur_task->task ) {
                /* FIXME: Add task_t for idle tasks. */
                cpu->cur_task->task->state = TASK_READY;
            }
        }
        if ( NULL == g_kvar->runqueue ) {
            cpu->next_task = cpu->idle_task;
        } else {
            /* Low level scheduler */
            t = g_kvar->runqueue;
            g_kvar->runqueue = t->next;
            cpu->next_task = t->arch;
            t->state = TASK_RUNNING;
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
 * Allocate a virt_memory_data_t data structure
 */
void *
vmem_data_alloc(virt_memory_t *vmem)
{
    void *data;

    (void)vmem;
    data = kmem_slab_alloc(VIRT_MEMORY_SLAB_DATA_NAME);
    if ( NULL == data ) {
        return NULL;
    }

    /* Zeros */
    kmemset(data, 0, sizeof(virt_memory_data_t));

    return data;
}

/*
 * Release a virt_memory_data_t data structure
 */
void
vmem_data_free(virt_memory_t *vmem, void *data)
{
    int ret;

    (void)vmem;
    ret = kmem_slab_free(VIRT_MEMORY_SLAB_DATA_NAME, data);
    kassert( ret == 0 );
}

/*
 * Callback function after kernel memory initialization
 */
int
vmem_callback_init(void)
{
    int ret;

    ret = kmem_slab_create_cache(VIRT_MEMORY_SLAB_NAME, sizeof(virt_memory_t));
    if ( ret < 0 ) {
        return -1;
    }
    ret = kmem_slab_create_cache(VIRT_MEMORY_SLAB_DATA_NAME,
                                 sizeof(virt_memory_data_t));
    if ( ret < 0 ) {
        return -1;
    }
    ret = kmem_slab_create_cache(PGT_SLAB_NAME, sizeof(pgt_t));
    if ( ret < 0 ) {
        return -1;
    }

    ret = task_mgr_init(sizeof(struct arch_task));
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Allocate and initialize a new virtual memory data structure
 */
virt_memory_t *
arch_memory_new(void)
{
    int ret;
    virt_memory_t *vmem;
    virt_memory_allocator_t a;
    void *pages;
    pgt_t *pgt;

    vmem = kmem_slab_alloc(VIRT_MEMORY_SLAB_NAME);
    if ( NULL == vmem ) {
        return NULL;
    }

    /* Prepare pgt_t */
    pages = phys_mem_buddy_alloc(g_kvar->phys.czones[MEMORY_ZONE_KERNEL].heads,
                                 9);
    if ( NULL == pages ) {
        return NULL;
    }
    pgt = kmem_slab_alloc(PGT_SLAB_NAME);
    if ( NULL == pgt ) {
        kmem_slab_free(VIRT_MEMORY_SLAB_NAME, vmem);
        return NULL;
    }
    pgt_init(pgt, pages, 1 << 9, KERNEL_LMAP);
    vmem->arch = pgt;

    a.spec = NULL;
    a.alloc = vmem_data_alloc;
    a.free = vmem_data_free;
    ret = virt_memory_new(vmem, &g_kvar->mm, &a);
    if ( ret < 0 ) {
        phys_mem_buddy_free(g_kvar->phys.czones[MEMORY_ZONE_KERNEL].heads,
                            pages, 9);
        kmem_slab_free(PGT_SLAB_NAME, pgt);
        kmem_slab_free(VIRT_MEMORY_SLAB_NAME, vmem);
        return NULL;
    }
    vmem->flags = MEMORY_MAP_USER;

    return vmem;
}

/*
 * initrd
 */
struct initrd_entry {
    char name[16];
    uint64_t offset;
    uint64_t size;
};

/*
 * Search a file in initrd
 */
static int
_initrd_find_file(const char *fname, void **start, size_t *size)
{
    struct initrd_entry *e;

    e = (void *)INITRD_BASE;
    int i;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(fname, e->name) ) {
            /* Found */
            *start = (void *)INITRD_BASE + e->offset;
            *size = e->size;
            return 0;
        }
        e++;
    }

    /* Not found */
    return -1;
}

/*
 * Create the init process
 */
static proc_t *
_init_new(void)
{
    int ret;
    void *start;
    size_t size;
    proc_t *proc;
    size_t nr;
    struct arch_task *t;
    void *prog;

    /* Find init from initrd */
    ret = _initrd_find_file("init", &start, &size);
    if ( ret < 0 ) {
        return NULL;
    }
    proc = proc_new(1);
    if ( NULL == proc ) {
        return NULL;
    }
    g_kvar->procs[0] = proc;
    t = proc->task->arch;

    /* Switch the memory context */
    proc_use(proc);

    /* Calculate the number of pages required for the program */
    nr = (size + MEMORY_PAGESIZE -1) / MEMORY_PAGESIZE;

    /* Program */
    prog = (void *)PROC_PROG_ADDR;
    kmemcpy(prog, start, size);

    ret = task_init(proc->task, prog);
    if ( ret < 0 ) {
        return NULL;
    }
    t->cr3 = ((pgt_t *)proc->vmem->arch)->cr3;

    return proc;
}

/*
 * Create tasks
 */
static int
_prepare_multitasking(void)
{
    int ret;
    struct arch_cpu_data *cpu;
    void *start;
    size_t size;
    proc_t *proc;
    void *kstack;
    void *ustack;
    struct arch_task *taski;

    proc = _init_new();
    if ( NULL == proc ) {
        panic("Could not initialize the init process.");
    }

    /* Find init from initrd */
    ret = _initrd_find_file("init", &start, &size);
    if ( ret < 0 ) {
        panic("Could not find init.");
    }
    kprintf("Found init: %llx %lld\n", start, size);

    ret = kmem_slab_create_cache(ARCH_TASK_NAME, sizeof(struct arch_task));
    if ( ret < 0 ) {
        panic("Cannot create a slab for arch_task.");
    }

    /* Idle task */
    taski = kmalloc(sizeof(struct arch_task));
    if ( NULL == taski ) {
        return -1;
    }
    kstack = kmalloc(4096);
    if ( NULL == kstack ) {
        return -1;
    }
    ustack = kmalloc(4096);
    if ( NULL == ustack ) {
        return -1;
    }
    taski->rp = kstack + 4096 - 16 - sizeof(struct stackframe64);
    kmemset(taski->rp, 0, sizeof(struct stackframe64));
    taski->sp0 = (uint64_t)kstack + 4096 - 16;
    taski->rp->sp = (uint64_t)ustack + 4096 - 16;
    taski->rp->ip = (uint64_t)task_idle;
    taski->rp->cs = GDT_RING0_CODE_SEL;
    taski->rp->ss = GDT_RING0_DATA_SEL;
    taski->rp->fs = GDT_RING0_DATA_SEL;
    taski->rp->gs = GDT_RING0_DATA_SEL;
    taski->rp->flags = 0x202;
    taski->xregs = kmalloc(4096);
    if ( NULL == taski->xregs ) {
        return -1;
    }
    kmemset(taski->xregs, 0 , 4096);
    taski->cr3 = ((pgt_t *)g_kvar->mm.kmem.arch)->cr3;

    /* Set the task A as the initial task */
    cpu = (struct arch_cpu_data *)CPU_TASK(0);
    cpu->cur_task = NULL;
    cpu->next_task = proc->task->arch;
    cpu->idle_task = taski;
    cpu->fpu_task = NULL;

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
    wrmsr(MSR_IA32_FMASK, 0x0202);

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
    void *bstack;

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
    ret = kmem_slab_init();
    if ( ret < 0 ) {
        panic("Failed to initialize the slab allocator.");
    }

    /* Initialize kmmaloc */
    ret = kmalloc_init(&kvar->slab);
    if ( ret < 0 ) {
        panic("Failed to initialize the kmalloc slab.");
    }

    /* Initialize virtual memory */
    ret = vmem_callback_init();
    if ( ret < 0 ) {
        panic("Failed to initialize the virtual memory");
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

    /* Initialize the scheduler */
    g_kvar->runqueue = NULL;

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
    bstack = memory_alloc_pages(&kvar->mm, MAX_PROCESSORS,
                                MEMORY_ZONE_KERNEL, 0);
    if ( NULL == bstack ) {
        panic("Cannot allocate boot stack for application processors.");
    }
    kmemset(bstack, 0, MAX_PROCESSORS * MEMORY_PAGESIZE);
    *(volatile uintptr_t *)(APVAR_CR3 + KERNEL_LMAP)
        = ((arch_var_t *)kvar->arch)->pgt.cr3;
    *(volatile uintptr_t *)(APVAR_SP + KERNEL_LMAP) = (uintptr_t)bstack;

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
        kprintf("%016llx %016llx %08llx %08llx\r\n", ent->base, ent->len,
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
    void *kstack;
    void *ustack;

    /* Idle task */
    idle = kmalloc(sizeof(struct arch_task));
    if ( NULL == idle ) {
        return -1;
    }
    kstack = kmalloc(4096);
    if ( NULL == kstack ) {
        kfree(idle);
        return -1;
    }
    ustack = kmalloc(4096);
    if ( NULL == ustack ) {
        kfree(kstack);
        kfree(idle);
        return -1;
    }
    idle->rp = kstack + 4096 - 16 - sizeof(struct stackframe64);
    kmemset(idle->rp, 0, sizeof(struct stackframe64));
    idle->sp0 = (uint64_t)kstack + 4096 - 16;
    idle->rp->sp = (uint64_t)ustack + 4096 - 16;
    idle->rp->ip = (uint64_t)task_idle;
    idle->rp->cs = GDT_RING0_CODE_SEL;
    idle->rp->ss = GDT_RING0_DATA_SEL;
    idle->rp->fs = GDT_RING0_DATA_SEL;
    idle->rp->gs = GDT_RING0_DATA_SEL;
    idle->rp->flags = 0x202;
    idle->xregs = kmalloc(4096);
    if ( NULL == idle->xregs ) {
        return -1;
    }
    kmemset(idle->xregs, 0 , 4096);
    idle->cr3 = ((pgt_t *)g_kvar->mm.kmem.arch)->cr3;

    /* Set the task A as the initial task */
    cpu = (struct arch_cpu_data *)(uintptr_t)CPU_TASK(lapic_id);
    cpu->cur_task = NULL;
    cpu->next_task = idle;
    cpu->idle_task = idle;
    cpu->fpu_task = NULL;

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
 * hlt
 */
void
sys_hlt(void)
{
    task_t *t;
    /* Allowed only from idle tasks */
    t = this_task();
    if ( NULL != t->proc ) {
        return;
    }
    hlt();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
