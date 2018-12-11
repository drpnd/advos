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

#include "memory.h"

/* Defined in arch/x86_64/asm.S */
void * kmemset(void *b, int c, uint64_t len);
#define NULL    (void *)0

/*
 * Add a block to the specified order
 */
static void
_add_block(phys_memory_buddy_page_t **buddy, int order, uintptr_t addr)
{
    phys_memory_buddy_page_t *page;
    phys_memory_buddy_page_t **cur;

    page = (phys_memory_buddy_page_t *)addr;
    page->next = NULL;

    /* Keep the linked list sorted */
    cur = &buddy[order];
    while ( *cur ) {
        if ( addr < (uint64_t)(*cur) ) {
            /* Insert here */
            page->next = *cur;
            *cur = page;
            return;
        }

        cur = &(*cur)->next;
    }

    /* If reaches at the end of the list */
    *cur = page;
}

/*
 * Try to add a memory region to the buddy system at the specified order
 */
static void
_add_region_order(phys_memory_buddy_page_t **buddy, int order,
                  uintptr_t base, uintptr_t next)
{
    uint64_t blocksize;
    uintptr_t base_aligned;
    uintptr_t next_aligned;
    uintptr_t ptr;
    int i;
    int nr;

    /* May happen if base or next is not 4 KiB-alinged */
    if ( order < 0 ) {
        return;
    }

    /* The size of the block for this order */
    blocksize = (1ULL << order) * MEMORY_PAGESIZE;

    /* Align the base and next addresses */
    base_aligned = (base + (blocksize - 1)) & ~(blocksize - 1);
    next_aligned = next & ~(blocksize - 1);

    /* Add smaller chunk to the lower order of the buddy system */
    if ( base != base_aligned ) {
        ptr = base_aligned < next ? base_aligned : next;
        _add_region_order(buddy, order - 1, base, ptr);
    }
    if ( next != next_aligned && next_aligned >= base ) {
        ptr = next_aligned > base ? next_aligned : base;
        _add_region_order(buddy, order - 1, ptr, next);
    }

    /* Add pages to this zone */
    nr = ((next_aligned - base_aligned) >> order) / MEMORY_PAGESIZE;
    for ( i = 0; i < nr; i++ ) {
        _add_block(buddy, order, base_aligned + i * blocksize);
    }
}

/*
 * Add a memory region to the buddy system
 */
void
phys_mem_buddy_add_region(phys_memory_buddy_page_t **buddy, uintptr_t base,
                          uintptr_t next)
{
    _add_region_order(buddy, MEMORY_PHYS_BUDDY_ORDER, base, next);
}

/*
 * Initialize the physical memory management region
 */
int
phys_memory_init(phys_memory_t *mem, int nr, memory_sysmap_entry_t *map,
                 uint64_t p2v)
{
    int i;
    uint64_t base;
    uint64_t next;
    uint64_t kbase;

    /* Initialize the region */
    kmemset(mem, 0, sizeof(phys_memory_t));

    /* The following routine initializes the buddy system of the physical memory
       according to the memory zone. */
    for ( i = 0; i < nr; i++ ) {
        /* Base address and the address of the next block (base + length) */
        base = map[i].base;
        next = base + map[i].len;

        /* First 2 MiB is for boot loader and kernel */
        if ( base < 0x00200000 ) {
            base = 0x00200000;
        }
        if ( next < 0x00200000 ) {
            next = 0x00200000;
        }

        /* Round up for 4 KiB page alignment */
        base = (base + (MEMORY_PAGESIZE - 1)) & ~(MEMORY_PAGESIZE - 1);
        next = next & ~(MEMORY_PAGESIZE - 1);

        /* Resolve the zone */
        if ( base >= MEMORY_ZONE_NUMA_AWARE_LB ) {
            /* No pages in the DMA nor kerne zones in this entry */
            continue;
        }

        /* Ignore the NUMA-aware zone */
        if ( next > MEMORY_ZONE_NUMA_AWARE_LB ) {
            next = MEMORY_ZONE_NUMA_AWARE_LB;
        }
        if ( next > MEMORY_ZONE_KERNEL_LB ) {
            /* At least one page for the kernel zone */
            kbase = MEMORY_ZONE_KERNEL_LB;

            /* Add this region to the buddy system */
            phys_mem_buddy_add_region(mem->czones[MEMORY_ZONE_KERNEL].heads,
                                      base + p2v, next + p2v);
        } else {
            kbase = next;
        }
        if ( base < MEMORY_ZONE_KERNEL_LB ) {
            /* At least one page for the DMA zone, then add this region to the
               buddy system */
            phys_mem_buddy_add_region(mem->czones[MEMORY_ZONE_DMA].heads,
                                      base + p2v, next + p2v);
        }
    }

    /* Set the p2v offset */
    mem->czones[MEMORY_ZONE_DMA].p2v = p2v;
    mem->czones[MEMORY_ZONE_KERNEL].p2v = p2v;

    /* Mark that DMA and kernel zones are initialized */
    mem->czones[MEMORY_ZONE_DMA].valid = 1;
    mem->czones[MEMORY_ZONE_KERNEL].valid = 1;

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
