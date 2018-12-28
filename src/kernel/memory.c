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

static virt_memory_free_t *
_atree_delete(virt_memory_free_t **, virt_memory_free_t *);

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
int
memory_init(memory_t *mem, phys_memory_t *phys, void *arch,
            int (*map)(void *, uintptr_t, page_t *),
            int (*unmap)(void *, uintptr_t, page_t *))
{
    union virt_memory_data *data;
    size_t nr;
    size_t i;

    mem->phys = phys;
    mem->blocks = NULL;
    mem->lists = NULL;

    /* Allocate 2 MiB for page management */
    data = phys_mem_buddy_alloc(phys->czones[MEMORY_ZONE_KERNEL].heads, 9);
    if ( NULL == data ) {
        return -1;
    }
    nr = (MEMORY_PAGESIZE << 9) / sizeof(union virt_memory_data);
    for ( i = 1; i < nr; i++ ){
        data[i - 1].next = &data[i];
    }
    data[nr - 1].next = NULL;
    mem->lists = data;

    /* Architecture specific data structure */
    mem->arch = arch;
    mem->map = map;
    mem->unmap = unmap;

    return 0;
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
_add(virt_memory_free_t **t, virt_memory_free_t *n)
{
    int ret;

    ret = _atree_add(t, n);
    if ( ret < 0 ) {
        return -1;
    }
    _atree_delete(t, n);
    ret = _stree_add(t, n);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Delete the node
 */
static virt_memory_free_t *
_atree_delete(virt_memory_free_t **t, virt_memory_free_t *n)
{
    virt_memory_free_t **x;

    if ( NULL == *t ) {
        return NULL;
    }
    if ( n == *t ) {
        if ( n->atree.left && n->atree.right ) {
            *t = n->atree.left;
            x = &n->atree.left;
            while ( NULL != *x ) {
                x = &(*x)->atree.right;
            }
            *x = n->atree.right;
        } else if ( n->atree.left ) {
            *t = n->atree.left;
        } else if ( n->atree.right ) {
            *t = n->atree.right;
        } else {
            *t = NULL;
        }
        return n;
    }
    if ( n->start > (*t)->start ) {
        return _atree_delete(&(*t)->atree.right, n);
    } else {
        return _atree_delete(&(*t)->atree.left, n);
    }
}
static virt_memory_free_t *
_stree_delete(virt_memory_free_t **t, virt_memory_free_t *n)
{
    virt_memory_free_t **x;

    if ( NULL == *t ) {
        return NULL;
    }
    if ( n == *t ) {
        if ( n->stree.left && n->stree.right ) {
            *t = n->stree.left;
            x = &n->stree.left;
            while ( NULL != *x ) {
                x = &(*x)->stree.right;
            }
            *x = n->stree.right;
        } else if ( n->stree.left ) {
            *t = n->stree.left;
        } else if ( n->stree.right ) {
            *t = n->stree.right;
        } else {
            *t = NULL;
        }
        return n;
    }
    if ( n->start > (*t)->start ) {
        return _stree_delete(&(*t)->stree.right, n);
    } else {
        return _stree_delete(&(*t)->stree.left, n);
    }
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
 * Allocate virtual memory data
 */
static union virt_memory_data *
_alloc_data(memory_t *mem)
{
    union virt_memory_data *data;

    if ( NULL == mem->lists ) {
        return NULL;
    }
    data = mem->lists;
    mem->lists = data->next;
    return data;
}

/*
 * Free virtual memory data
 */
static void
_free_data(memory_t *mem, union virt_memory_data *data)
{
    data->next = mem->lists;
    mem->lists = data;
}


/*
 * Add an entry
 */
static int
_entry_add(virt_memory_entry_t **t, virt_memory_entry_t *n)
{
    if ( NULL == *t ) {
        /* Insert here */
        *t = n;
        n->atree.left = NULL;
        n->atree.right = NULL;
        return 0;
    }
    if ( n->start == (*t)->start ) {
        return -1;
    }
    if ( n->start > (*t)->start ) {
        return _entry_add(&(*t)->atree.right, n);
    } else {
        return _entry_add(&(*t)->atree.left, n);
    }
}

/*
 * Allocate pages from the block
 */
static void *
_alloc_pages_block(memory_t *mem, virt_memory_block_t *block, size_t nr)
{
    int superpage;
    size_t size;
    virt_memory_free_t *n;
    virt_memory_free_t *f0;
    virt_memory_free_t *f1;
    virt_memory_entry_t *e;
    virt_memory_object_t *obj;
    page_t **pp;
    page_t *p;
    void *r;
    size_t i;

    /* Search from the binary tree */
    size = nr * MEMORY_PAGESIZE;
    superpage = 0;
    if ( size >= MEMORY_SUPERPAGESIZE ) {
        /* Search larger size to make sure to align superpages */
        size += MEMORY_SUPERPAGESIZE;
        superpage = 1;
    }
    n = _search_fit_size(block, block->frees.stree, size);
    if ( NULL == n ) {
        /* No available space */
        return NULL;
    }
    r = _stree_delete(&block->frees.stree, n);
    if ( NULL == r ) {
        return NULL;
    }
    r = _atree_delete(&block->frees.atree, n);
    if ( NULL == r ) {
        _stree_add(&block->frees.stree, n);
        return NULL;
    }

    /* Allocate an entry */
    e = (virt_memory_entry_t *)_alloc_data(mem);
    if ( NULL == e ) {
        _atree_add(&block->frees.atree, n);
        _stree_add(&block->frees.stree, n);
        return NULL;
    }
    kmemset(e, 0, sizeof(virt_memory_entry_t));

    /* Prepare for free spaces */
    f0 = (virt_memory_free_t *)_alloc_data(mem);
    if ( NULL == f0 ) {
        _free_data(mem, (union virt_memory_data *)e);
        _atree_add(&block->frees.atree, n);
        _stree_add(&block->frees.stree, n);
        return NULL;
    }
    kmemset(f0, 0, sizeof(virt_memory_free_t));
    f1 = (virt_memory_free_t *)_alloc_data(mem);
    if ( NULL == f1 ) {
        _free_data(mem, (union virt_memory_data *)f0);
        _free_data(mem, (union virt_memory_data *)e);
        _atree_add(&block->frees.atree, n);
        _stree_add(&block->frees.stree, n);
        return NULL;
    }
    kmemset(f1, 0, sizeof(virt_memory_free_t));

    /* Map virtual page */
    if ( superpage ) {
        if ( n->start & (MEMORY_SUPERPAGESIZE - 1) ) {
            /* Not aligned to superpage */
            e->start = (n->start + (MEMORY_SUPERPAGESIZE - 1))
                & ~(uintptr_t)(MEMORY_SUPERPAGESIZE - 1);
            e->size = nr * MEMORY_PAGESIZE;

            f0->start = n->start;
            f0->size = e->start - n->start;
            _atree_add(&block->frees.atree, f0);
            _stree_add(&block->frees.stree, f0);
        } else {
            /* Aligned to superpage */
            e->start = n->start;
            e->size = nr * MEMORY_PAGESIZE;
            _free_data(mem, (union virt_memory_data *)f0);
        }
        if ( n->start + n->size != e->start + e->size ) {
            f1->start = e->start + e->size;
            f1->size = n->start + n->size - (e->start + e->size);
            _atree_add(&block->frees.atree, f1);
            _stree_add(&block->frees.stree, f1);
        } else {
            _free_data(mem, (union virt_memory_data *)f1);
        }
    } else {
        e->start = n->start;
        e->size = nr * MEMORY_PAGESIZE;
        if ( n->start + n->size != e->start + e->size ) {
            f1->start = e->start + e->size;
            f1->size = n->start + n->size - (e->start + e->size);
            _atree_add(&block->frees.atree, f1);
            _stree_add(&block->frees.stree, f1);
        } else {
            _free_data(mem, (union virt_memory_data *)f1);
        }
    }

    /* Allocate an object */
    obj = (virt_memory_object_t *)_alloc_data(mem);
    kmemset(obj, 0, sizeof(virt_memory_object_t));
    obj->size = nr * MEMORY_PAGESIZE;
    pp = &obj->pages;

    /* Allocate superpages */
    for ( i = 0; i < nr;
          i += (1 << (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT)) ) {
        p = (page_t *)_alloc_data(mem);
        kmemset(p, 0, sizeof(page_t));
        p->zone = MEMORY_ZONE_KERNEL; /* FIXME */
        p->numadomain = 0;  /* FIXME */
        p->order = MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT;

        r = phys_mem_buddy_alloc(mem->phys->czones[MEMORY_ZONE_KERNEL].heads,
                                 p->order);
        if ( NULL == r ) {
            /* FIXME */
            return NULL;
        }
        p->physical = (uintptr_t)r;
        /* Map */
        mem->map(mem->arch, e->start + i * MEMORY_PAGESIZE, p);

        *pp = p;
        pp = &p->next;
    }

    /* Allocate pages */
    for ( ; i < nr; i++ ) {
        p = (page_t *)_alloc_data(mem);
        kmemset(p, 0, sizeof(page_t));
        p->zone = MEMORY_ZONE_KERNEL; /* FIXME */
        p->numadomain = 0;  /* FIXME */
        p->order = 0;

        r = phys_mem_buddy_alloc(mem->phys->czones[MEMORY_ZONE_KERNEL].heads,
                                 p->order);
        if ( NULL == r ) {
            /* FIXME */
            return NULL;
        }
        p->physical = (uintptr_t)r;
        /* Map */
        mem->map(mem->arch, e->start + i * MEMORY_PAGESIZE, p);

        *pp = p;
        pp = &p->next;
    }

    e->object = obj;
    _entry_add(&block->entries, e);

    return (void *)e->start;
}

#if 0
/*
 * Add a new memory block
 */
int
memory_block_add(memory_t *mem, uintptr_t start, size_t length)
{
    union virt_memory_data *data;
    union virt_memory_data *fr;
    virt_memory_block_t *n;
    virt_memory_block_t **b;

    /* Allocate data and initialize the block */
    data = _alloc_data(mem);
    if ( NULL == data ) {
        return -1;
    }
    n = &data->block;
    n->start = start;
    n->length = length;
    n->next = NULL;
    n->entries = NULL;
    n->frees.atree = NULL;
    n->frees.stree = NULL;

    /* Add a free entry */
    fr = _alloc_data(mem);
    if ( NULL == fr ) {
        _free_data(mem, data);
        return -1;
    }
    kmemset(fr, 0, sizeof(union virt_memory_data));
    fr->free.start = start;
    fr->free.size = length;
    n->frees.atree = &fr->free;
    n->frees.stree = &fr->free;

    /* Insert the block */
    b = &mem->blocks;
    while ( NULL != *b ) {
        if ( (*b)->start > n->start ) {
            /* Try to insert here */
            if ( n->start + n->length > (*b)->start ) {
                /* Overlapping space */
                _free_data(mem, fr);
                _free_data(mem, data);
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
#endif

/*
 * Allocate pages
 */
void *
memory_alloc_pages(memory_t *mem, size_t nr)
{
    virt_memory_block_t *block;
    void *ptr;

    block = mem->blocks;
    ptr = NULL;
    while ( NULL != block && NULL == ptr ) {
        ptr = _alloc_pages_block(mem, block, nr);
    }

    return ptr;
}

/*
 * Find the entry corresponding to the specified address
 */
static virt_memory_entry_t *
_find_entry(virt_memory_entry_t *e, uintptr_t addr)
{
    if ( NULL == e ) {
        return NULL;
    }
    if ( addr >= e->start && addr < e->start + e->size ) {
        /* Found */
        return e;
    }
    if ( addr < e->start ) {
        /* Search the left */
        return _find_entry(e->atree.left, addr);
    } else {
        /* Search the right */
        return _find_entry(e->atree.right, addr);
    }
}

/*
 * Free pages from the list of the specified page list
 */
static void
_free_pages(memory_t *mem, page_t *page)
{
    page_t *p;

    p = page;
    while ( NULL != p ) {
        /* Free physical pages */
        if ( MEMORY_ZONE_DMA == p->zone ) {
            phys_mem_buddy_free(mem->phys->czones[MEMORY_ZONE_DMA].heads,
                                (void *)p->physical, p->order);
        } else if ( MEMORY_ZONE_KERNEL == p->zone ) {
            phys_mem_buddy_free(mem->phys->czones[MEMORY_ZONE_KERNEL].heads,
                                (void *)p->physical, p->order);
        } else if ( MEMORY_ZONE_NUMA_AWARE == p->zone ) {
            phys_mem_buddy_free(mem->phys->numazones[p->numadomain].heads,
                                (void *)p->physical, p->order);
        } else {
            /* Do nothing */
        }
        /* Free this page */
        _free_data(mem, (union virt_memory_data *)page);

        p = p->next;
    }
}

/*
 * Find a free entry neighboring to the start and the end
 */
static virt_memory_free_t *
_find_free_entry_by_addr(virt_memory_free_t *e, uintptr_t start, uintptr_t end)
{
    if ( NULL == e ) {
        return NULL;
    }
    if ( end == e->start || start == e->start + e->size ) {
        /* Found */
        return e;
    }
    if ( start >= e->start && start < e->start + e->size ) {
        /* Overlapping region */
        return NULL;
    } else if ( start < e->start ) {
        /* Search the left */
        return _find_free_entry_by_addr(e->atree.left, start, end);
    } else {
        /* Search the right */
        return _find_free_entry_by_addr(e->atree.right, start, end);
    }
}

/*
 * Free an entry
 */
static int
_free_entry(memory_t *mem, virt_memory_block_t *b, virt_memory_entry_t *e)
{
    virt_memory_free_t free;
    virt_memory_free_t *f;
    void *r;
    int ret;

    /* Find a neighboring free entry by address */
    f = _find_free_entry_by_addr(b->frees.atree, e->start, e->start + e->size);
    if ( NULL == f ) {
        /* Not found, then convert the data structure to free entry */
        kmemset(&free, 0, sizeof(virt_memory_free_t));
        free.start = e->start;
        free.size = e->size;
        f = (virt_memory_free_t *)e;
        kmemcpy(f, &free, sizeof(virt_memory_free_t));

        /* Add this node to the free entry tree */
        ret = _stree_add(&b->frees.stree, f);
        if ( ret < 0 ) {
            return -1;
        }
    } else {
        /* Expand the free region */
        if ( f->start == e->start + e->size ) {
            f->start = e->start;
        } else {
            f->size = f->size + e->size;
        }
        _free_data(mem, (union virt_memory_data*)e);

        /* Rebalance the size-based tree */
        r = _stree_delete(&b->frees.stree, f);
        if ( NULL == r ) {
            return -1;
        }
        ret = _stree_add(&b->frees.stree, f);
        if ( ret < 0 ) {
            return -1;
        }
    }

    return 0;
}

/*
 * Free pages
 */
void
memory_free_pages(memory_t *mem, void *ptr)
{
    virt_memory_block_t *b;
    uintptr_t addr;
    virt_memory_entry_t *e;
    page_t *page;

    /* Convert the pointer to the address in integer */
    addr = (uintptr_t)ptr;

    /* Find a block */
    b = mem->blocks;
    while ( NULL != b ) {
        if ( addr >= b->start && addr < b->start + b->length ) {
            /* Found */
            break;
        }
        b = b->next;
    }
    if ( NULL == b ) {
        /* Not found */
        return;
    }

    /* Find an entriy corresponding to the virtual address */
    e = _find_entry(b->entries, addr);
    if ( NULL == e ) {
        /* Not found */
        return;
    }
    if ( addr != e->start ) {
        /* Start address does not match. */
        return;
    }

    /* Find pages from the object */
    page = e->object->pages;
    /* Free the corersponding pages */
    _free_pages(mem, page);
    /* Free the object */
    _free_data(mem, (union virt_memory_data *)e->object);
    /* Retuurn to the free entry */
    _free_entry(mem, b, e);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
