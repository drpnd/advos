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
#include "memory.h"

/*
 * System memory map entry
 */
typedef struct {
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t attr;
} __attribute__ ((packed)) memory_e820_entry_t;

#define MEMORY_E820_USABLE              1
#define MEMORY_E820_RESERVED            2
#define MEMORY_E820_ACPI_RECLAIMABLE    3
#define MEMORY_E820_ACPI_NVS            4
#define MEMORY_E820_BAD                 5

/*
 * Physical address to zone
 */
int
memory_phys_to_zone(uintptr_t addr)
{
    if ( addr < MEMORY_ZONE_KERNEL_LB ) {
        /* Below 16 MiB (defined in "memory.h") */
        return MEMORY_ZONE_DMA;
    } else if ( addr < MEMORY_ZONE_NUMA_AWARE_LB ) {
        /* Below 64 MiB (defined in "memory.h") */
        return MEMORY_ZONE_KERNEL;
    } else {
        /* NUMA-aware region */
        return MEMORY_ZONE_NUMA_AWARE;
    }
}


/*
 * Buddy system
 */
int
memory_buddy_alloc(int order)
{

    return 0;
}


static int
_init_e820_entry(uintptr_t base, uint64_t length)
{
    uintptr_t next;
    uintptr_t kbase;
    int nr;

    next = base + length;

    /* Page align */
    base = (base + 0x1fffff) & ~0x1fffffULL;
    next = next & ~0x1fffffULL;

    if ( base >= MEMORY_ZONE_NUMA_AWARE_LB ) {
        /* No pages in the DMA nor kerne zones */
        return 0;
    }

    /* Ignore the NUMA-aware zone */
    if ( next > MEMORY_ZONE_NUMA_AWARE_LB ) {
        next = MEMORY_ZONE_NUMA_AWARE_LB;
    }
    if ( next > MEMORY_ZONE_KERNEL_LB ) {
        /* At least one page for the kernel zone */
        kbase = MEMORY_ZONE_KERNEL_LB;

        /* Calculate the number of pages in the kernel zone */
        nr = (next - kbase) / 0x200000ULL;

        //print_hex(0xb8000 + 80 * 2 * 10, kbase, 8);
        //print_hex(0xb8000 + 80 * 2 * 10 + 34, next, 8);
    } else {
        kbase = next;
    }
    if ( base < MEMORY_ZONE_KERNEL_LB ) {
        /* At least one page for the DMA zone */

        /* Calculate the number of pages in the DMA zone */
        nr = (kbase - base) / 0x200000ULL;

        //print_hex(0xb8000 + 80 * 2 * 11, base, 8);
        //print_hex(0xb8000 + 80 * 2 * 11 + 34, kbase, 8);
    }

    return 0;
}


/*
 * Initialize the non NUMA aware region (DMA and kernel) with BIOS-e820
 */
int
memory_init_e820(void)
{
    memory_e820_entry_t *table;
    int nr;
    int i;
    int ret;

    nr = *(uint16_t *)BI_MM_NENT_ADDR;
    table = (memory_e820_entry_t *)BI_MM_TABLE_ADDR;

    for ( i = 0; i < nr; i++ ) {
        if ( MEMORY_E820_USABLE != table[i].type ) {
            /* Not usable region */
            continue;
        }
        ret = _init_e820_entry(table[i].base, table[i].len);
    }

    return 0;
}


int
init_e820(void)
{
    


    return 0;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
