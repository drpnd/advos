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
#include "kernel.h"

/*
 * Prototype declarations
 */
static void _add_block(phys_memory_buddy_page_t **, int, uintptr_t);
static void
_add_region_order(phys_memory_buddy_page_t **, int, uintptr_t, uintptr_t);
static void _split_buddy(phys_memory_buddy_page_t **, int);
static void _merge_buddy(phys_memory_buddy_page_t **, int);
static void _insert_buddy(phys_memory_buddy_page_t **, uintptr_t, int);

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
        if ( addr < (uintptr_t)(*cur) ) {
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
 * Split a block at the specified order into two
 */
static void
_split_buddy(phys_memory_buddy_page_t **buddy, int order)
{
    phys_memory_buddy_page_t *block;
    phys_memory_buddy_page_t *split;
    uintptr_t offset;

    if ( order >= MEMORY_PHYS_BUDDY_ORDER ) {
        return;
    }

    if ( NULL != buddy[order] ) {
        /* Valid block in this order */
        return;
    }

    /* Split the upper order if no block at the upper order */
    _split_buddy(buddy, order + 1);

    /* Check the upper order */
    if ( NULL == buddy[order + 1] ) {
        /* No valid block */
        return;
    }

    /* Take one block from the upper order */
    block = buddy[order + 1];
    buddy[order + 1] = buddy[order + 1]->next;
    block->next = NULL;

    /* Split the block into two */
    offset = MEMORY_PAGESIZE << order;
    split = (phys_memory_buddy_page_t *)((uintptr_t)block + offset);
    split->next = NULL;
    block->next = split;
    buddy[order] = block;
}

/*
 * Allocate pages
 */
void *
phys_mem_buddy_alloc(phys_memory_buddy_page_t **buddy, int order)
{
    phys_memory_buddy_page_t *block;

    /* Exceed the supported order */
    if ( order > MEMORY_PHYS_BUDDY_ORDER ) {
        return NULL;
    }

    /* Try to split the upper order if needed */
    _split_buddy(buddy, order);
    if ( NULL == buddy[order] ) {
        /* No block found */
        return 0;
    }
    block = buddy[order];
    buddy[order] = buddy[order]->next;
    block->next = NULL;

    return block;
}

/*
 * Merge buddy blocks to the upper order
 */
static void
_merge_buddy(phys_memory_buddy_page_t **buddy, int order)
{
    phys_memory_buddy_page_t **cur;
    uintptr_t block;
    uintptr_t blocksize;

    if ( order >= MEMORY_PHYS_BUDDY_ORDER ) {
        return;
    }

    blocksize = MEMORY_PAGESIZE << order;

    cur = &buddy[order];
    while ( *cur ) {
        block = (uintptr_t)*cur;
        if ( 0 == (block & ((blocksize << 1) - 1)) ) {
            /* is aligned for the upper order */
            if ( (uintptr_t)(*cur)->next == block + blocksize ) {
                /* is buddy */
                *cur = (*cur)->next->next;
                _insert_buddy(buddy, block, order + 1);
                /* May return here if we can ensure only one block could be
                   merged. */
                if ( NULL == *cur ) {
                    break;
                }
            }
        }
        cur = &(*cur)->next;
    }
}

/*
 * Insert the block to the buddy system
 */
static void
_insert_buddy(phys_memory_buddy_page_t **buddy, uintptr_t addr, int order)
{
    phys_memory_buddy_page_t *block;
    phys_memory_buddy_page_t **cur;

    if ( order > MEMORY_PHYS_BUDDY_ORDER ) {
        return;
    }

    /* Search the position to insert */
    cur = &buddy[order];
    while ( *cur ) {
        if ( addr < (uintptr_t)(*cur) ) {
            /* Insert here */
            break;
        }
        cur = &(*cur)->next;
    }

    /* Insert at the tail */
    block = (phys_memory_buddy_page_t *)addr;
    block->next = *cur;
    *cur = block;
    _merge_buddy(buddy, order);
}

/*
 * Release pages
 */
void
phys_mem_buddy_free(phys_memory_buddy_page_t **buddy, void *ptr, int order)
{
    _insert_buddy(buddy, (uintptr_t)ptr, order);
}

/*
 * Initialize the physical memory management region
 * FIXME: This function needs to check the duplicate memory region.
 */
int
phys_memory_init(phys_memory_t *mem, int nr, memory_sysmap_entry_t *map,
                 uintptr_t p2v)
{
    int i;
    uintptr_t base;
    uintptr_t next;
    uintptr_t kbase;

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
    mem->p2v = p2v;

    /* Mark that DMA and kernel zones are initialized */
    mem->czones[MEMORY_ZONE_DMA].valid = 1;
    mem->czones[MEMORY_ZONE_KERNEL].valid = 1;

    return 0;
}

/*
 * Initialize virtual memory
 */
memory_t *
memory_init(void)
{
    return NULL;
}

/*
 * Add a node
 */
static int
_atree_add(virt_memory_free_t **t, virt_memory_free_t *n)
{
    if ( NULL == *t ) {
        /* Insert here */
        *t = n;
        n->stree.left = NULL;
        n->stree.right = NULL;
        return 0;
    }
    if ( n->start == (*t)->start ) {
        return -1;
    }
    if ( n->start > (*t)->start ) {
        return _atree_add(&(*t)->atree.right, n);
    } else {
        return _atree_add(&(*t)->atree.left, n);
    }
}
static int
_stree_add(virt_memory_free_t **t, virt_memory_free_t *n)
{
    if ( NULL == *t ) {
        /* Insert here */
        *t = n;
        n->stree.left = NULL;
        n->stree.right = NULL;
        return 0;
    } else if ( (*t)->size == n->size ) {
        /* Insert here */
        n->stree.left = *t;
        n->stree.right = NULL;
        *t = n;
        return 0;
    }
    if ( n->size > (*t)->size ) {
        return _stree_add(&(*t)->stree.right, n);
    } else {
        return _stree_add(&(*t)->stree.left, n);
    }
}
static int
_add(virt_memory_free_t *t, virt_memory_free_t *n)
{
    int ret;

    ret = _atree_add(&t, n);
    if ( ret < 0 ) {
        return -1;
    }
    ret = _stree_add(&t, n);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Delete the node
 */
static virt_memory_free_t *
_atree_delete(virt_memory_free_t *t, virt_memory_free_t *n)
{
    if ( NULL == t ) {
        return NULL;
    }
    if ( n == t ) {
        t->atree.left = n->atree.left;
        t->atree.right = n->atree.left;
        return t;
    }
    if ( n->start > t->start ) {
        return _atree_delete(t->atree.right, n);
    } else {
        return _atree_delete(t->atree.left, n);
    }
}
static virt_memory_free_t *
_stree_delete(virt_memory_free_t *t, virt_memory_free_t *n)
{
    if ( NULL == t ) {
        return NULL;
    }
    if ( n == t ) {
        t->stree.left = n->stree.left;
        t->stree.right = n->stree.left;
        return t;
    }
    if ( n->size > t->size ) {
        return _stree_delete(t->stree.right, n);
    } else {
        return _stree_delete(t->stree.left, n);
    }
}
static virt_memory_free_t *
_delete(virt_memory_free_t *t, virt_memory_free_t *n)
{
    virt_memory_free_t *da;
    virt_memory_free_t *ds;

    da = _atree_delete(t, n);
    ds = _stree_delete(t, n);
    kassert(da == ds);

    return n;
}

/*
 * Search the tree by size
 */
static virt_memory_free_t *
_search_fit_size(virt_memory_block_t *block, virt_memory_free_t *t, size_t sz)
{
    virt_memory_free_t *s;

    if ( NULL == t ) {
        return NULL;
    }
    if ( sz > t->size ) {
        /* Search the right subtree */
        return _search_fit_size(block, t->stree.right, sz);
    } else {
        /* Search the left subtree */
        s = _search_fit_size(block, t->stree.left, sz);
        if ( NULL == s ) {
            /* Not found any block that fits this size, then return this node */
            return t;
        }
        return s;
    }
}

/*
 * Allocate pages from the block
 */
static page_t *
_alloc_pages_block(memory_t *mem, virt_memory_block_t *block, size_t nr)
{
    virt_memory_free_t *n;
    page_t *page;

    /* Search from the binary tree */
    n = _search_fit_size(block, block->frees, nr * MEMORY_PAGESIZE);
    if ( NULL == n ) {
        /* No available space */
        return NULL;
    }


    return NULL;
}

/*
 * Allocate pages
 */
page_t *
memory_alloc_pages(memory_t *mem, size_t nr)
{
    virt_memory_block_t *block;
    page_t *page;

    block = mem->blocks;
    page = NULL;
    while ( NULL != block && NULL == page ) {
        page = _alloc_pages_block(mem, block, nr);
    }

    return page;
}


/*
 * Add a new memory block
 */
int
memory_block_add(memory_t *mem, virt_memory_block_t *n)
{
    virt_memory_block_t **b;

    b = &mem->blocks;
    while ( NULL != *b ) {
        if ( (*b)->start > n->start ) {
            /* Try to insert here */
            if ( n->start + n->length > (*b)->start ) {
                /* Overlapping space */
                return -1;
            }
            break;
        }
        b = &(*b)->next;
    }
    n->next = *b;
    *b = n;

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
