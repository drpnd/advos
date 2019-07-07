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
 * Structures for btree search conditions
 */
struct virt_memory_start_end {
    uintptr_t start;
    uintptr_t end;
};

struct virt_memory_size {
    size_t size;
    virt_memory_free_t *ret;
};

/*
 * Prototype declarations
 */
static virt_memory_block_t * _find_block(virt_memory_t *, uintptr_t);
static int _free_add(virt_memory_block_t *, virt_memory_free_t *);
static virt_memory_free_t *
_free_delete(virt_memory_block_t *, virt_memory_free_t *);
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
    mem->kmem.objects = NULL;
    mem->kmem.mem = mem;

    /* Architecture specific data structure */
    mem->kmem.arch = arch;
    mem->ifs.map = ifs->map;
    mem->ifs.unmap = ifs->unmap;
    mem->ifs.prepare = ifs->prepare;
    mem->ifs.refer = ifs->refer;
    mem->ifs.new = ifs->new;
    mem->ifs.ctxsw = ifs->ctxsw;

    return 0;
}

/*
 * Compare the start addrsss
 */
int
virt_memory_comp_addr(void *a, void *b)
{
    virt_memory_free_t *va;
    virt_memory_free_t *vb;

    /* Cast */
    va = (virt_memory_free_t *)a;
    vb = (virt_memory_free_t *)b;

    if ( va->start == vb->start ) {
        return 0;
    } else if ( va->start > vb->start ) {
        return 1;
    } else {
        return -1;
    }
}

/*
 * Compare the size
 */
int
virt_memory_comp_size(void *a, void *b)
{
    virt_memory_free_t *va;
    virt_memory_free_t *vb;

    /* Cast */
    va = (virt_memory_free_t *)a;
    vb = (virt_memory_free_t *)b;

    if ( va->size == vb->size ) {
        return 0;
    } else if ( va->size > vb->size ) {
        return 1;
    } else {
        return -1;
    }
}

/*
 * Search condition for a fitting entry
 */
int
virt_memory_cond_fit(void *a, void *data)
{
    virt_memory_entry_t *va;
    uintptr_t addr;

    /* Cast */
    va = (virt_memory_entry_t *)a;
    addr = (uintptr_t)data;

    if ( addr >= va->start && addr < va->start + va->size ) {
        /* Found */
        return 0;
    }

    if ( addr < va->start ) {
        /* Search left */
        return -1;
    } else {
        /* Search right */
        return 1;
    }
}

/*
 * Search condition for a fitting free entry
 */
int
virt_memory_cond_fit_free(void *a, void *data)
{
    virt_memory_free_t *va;
    uintptr_t addr;

    /* Cast */
    va = (virt_memory_free_t *)a;
    addr = (uintptr_t)data;

    if ( addr >= va->start && addr < va->start + va->size ) {
        /* Found */
        return 0;
    }

    if ( addr < va->start ) {
        /* Search left */
        return -1;
    } else {
        /* Search right */
        return 1;
    }
}

/*
 * Search condition for a fitting free entry by size
 */
int
virt_memory_cond_fit_free_size(void *a, void *data)
{
    virt_memory_free_t *va;
    struct virt_memory_size *sz;

    /* Cast */
    va = (virt_memory_free_t *)a;
    sz = (struct virt_memory_size *)data;

    if ( sz->size > va->size ) {
        /* Search the right subtree */
        return 1;
    } else {
        /* Search the left subtree */
        sz->ret = va;
        return -1;
    }
}

/*
 * Search condition for a neighboring free entry
 */
