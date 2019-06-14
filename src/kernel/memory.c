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

#include "memory.h"
#include "kernel.h"

/*
 * Prototype declarations
 */
static virt_memory_block_t * _find_block(virt_memory_t *, uintptr_t);
static int _free_add(virt_memory_block_t *, virt_memory_free_t *);
static virt_memory_free_t *
_free_atree_delete(virt_memory_free_t **, virt_memory_free_t *);
static virt_memory_free_t *
_free_delete(virt_memory_block_t *, virt_memory_free_t *);
static virt_memory_free_t *
_search_fit_size(virt_memory_block_t *, virt_memory_free_t *, size_t);
static void *
_alloc_pages_block(virt_memory_t *, virt_memory_block_t *, size_t, int, int);

/*
 * Initialize virtual memory
 */
int
memory_init(memory_t *mem, phys_memory_t *phys, void *arch, uintptr_t p2v,
            memory_arch_interfaces_t *ifs)
{
    int ret;

    /* Initialize the kernel memory */
    ret = kmem_init(&mem->kmem, phys, p2v);
    if ( ret < 0 ) {
        return -1;
    }

    mem->phys = phys;
    mem->kmem.blocks = NULL;
    mem->kmem.mem = mem;

    /* Architecture specific data structure */
    mem->kmem.arch = arch;
    mem->ifs.map = ifs->map;
    mem->ifs.unmap = ifs->unmap;
    mem->ifs.fork = ifs->fork;
    mem->ifs.ctxsw = ifs->ctxsw;

    return 0;
}

/*
 * Insert a memory block to the specified virtual memory
 */
static int
_block_insert(virt_memory_t *vmem, virt_memory_block_t *n)
{
    virt_memory_block_t **b;

    /* Insert the block to the sorted list */
    b = &vmem->blocks;
    while ( NULL != *b ) {
        if ( (*b)->start > n->start ) {
            /* Try to insert here */
            if ( n->end >= (*b)->start ) {
                /* Overlapping space, then raise an error */
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
 * Find a memory block including the specified address
 */
static virt_memory_block_t *
_find_block(virt_memory_t *vmem, uintptr_t addr)
{
    virt_memory_block_t *b;

    /* Search a block */
    b = vmem->blocks;
    while ( NULL != b ) {
        if ( addr >= b->start && addr <= b->end ) {
            /* Found */
            break;
        }
        b = b->next;
    }
    return b;
}

/*
 * Find a free entry including to the start
 */
static virt_memory_free_t *
_find_free_entry(virt_memory_free_t *e, uintptr_t addr)
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
        return _find_free_entry(e->atree.left, addr);
    } else {
        /* Search the right */
        return _find_free_entry(e->atree.right, addr);
    }
}

/*
 * Find a free entry neighboring to the start and the end
 */
static virt_memory_free_t *
_find_neighbor_free_entry(virt_memory_free_t *e, uintptr_t start, uintptr_t end)
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
        return _find_neighbor_free_entry(e->atree.left, start, end);
    } else {
        /* Search the right */
        return _find_neighbor_free_entry(e->atree.right, start, end);
    }
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
 * Delete an entry
 */
static virt_memory_entry_t *
_entry_delete(virt_memory_entry_t **t, virt_memory_entry_t *n)
{
    virt_memory_entry_t **x;

    if ( NULL == *t ) {
        /* Not found */
        return NULL;
    }
    if ( n == (*t) ) {
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
        return _entry_delete(&(*t)->atree.right, n);
    } else {
        return _entry_delete(&(*t)->atree.left, n);
    }
}

/*
 * Calculate the page order
 */
static int
_order(uintptr_t addr1, uintptr_t addr2, size_t size)
{
    uintptr_t p1;
    uintptr_t p2;
    int order;

    order = 0;
    p1 = addr1 >> MEMORY_PAGESIZE_SHIFT;
    p2 = addr2 >> MEMORY_PAGESIZE_SHIFT;
    for ( order = 0; ; order++ ) {
        if ( p1 & ((2ULL << order) - 1) ) {
            break;
        }
        if ( p2 & ((2ULL << order) - 1) ) {
            break;
        }
        /* Check the size */
        if ( (2ULL << (MEMORY_PAGESIZE_SHIFT + order)) > size ) {
            /* Exceed the size for order + 1, then terminate here */
            break;
        }
    }

    return order;
}

/*
 * Add a node to the free entry tree
 */
static int
_free_atree_add(virt_memory_free_t **t, virt_memory_free_t *n)
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
        return _free_atree_add(&(*t)->atree.right, n);
    } else {
        return _free_atree_add(&(*t)->atree.left, n);
    }
}
static int
_free_stree_add(virt_memory_free_t **t, virt_memory_free_t *n)
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
        return _free_stree_add(&(*t)->stree.right, n);
    } else {
        return _free_stree_add(&(*t)->stree.left, n);
    }
}
static int
_free_add(virt_memory_block_t *b, virt_memory_free_t *n)
{
    int ret;
    virt_memory_free_t *p;

    ret = _free_atree_add(&b->frees.atree, n);
    if ( ret < 0 ) {
        return -1;
    }
    ret = _free_stree_add(&b->frees.stree, n);
    if ( ret < 0 ) {
        p = _free_atree_delete(&b->frees.atree, n);
        kassert( p != NULL );
        return -1;
    }

    return 0;
}