int
virt_memory_cond_neigh_free(void *a, void *data)
{
    virt_memory_free_t *va;
    uintptr_t start;
    uintptr_t end;
    struct virt_memory_start_end *se;

    /* Cast */
    va = (virt_memory_free_t *)a;
    se = (struct virt_memory_start_end *)data;
    start = se->start;
    end = se->end;

    if ( end == va->start || start == va->start + va->size ) {
        /* Found */
        return 0;
    }
    if ( start < va->start ) {
        /* Search the left */
        return -1;
    } else {
        /* Search the right */
        return 1;
    }
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
_find_free_entry(virt_memory_block_t *b, uintptr_t addr)
{
    btree_node_t *n;

    n = btree_search(b->frees.atree, (void *)addr, virt_memory_cond_fit_free);
    if ( NULL == n ) {
        return NULL;
    }

    return n->data;
}

/*
 * Find a free entry neighboring to the start and the end
 */
static virt_memory_free_t *
_find_neighbor_free_entry(virt_memory_block_t *b, uintptr_t start,
                          uintptr_t end)
{
    btree_node_t *n;
    struct virt_memory_start_end sn;

    sn.start = start;
    sn.end = end;
    n = btree_search(b->frees.atree, (void *)&sn, virt_memory_cond_neigh_free);
    if ( NULL == n ) {
        return NULL;
    }

    return n->data;

}

/*
 * Add an entry
 */
static int
_entry_add(virt_memory_block_t *b, virt_memory_entry_t *n)
{
    btree_node_t *bn;

    bn = &n->atree;
    bn->data = n;

    return btree_add(&b->entries, bn, virt_memory_comp_addr, 0);
}

/*
 * Delete an entry
 */
static virt_memory_entry_t *
_entry_delete(virt_memory_block_t *b, virt_memory_entry_t *n)
{
    btree_node_t *r;

    r = btree_delete(&b->entries, &n->atree, virt_memory_comp_addr);
    if ( NULL == r ) {
        return NULL;
    }

    return r->data;
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
_free_add(virt_memory_block_t *b, virt_memory_free_t *n)
{
    int ret;
    void *p;

    n->atree.data = n;
    n->stree.data = n;

    ret = btree_add(&b->frees.atree, &n->atree, virt_memory_comp_addr, 0);
    if ( ret < 0 ) {
        return -1;
    }
    ret = btree_add(&b->frees.stree, &n->stree, virt_memory_comp_size, 1);
    if ( ret < 0 ) {
        p = btree_delete(&b->frees.atree, &n->atree, virt_memory_comp_addr);
        kassert( p != NULL );
        return -1;
    }

    return 0;
}

/*
 * Delete the specified node from the free entry tree
 */
static virt_memory_free_t *
_free_delete(virt_memory_block_t *b, virt_memory_free_t *n)
{
    btree_node_t *fa;
    btree_node_t *fs;

    fa = btree_delete(&b->frees.atree, &n->atree, virt_memory_comp_addr);
    fs = btree_delete(&b->frees.stree, &n->stree, virt_memory_comp_size);
    kassert( fa != NULL && fs != NULL );
    kassert( fa->data == fs->data );

    return fa->data;
}

/*
 * Search the tree by size
 */
static virt_memory_free_t *
_search_fit_size(virt_memory_block_t *block, size_t sz)
{
    struct virt_memory_size r;

    r.size = sz;
    r.ret = NULL;
    btree_search(block->frees.stree, &r, virt_memory_cond_fit_free_size);
    if ( NULL == r.ret ) {
        return NULL;
    }

    return r.ret;
}

/*
 * Allocate an oobject
 */
static virt_memory_object_t *
_alloc_object(virt_memory_t *vmem, size_t size)
{
    virt_memory_object_t *obj;

    obj = (virt_memory_object_t *)vmem->allocator.alloc(vmem);
    if ( NULL == obj ) {
        return NULL;
    }
    obj->type = MEMORY_OBJECT;
    obj->size = size;
    obj->pages = NULL;
    obj->refs = 0;
    obj->next = NULL;

    /* Insert into the linked list of the virtual memory */
    obj->next = vmem->objects;
    vmem->objects = obj;

    return obj;
}

/*
 * Allocate an entry
 */
static virt_memory_entry_t *
_alloc_entry(virt_memory_t *vmem, virt_memory_object_t *obj, uintptr_t addr,
             size_t size, off_t offset, int flags)
{
    virt_memory_entry_t *e;
    virt_memory_free_t *f;
    virt_memory_block_t *b;
    virt_memory_free_t *f0;
    virt_memory_free_t *f1;
    int ret;

    /* Page alignment check */
    if ( addr & (MEMORY_PAGESIZE - 1) ) {
        return NULL;
    }
    if ( size & (MEMORY_PAGESIZE - 1) ) {
        return NULL;
    }

    /* Find a block including the virtual address */
    b = _find_block(vmem, addr);
    if ( NULL == b ) {
        /* Not found */
        return NULL;
    }

    /* Find a free space corresponding to the virtual address */
    f = _find_free_entry(b, addr);
    if ( NULL == f ) {
        /* Not found */
        return NULL;
    }

    /* Check the entry size */
    if ( addr + size > f->start + f->size ) {
        /* The end address exceeds the range of the found entry */
        return NULL;
    }

    /* Check the object size */
    if ( offset + size > obj->size ) {
        /* Exceed the range of the object */
        return NULL;
    }

    /* Prepare an entry */
    e = (virt_memory_entry_t *)vmem->allocator.alloc(vmem);
    if ( NULL == e ) {
        return NULL;
    }
    e->start = addr;
    e->size = size;
    e->offset = offset;
    obj->refs++;
    e->object = obj;
    e->flags = flags;

    /* Prepare for free spaces */
    f0 = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == f0 ) {
        goto error_f0;
    }
    f1 = (virt_memory_free_t *)vmem->allocator.alloc(vmem);
    if ( NULL == f1 ) {
        goto error_f1;
    }

    /* Add this entry */
    ret = _entry_add(b, e);
    if ( ret < 0 ) {
        goto error_entry;
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

    return e;

error_free:
    _free_delete(b, f0);
error_post:
    _entry_delete(b, e);
    /* Add it back */
    _free_add(b, f);
error_entry:
    vmem->allocator.free(vmem, (void *)f1);
error_f1:
    vmem->allocator.free(vmem, (void *)f0);
error_f0:
    obj->refs--;
    vmem->allocator.free(vmem, (void *)e);

    return NULL;

}

/*
 * Allocate pages for the specified range
 */
static int
_alloc_pages(virt_memory_t *vmem, virt_memory_entry_t *e)
{
    page_t *p;
    page_t **pp;
    int ret;
    size_t nr;
    size_t i;
    void *r;
    int flags;
    uintptr_t virtual;

    /* # of pages */
    nr = e->size / MEMORY_PAGESIZE;

    /* Allocate and map pages */
    pp = &e->object->pages;
    for ( i = 0; i < nr; i++ ) {
        p = (page_t *)vmem->allocator.alloc(vmem);
        if ( NULL == p ) {
            goto error_page;
        }
        p->index = i;
        p->zone = MEMORY_ZONE_NUMA_AWARE;
        p->numadomain = 0;
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
        flags = vmem->flags;
        ret = vmem->mem->ifs.map(vmem->arch, e->start + i * MEMORY_PAGESIZE,
                                 p, flags);
        if ( ret < 0 ) {
            vmem->allocator.free(vmem, (void *)p);
            phys_mem_free(vmem->mem->phys, (void *)p->physical, p->order,
                          p->zone, p->numadomain);
            goto error_page;
        }

        *pp = p;
        pp = &p->next;
    }

    return 0;

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

    return -1;
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
    int flags;

    /* Search from the binary tree */
    size = nr * MEMORY_PAGESIZE;
    superpage = 0;
    if ( size >= MEMORY_SUPERPAGESIZE ) {
        /* Search larger size to make sure to align superpages */
        size += MEMORY_SUPERPAGESIZE;
        superpage = 1;
    }
    f = _search_fit_size(block, size);
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
        flags = vmem->flags;
        ret = vmem->mem->ifs.map(vmem->arch, e->start + i * MEMORY_PAGESIZE,
                                 p, flags);
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
        flags = vmem->flags;
        ret = vmem->mem->ifs.map(vmem->arch, e->start + i * MEMORY_PAGESIZE,
                                 p, flags);
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
    ret = _entry_add(block, e);
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
    _entry_delete(block, e);
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
_find_entry(virt_memory_block_t *b, uintptr_t addr)
{
    btree_node_t *n;

    n = btree_search(b->entries, (void *)addr, virt_memory_cond_fit);
    if ( NULL == n ) {
        return NULL;
    }
    return n->data;
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
    f = _find_neighbor_free_entry(b, e->start, e->start + e->size);
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
    e = _find_entry(b, addr);
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
    r = _entry_delete(b, e);
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
    vm->objects = NULL;

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
    e = _find_entry(b, addr);
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
    r = _entry_delete(b, e);
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
    fr->atree.data = fr;
    fr->atree.left = NULL;
    fr->atree.right = NULL;
    fr->stree.data = fr;
    fr->stree.left = NULL;
    fr->stree.right = NULL;
    n->frees.atree = &fr->atree;
    n->frees.stree = &fr->stree;

    /* Prepare the page table */
    ret = vmem->mem->ifs.prepare(vmem->arch, n->start, n->end - n->start + 1);
    if ( ret < 0 ) {
        vmem->allocator.free(vmem, (void *)fr);
        vmem->allocator.free(vmem, (void *)n);
        return -1;
    }

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
    int flags;

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
    f = _find_free_entry(b, virtual);
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
        flags = vmem->flags;
        ret = vmem->mem->ifs.map(vmem->arch, virtual, p, flags);
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
    ret = _entry_add(b, e);
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
    _entry_delete(b, e);
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
 * Allocate pages with specified address
 */
void *
virt_memory_alloc_pages_addr(virt_memory_t *vmem, uintptr_t virtual, size_t nr,
                             int zone, int numadomain)
{
    virt_memory_block_t *b;
    virt_memory_free_t *f;
    virt_memory_free_t *f0;
    virt_memory_free_t *f1;
    virt_memory_entry_t *e;
    size_t size;
    page_t *p;
    page_t **pp;
    int ret;
    int flags;
    size_t i;
    void *r;

    /* Page alignment check */
    if ( virtual & (MEMORY_PAGESIZE - 1) ) {
        return NULL;
    }

    /* Find a block including the virtual address */
    b = _find_block(vmem, virtual);
    if ( NULL == b ) {
        /* Not found */
        return NULL;
    }

    /* Find a free space corresponding to the virtual address */
    f = _find_free_entry(b, virtual);
    if ( NULL == f ) {
        /* Not found */
        return NULL;
    }

    /* Check if the whole size is within this space */
    size = MEMORY_PAGESIZE * nr;
    if ( virtual + size > f->start + f->size ) {
        return NULL;
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

    /* Allocate and map pages */
    pp = &e->object->pages;
    for ( i = 0; i < nr; i++ ) {
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
        flags = vmem->flags;
        ret = vmem->mem->ifs.map(vmem->arch, e->start + i * MEMORY_PAGESIZE,
                                 p, flags);
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
    ret = _entry_add(b, e);
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

    return (void *)virtual;

error_free:
    _free_delete(b, f0);
error_post:
    _entry_delete(b, e);
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
    return NULL;
}

/*
 * Copy entries
 */
static int
_entry_fork(virt_memory_t *dst, virt_memory_t *src, virt_memory_block_t *b,
            btree_node_t *bn)
{
    virt_memory_entry_t *n;
    int ret;
    virt_memory_object_t *obj;
    virt_memory_entry_t *e;

    e = (virt_memory_entry_t *)bn->data;

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
    ret = _entry_add(b, n);
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
_entry_free_all(virt_memory_t *vmem, btree_node_t *n)
{
    virt_memory_entry_t *e;

    e = (virt_memory_entry_t *)n->data;
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
 * New process memory
 */
int
virt_memory_new(virt_memory_t *dst, memory_t *mem, virt_memory_allocator_t *a)
{
    virt_memory_block_t *b;
    int ret;

    dst->mem = mem;
    dst->blocks = NULL;
    dst->objects = NULL;

    /* Setup allocator */
    dst->allocator.spec = a->spec;
    dst->allocator.alloc = a->alloc;
    dst->allocator.free = a->free;

    /* Copy the kernel memory blocks */
    b = mem->kmem.blocks;
    while ( NULL != b ) {
        ret = dst->mem->ifs.refer(dst->arch, mem->kmem.arch, b->start,
                                  b->end - b->start + 1);
        if ( ret < 0 ) {
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