/*
 * Delete the specified node from the free entry tree
 */
static virt_memory_free_t *
_free_atree_delete(virt_memory_free_t **t, virt_memory_free_t *n)
{
    virt_memory_free_t **x;

    if ( NULL == *t ) {
        /* Not found */
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
        return _free_atree_delete(&(*t)->atree.right, n);
    } else {
        return _free_atree_delete(&(*t)->atree.left, n);
    }
}
static virt_memory_free_t *
_free_stree_delete(virt_memory_free_t **t, virt_memory_free_t *n)
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
        return _free_stree_delete(&(*t)->stree.right, n);
    } else {
        return _free_stree_delete(&(*t)->stree.left, n);
    }
}
static virt_memory_free_t *
_free_delete(virt_memory_block_t *b, virt_memory_free_t *n)
{
    virt_memory_free_t *fa;
    virt_memory_free_t *fs;

    fa = _free_atree_delete(&b->frees.atree, n);
    fs = _free_stree_delete(&b->frees.stree, n);
    kassert( fa == fs );

    return fa;
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
static void *
_alloc_pages_block(virt_memory_t *vmem, virt_memory_block_t *block, size_t nr,
                   int zone, int numadomain)
{
    int superpage;
    size_t size;
    virt_memory_free_t *f;
    virt_memory_free_t *f0;
    virt_memory_free_t *f1;
    virt_memory_entry_t *e;
    page_t **pp;
    page_t *p;
    void *r;
    size_t i;
    int ret;
    uintptr_t virtual;

    /* Search from the binary tree */
    size = nr * MEMORY_PAGESIZE;
    superpage = 0;
    if ( size >= MEMORY_SUPERPAGESIZE ) {
        /* Search larger size to make sure to align superpages */
        size += MEMORY_SUPERPAGESIZE;
        superpage = 1;
    }
    f = _search_fit_size(block, block->frees.stree, size);
    if ( NULL == f ) {
        /* No available space */
        return NULL;
    }

    /* Allocate an entry and an object */
    e = (virt_memory_entry_t *)vmem->allocator.alloc(vmem);
    if ( NULL == e ) {
        goto error_entry;
    }
    e->size = nr * MEMORY_PAGESIZE;
    e->flags = MEMORY_VMF_RW;
    e->object = (virt_memory_object_t *)vmem->allocator.alloc(vmem);
    if ( NULL == e->object ) {
        goto error_obj;
    }
    e->object->type = MEMORY_OBJECT;
    e->object->size = nr * MEMORY_PAGESIZE;
    e->object->pages = NULL;
    e->object->refs = 1;

    /* Prepare for free spaces */
    f0 = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == f0 ) {
        goto error_f0;
    }
    f1 = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == f1 ) {
        goto error_f1;
    }

    /* Align for superpages */
    if ( superpage ) {
        if ( f->start & (MEMORY_SUPERPAGESIZE - 1) ) {
            /* Not aligned to superpage, then align */
            e->start = (f->start + (MEMORY_SUPERPAGESIZE - 1))
                & ~(uintptr_t)(MEMORY_SUPERPAGESIZE - 1);
        } else {
            /* Aligned to superpage */
            e->start = f->start;
        }
    } else {
        e->start = f->start;
    }

    /* Allocate and map superpages */
    pp = &e->object->pages;
    for ( i = 0;
          i + (1 << (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT)) <= nr;
          i += (1 << (MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT)) ) {
        p = (page_t *)vmem->allocator.alloc(vmem);
        if ( NULL == p ) {
            goto error_page;
        }
        p->index = i;
        p->zone = zone;
        p->numadomain = numadomain;
        p->flags = 0;
        if ( e->flags & MEMORY_VMF_RW ) {
            p->flags |= MEMORY_PGF_RW;
        }
        p->order = MEMORY_SUPERPAGESIZE_SHIFT - MEMORY_PAGESIZE_SHIFT;
        p->next = NULL;

        /* Allocate a physical superpage */
        r = phys_mem_alloc(vmem->mem->phys, p->order, p->zone, p->numadomain);
        if ( NULL == r ) {
            vmem->allocator.free(vmem, (void *)p);
            goto error_page;
        }
        p->physical = (uintptr_t)r;

        /* Map */
        ret = vmem->mem->ifs.map(vmem->arch, e->start + i * MEMORY_PAGESIZE,
                             p, 0);
        if ( ret < 0 ) {
            vmem->allocator.free(vmem, (void *)p);
            phys_mem_free(vmem->mem->phys, (void *)p->physical, p->order,
                          p->zone, p->numadomain);
            goto error_page;
        }

        *pp = p;
        pp = &p->next;
    }

    /* Allocate and map pages */
    for ( ; i < nr; i++ ) {
        p = (page_t *)vmem->allocator.alloc(vmem);
        if ( NULL == p ) {
            goto error_page;
        }
        p->index = i;
        p->zone = zone;
        p->numadomain = numadomain;
        p->flags = 0;
        if ( e->flags & MEMORY_VMF_RW ) {
            p->flags |= MEMORY_PGF_RW;
        }
        p->order = 0;
        p->next = NULL;

        /* Allocate a physical page */
        r = phys_mem_alloc(vmem->mem->phys, p->order, p->zone, p->numadomain);
        if ( NULL == r ) {
            vmem->allocator.free(vmem, (void *)p);
            goto error_page;
        }
        p->physical = (uintptr_t)r;

        /* Map */
        ret = vmem->mem->ifs.map(vmem->arch, e->start + i * MEMORY_PAGESIZE,
                             p, 0);
        if ( ret < 0 ) {
            vmem->allocator.free(vmem, (void *)p);
            phys_mem_free(vmem->mem->phys, (void *)p->physical, p->order,
                          p->zone, p->numadomain);
            goto error_page;
        }

        *pp = p;
        pp = &p->next;
    }

    /* Add this entry */
    ret = _entry_add(&block->entries, e);
    if ( ret < 0 ) {
        goto error_page;
    }

    /* Remove the free entry */
    f = _free_delete(block, f);
    kassert( f != NULL );
    if ( f->start == e->start && f->size == e->size  ) {
        /* Remove the entire entry */
        vmem->allocator.free(vmem, (void *)f0);
        vmem->allocator.free(vmem, (void *)f1);
    } else if ( f->start == e->start ) {
        /* The start address is same, then add the rest to the free entry */
        f0->start = f->start + e->size;
        f0->size = f->size - e->size;
        ret = _free_add(block, f0);
        if ( ret < 0 ) {
            goto error_post;
        }
    } else if ( f->start + f->size == e->start + e->size ) {
        /* The end address is same, then add the rest to the free entry */
        f0->start = f->start;
        f0->size = f->size - e->size;
        ret = _free_add(block, f0);
        if ( ret < 0 ) {
            goto error_post;
        }
        vmem->allocator.free(vmem, (void *)f1);
    } else {
        f0->start = f->start;
        f0->size = e->start + e->size - f->start;
        f1->start = e->start + e->size;
        f1->size = f->start + f->size - f1->start;
        ret = _free_add(block, f0);
        if ( ret < 0 ) {
            goto error_post;
        }
        ret = _free_add(block, f1);
        if ( ret < 0 ) {
            goto error_free;
        }
    }
    vmem->allocator.free(vmem, (void *)f);

    return (void *)e->start;

error_free:
    _free_delete(block, f0);
error_post:
    _entry_delete(&block->entries, e);
    /* Add it back */
    _free_add(block, f);
error_page:
    p = e->object->pages;
    virtual = e->start;
    while ( NULL != p ) {
        ret = vmem->mem->ifs.unmap(vmem->arch, virtual, p);
        kassert(ret == 0);
        phys_mem_free(vmem->mem->phys, (void *)p->physical, p->order, p->zone,
                      p->numadomain);
        virtual += ((uintptr_t)MEMORY_PAGESIZE << p->order);
        vmem->allocator.free(vmem, (void *)p);
        p = p->next;
    }
    vmem->allocator.free(vmem, (void *)f1);
error_f1:
    vmem->allocator.free(vmem, (void *)f0);
error_f0:
    vmem->allocator.free(vmem, (void *)e->object);
error_obj:
    vmem->allocator.free(vmem, (void *)e);
error_entry:
    return NULL;
}

/*
 * Allocate pages
 */
void *
memory_alloc_pages(memory_t *mem, size_t nr, int zone, int domain)
{
    virt_memory_block_t *block;
    void *ptr;

    block = mem->kmem.blocks;
    ptr = NULL;
    while ( NULL != block && NULL == ptr ) {
        ptr = _alloc_pages_block(&mem->kmem, block, nr, zone, domain);
        block = block->next;
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
_pages_free(virt_memory_t *vmem, page_t *p)
{
    while ( NULL != p ) {
        /* Free physical pages */
        phys_mem_free(vmem->mem->phys, (void *)p->physical, p->order, p->zone,
                      p->numadomain);

        /* Free this page */
        vmem->allocator.free(vmem, (void *)p);

        p = p->next;
    }
}

/*
 * Free an entry
 */
static int
_entry_free(virt_memory_t *vmem, virt_memory_block_t *b, virt_memory_entry_t *e)
{
    virt_memory_free_t free;
    virt_memory_free_t *f;
    void *r;
    int ret;

    /* Find a neighboring free entry by address */
    f = _find_neighbor_free_entry(b->frees.atree, e->start, e->start + e->size);
    if ( NULL == f ) {
        /* Not found, then convert the data structure to free entry */
        kmemset(&free, 0, sizeof(virt_memory_free_t));
        free.start = e->start;
        free.size = e->size;
        f = (virt_memory_free_t *)e;
        kmemcpy(f, &free, sizeof(virt_memory_free_t));

        /* Add this node to the free entry tree */
        ret = _free_add(b, f);
        if ( ret < 0 ) {
            return -1;
        }
    } else {
        /* Remove the node first */
        r = _free_delete(b, f);
        kassert( r != NULL );

        /* Expand the free region */
        if ( f->start == e->start + e->size ) {
            f->start = e->start;
            f->size = f->size + e->size;
        } else {
            f->size = f->size + e->size;
        }
        vmem->allocator.free(vmem, (void *)e);

        /* Rebalance the size-based tree */
        ret = _free_add(b, f);
        kassert( ret == 0 );
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
    void *r;

    /* Convert the pointer to the address in integer */
    addr = (uintptr_t)ptr;

    /* Find a block */
    b = _find_block(&mem->kmem, addr);
    if ( NULL == b ) {
        /* Not found */
        return;
    }

    /* Find an entry corresponding to the virtual address */
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
    _pages_free(&mem->kmem, page);
    /* Free the object */
    mem->kmem.allocator.free(&mem->kmem, (void *)e->object);
    /* Retuurn to the free entry */
    r = _entry_delete(&b->entries, e);
    kassert( r == e );
    _entry_free(&mem->kmem, b, e);
}

/*
 * Initialize virtual memory
 */
virt_memory_t *
virt_memory_init(memory_t *mem, virt_memory_allocator_t *alloca)
{
    virt_memory_t *vm;

    vm = kmalloc(sizeof(virt_memory_t));
    if ( NULL == vm ) {
        return NULL;
    }
    vm->mem = mem;
    vm->blocks = NULL;
    vm->arch = mem->ifs.fork(mem->kmem.arch);

    /* Setup allocator */
    vm->allocator.spec = alloca->spec;
    vm->allocator.alloc = alloca->alloc;
    vm->allocator.free = alloca->free;

    return vm;
}

/*
 * Allocate virtual memory
 */
void *
virt_memory_alloc_pages(virt_memory_t *vmem, size_t nr, int zone, int domain)
{
    virt_memory_block_t *block;
    void *ptr;

    block = vmem->blocks;
    ptr = NULL;
    while ( NULL != block && NULL == ptr ) {
        ptr = _alloc_pages_block(vmem, block, nr, zone, domain);
        block = block->next;
    }

    return ptr;
}

/*
 * Free pages
 */
void
virt_memory_free_pages(virt_memory_t *vmem, void *ptr)
{
    virt_memory_block_t *b;
    uintptr_t addr;
    virt_memory_entry_t *e;
    page_t *page;
    void *r;

    /* Convert the pointer to the address in integer */
    addr = (uintptr_t)ptr;

    /* Find a block */
    b = _find_block(vmem, addr);
    if ( NULL == b ) {
        /* Not found */
        return;
    }

    /* Find an entry corresponding to the virtual address */
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
    _pages_free(vmem, page);
    /* Free the object */
    vmem->allocator.free(vmem, (void *)e->object);
    /* Retuurn to the free entry */
    r = _entry_delete(&b->entries, e);
    kassert( r == e );
    _entry_free(vmem, b, e);
}

/*
 * Add a new memory block
 */
int
virt_memory_block_add(virt_memory_t *vmem, uintptr_t start, uintptr_t end)
{
    virt_memory_free_t *fr;
    virt_memory_block_t *n;
    int ret;

    /* Allocate data and initialize the block */
    n = (virt_memory_block_t *)vmem->allocator.alloc(vmem);
    if ( NULL == n ) {
        return -1;
    }
    n->start = start;
    n->end = end;
    n->next = NULL;
    n->entries = NULL;
    n->frees.atree = NULL;
    n->frees.stree = NULL;

    /* Add a free entry aligned to the block and the page size */
    fr = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == fr ) {
        vmem->allocator.free(vmem, (void *)n);
        return -1;
    }
    fr->start = (start + MEMORY_PAGESIZE - 1)
        & ~(uintptr_t)(MEMORY_PAGESIZE - 1);
    fr->size = ((end + 1) & ~(uintptr_t)(MEMORY_PAGESIZE - 1)) - fr->start;
    n->frees.atree = fr;
    n->frees.stree = fr;

    /* Insert the block to the sorted list */
    ret = _block_insert(vmem, n);
    if ( ret < 0 ) {
        vmem->allocator.free(vmem, (void *)fr);
        vmem->allocator.free(vmem, (void *)n);
        return -1;
    }

    return 0;
}

/*
 * Wire pages
 */
int
virt_memory_wire(virt_memory_t *vmem, uintptr_t virtual, size_t nr,
                 uintptr_t physical)
{
    virt_memory_block_t *b;
    uintptr_t endplus1;
    virt_memory_free_t *f;
    virt_memory_free_t *f0;
    virt_memory_free_t *f1;
    virt_memory_entry_t *e;
    size_t size;
    page_t *p;
    page_t **pp;
    uintptr_t idx;
    int ret;
    int order;

    /* Page alignment check */
    if ( virtual & (MEMORY_PAGESIZE - 1) ) {
        return -1;
    }
    if ( physical & (MEMORY_PAGESIZE - 1) ) {
        return -1;
    }

    /* Find a block including the virtual address */
    b = _find_block(vmem, virtual);
    if ( NULL == b ) {
        /* Not found */
        return -1;
    }

    /* Find a free space corresponding to the virtual address */
    f = _find_free_entry(b->frees.atree, virtual);
    if ( NULL == f ) {
        /* Not found */
        return -1;
    }

    /* Check if the whole size is within this space */
    size = MEMORY_PAGESIZE * nr;
    if ( virtual + size > f->start + f->size ) {
        return -1;
    }

    /* Prepare an entry and an object */
    e = (virt_memory_entry_t *)vmem->allocator.alloc(vmem);
    if ( NULL == e ) {
        goto error_entry;
    }
    e->start = virtual;
    e->size = nr * MEMORY_PAGESIZE;
    e->offset = 0;
    e->flags = MEMORY_VMF_RW;
    e->object = (virt_memory_object_t *)vmem->allocator.alloc(vmem);
    if ( NULL == e->object ) {
        goto error_obj;
    }
    e->object->type = MEMORY_OBJECT;
    e->object->size = nr * MEMORY_PAGESIZE;
    e->object->pages = NULL;
    e->object->refs = 1;

    /* Prepare for free spaces */
    f0 = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == f0 ) {
        goto error_f0;
    }
    f1 = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == f1 ) {
        goto error_f1;
    }

    /* Allocate and map all pages */
    endplus1 = virtual + size;
    pp = &e->object->pages;
    idx = 0;
    while ( virtual < endplus1 ) {
        /* Allocate a page data structure */
        p = (page_t *)vmem->allocator.alloc(vmem);
        if ( NULL == p ) {
            goto error_page;
        }
        p->index = 0;
        p->physical = physical;
        p->flags = MEMORY_PGF_WIRED;
        if ( e->flags & MEMORY_VMF_RW ) {
            p->flags |= MEMORY_PGF_RW;
        }
        p->next = NULL;
        /* Calculate the order to minimize the number of page_t */
        order = _order(virtual, physical, endplus1 - virtual);
        p->order = order;
        ret = vmem->mem->ifs.map(vmem->arch, virtual, p, 0);
        if ( ret < 0 ) {
            vmem->allocator.free(vmem, (void *)p);
            goto error_page;
        }
        virtual += 1ULL << (order + MEMORY_PAGESIZE_SHIFT);
        physical += 1ULL << (order + MEMORY_PAGESIZE_SHIFT);
        idx += 1ULL << order;

        *pp = p;
        pp = &p->next;
    }

    /* Add this entry */
    ret = _entry_add(&b->entries, e);
    if ( ret < 0 ) {
        goto error_page;
    }

    /* Remove the free entry */
    f = _free_delete(b, f);
    kassert( f != NULL );
    if ( f->start == e->start && f->size == e->size  ) {
        /* Remove the entire entry */
        vmem->allocator.free(vmem, (void *)f0);
        vmem->allocator.free(vmem, (void *)f1);
    } else if ( f->start == e->start ) {
        /* The start address is same, then add the rest to the free entry */
        f0->start = f->start + e->size;
        f0->size = f->size - e->size;
        ret = _free_add(b, f0);
        if ( ret < 0 ) {
            goto error_post;
        }
        vmem->allocator.free(vmem, (void *)f1);
    } else if ( f->start + f->size == e->start + e->size ) {
        /* The end address is same, then add the rest to the free entry */
        f0->start = f->start;
        f0->size = f->size - e->size;
        ret = _free_add(b, f0);
        if ( ret < 0 ) {
            goto error_post;
        }
        vmem->allocator.free(vmem, (void *)f1);
    } else {
        f0->start = f->start;
        f0->size = e->start + e->size - f->start;
        f1->start = e->start + e->size;
        f1->size = f->start + f->size - f1->start;
        ret = _free_add(b, f0);
        if ( ret < 0 ) {
            goto error_post;
        }
        ret = _free_add(b, f1);
        if ( ret < 0 ) {
            goto error_free;
        }
    }
    vmem->allocator.free(vmem, (void *)f);

    return 0;

error_free:
    _free_delete(b, f0);
error_post:
    _entry_delete(&b->entries, e);
    /* Add it back */
    _free_add(b, f);
error_page:
    p = e->object->pages;
    virtual = e->start;
    while ( NULL != p ) {
        ret = vmem->mem->ifs.unmap(vmem->arch, virtual, p);
        kassert(ret == 0);
        virtual += ((uintptr_t)MEMORY_PAGESIZE << p->order);
        vmem->allocator.free(vmem, (void *)p);
        p = p->next;
    }
    vmem->allocator.free(vmem, (void *)f1);
error_f1:
    vmem->allocator.free(vmem, (void *)f0);
error_f0:
    vmem->allocator.free(vmem, (void *)e->object);
error_obj:
    vmem->allocator.free(vmem, (void *)e);
error_entry:
    return -1;
}

/*
 * Copy entries
 */
static int
_entry_fork(virt_memory_t *dst, virt_memory_t *src, virt_memory_block_t *b,
            virt_memory_entry_t *e)
{
    virt_memory_entry_t *n;
    int ret;
    virt_memory_object_t *obj;

    n = (virt_memory_entry_t *)dst->allocator.alloc(dst);
    if ( NULL == n ) {
        return -1;
    }
    n->start = e->start;
    n->size = e->size;
    n->offset = e->offset;
    n->flags = e->flags | MEMORY_VMF_COW;
    n->object = NULL;
    n->atree.left = NULL;
    n->atree.right = NULL;

    /* Add this entry to the entry tree */
    ret = _entry_add(&b->entries, n);
    if ( ret < 0 ) {
        /* Failed to add the entry */
        dst->allocator.free(dst, (void *)n);
        return -1;
    }

    /* Shadow object for the source entry */
    obj = (virt_memory_object_t *)src->allocator.alloc(src);
    if ( NULL == obj ) {
        dst->allocator.free(dst, (void *)n);
        return -1;
    }
    obj->type = MEMORY_SHADOW;
    obj->pages = NULL;
    obj->size = e->object->size;
    obj->refs = 1;
    obj->u.shadow.object = e->object;

    /* Shadow object for the destination entry */
    n->object = (virt_memory_object_t *)dst->allocator.alloc(dst);
    if ( NULL == n->object ) {
        src->allocator.free(src, (void *)obj);
        dst->allocator.free(dst, (void *)n);
        return -1;
    }
    n->object->type = MEMORY_SHADOW;
    n->object->pages = NULL;
    n->object->size = e->object->size;
    n->object->refs = 1;
    n->object->u.shadow.object = e->object;

    /* Replace the reference from the source entry  */
    e->object->refs++;
    e->object = obj;

    /* Traverse the tree */
    if ( NULL != e->atree.left ) {
        ret = _entry_fork(dst, src, b, e->atree.left);
        if ( ret < 0 ) {
            return -1;
        }
    }
    if ( NULL != e->atree.right ) {
        ret = _entry_fork(dst, src, b, e->atree.right);
        if ( ret < -1 ) {
            return -1;
        }
    }

    return 0;
}

/*
 * Free all entries
 */
static void
_entry_free_all(virt_memory_t *vmem, virt_memory_entry_t *e)
{
    if ( NULL != e->atree.left ) {
        _entry_free_all(vmem, e->atree.left);
    }
    if ( NULL != e->atree.right ) {
        _entry_free_all(vmem, e->atree.right);
    }
    vmem->allocator.free(vmem, (void *)e);
}

/*
 * Copy a block
 */
static int
_block_fork(virt_memory_t *dst, virt_memory_t *src, virt_memory_block_t *sb)
{
    int ret;
    virt_memory_block_t *n;

    /* Allocate data and initialize the block */
    n = (virt_memory_block_t *)dst->allocator.alloc(dst);
    if ( NULL == n ) {
        return -1;
    }
    n->start = sb->start;
    n->end = sb->end;
    n->next = NULL;
    n->entries = NULL;
    n->frees.atree = NULL;
    n->frees.stree = NULL;

    /* Copy entries */
    ret = _entry_fork(dst, src, n, sb->entries);
    if ( ret < 0 ) {
        /* Free all entries */
        _entry_free_all(dst, n->entries);
        dst->allocator.free(dst, (void *)n);
        return -1;
    }

    return 0;
}

/*
 * Fork
 */
int
virt_memory_fork(virt_memory_t *dst, virt_memory_t *src)
{
    virt_memory_block_t *b;
    int ret;

    /* Copy blocks */
    b = src->blocks;
    while ( NULL != b ) {
        ret = _block_fork(dst, src, b);
        if ( ret < 0 ) {
            /* dst may change since the function call. */
            return -1;
        }
        b = b->next;
    }

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
