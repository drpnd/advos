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

#include "pgt.h"
#include "const.h"
#include "../../kernel.h"
#include "../../memory.h"
#include <stdint.h>

#define invlpg(addr)    __asm__ __volatile__ ("invlpg (%%rax)" :: "a"((addr)))
#define set_cr3(cr3)    __asm__ __volatile__ ("movq %%rax,%%cr3" :: "a"((cr3)))
static uintptr_t
get_cr3(void)
{
    uintptr_t cr3;
    __asm__ __volatile__ ("movq %%cr3,%%rax" : "=a"((cr3)));
    return cr3;
}

#define MASK_PAGE(a)        ((a) & ~0xfffULL)
#define MASK_SUPERPAGE(a)   ((a) & ~0x1fffffULL)

/*
 * CR3
 */
union pgt_cr3 {
    struct {
        uint64_t ignore2:3;
        uint64_t pwt:1;         /* PWT: Page-level Write Transparent */
        uint64_t pcd:1;         /* PCD: Page-level Cache Disable */
        uint64_t ignore1:7;
        uint64_t pml4:52;
    } ptr;
    uint64_t v;
} __attribute__ ((packed));

/*
 * PML4E
 */
union pgt_pml4_entry {
    struct {
        uint64_t present:1;
        uint64_t rw:1;
        uint64_t us:1;
        uint64_t pwt:1;
        uint64_t pcd:1;
        uint64_t a:1;
        uint64_t ignore2:1;
        uint64_t rsv:1;
        uint64_t ignore1:4;
        uint64_t pdpt:51;
        uint64_t xd:1;          /* execute-disable */
    } ptr;
    uint64_t v;
} __attribute__ ((packed));

/*
 * PDPTE
 */
union pgt_pdpt_entry {
    /* Pointer */
    struct {
        uint64_t present:1;
        uint64_t rw:1;
        uint64_t us:1;
        uint64_t pwt:1;
        uint64_t pcd:1;
        uint64_t a:1;
        uint64_t ignore2:1;
        uint64_t page:1;        /* 0 */
        uint64_t ignore1:4;
        uint64_t pdpt:51;
        uint64_t xd:1;
    } ptr;
    /* Page */
    struct {
        uint64_t present:1;
        uint64_t rw:1;
        uint64_t us:1;
        uint64_t pwt:1;
        uint64_t pcd:1;
        uint64_t a:1;
        uint64_t d:1;
        uint64_t page:1;        /* 1 */
        uint64_t g:1;
        uint64_t ignore1:3;
        uint64_t pat:1;
        uint64_t pdpt:50;
        uint64_t xd:1;
    } page;
    /* Value */
    uint64_t v;
} __attribute__ ((packed));

/*
 * PDE
 */
union pgt_pd_entry {
    /* Pointer */
    struct {
        uint64_t present:1;
        uint64_t rw:1;
        uint64_t us:1;
        uint64_t pwt:1;
        uint64_t pcd:1;
        uint64_t a:1;
        uint64_t ignore2:1;
        uint64_t page:1;        /* 0 */
        uint64_t ignore1:4;
        uint64_t pdpt:51;
        uint64_t xd:1;
    } ptr;
    /* Page */
    struct {
        uint64_t present:1;
        uint64_t rw:1;
        uint64_t us:1;
        uint64_t pwt:1;
        uint64_t pcd:1;
        uint64_t a:1;
        uint64_t d:1;
        uint64_t page:1;        /* 1 */
        uint64_t g:1;
        uint64_t ignore1:3;
        uint64_t pat:1;
        uint64_t pdpt:50;
        uint64_t xd:1;
    } page;
    /* Value */
    uint64_t v;
} __attribute__ ((packed));

/*
 * PTE
 */
union pgt_pt_entry {
    /* Page */
    struct {
        uint64_t present:1;
        uint64_t rw:1;
        uint64_t us:1;
        uint64_t pwt:1;
        uint64_t pcd:1;
        uint64_t a:1;
        uint64_t d:1;
        uint64_t pat:1;
        uint64_t g:1;
        uint64_t ignore1:3;
        uint64_t pdpt:51;
        uint64_t xd:1;
    } page;
    /* Value */
    uint64_t v;
} __attribute__ ((packed));


/*
 * Calculate the physical address from virtual address
 */
static uintptr_t
_v2p(pgt_t *pgt, uintptr_t physical)
{
    return (physical - pgt->p2v);
}

/*
 * Resolve the virtual address from a physical address
 */
static uintptr_t
_p2v(pgt_t *pgt, uintptr_t physical)
{
    return (physical + pgt->p2v);
}

/*
 * Initialize the page table
 */
void
pgt_init(pgt_t *pgt, void *buf, size_t nr, uintptr_t p2v)
{
    union pgt_pml4_entry *pml4;
    union pgt_pdpt_entry *pdpt;
    union pgt_pd_entry *pd;
    size_t i;

    kassert( nr >= 3 );

    pgt->p2v = p2v;
    pgt->free = NULL;
    kmemset(buf, 0, 4096 * 3);
    pgt->cr3 = _v2p(pgt, (uintptr_t)buf);

    /* Prepare a PDPT and a PD for the kernel space */
    pml4 = (union pgt_pml4_entry *)buf;
    pdpt = (union pgt_pdpt_entry *)(buf + 4096);
    pd = (union pgt_pd_entry *)(buf + 8192);

    /* Set up 0-512 GiB PML4 entry */
    pml4[0].ptr.present = 1;
    pml4[0].ptr.rw = 1;
    pml4[0].ptr.us = 1;
    pml4[0].v |= _v2p(pgt, (uint64_t)pdpt);

    /* Set up 3-4 GiB PDPT entry */
    pdpt[3].ptr.present = 1;
    pdpt[3].ptr.rw = 1;
    pdpt[3].ptr.us = 1;
    pdpt[3].v |= _v2p(pgt, (uint64_t)pd);

    /* Push the remaining pages to the free list */
    for ( i = 3; i < nr; i++ ) {
        pgt_push(pgt, buf + i * 4096);
    }
}

/*
 * Initialize the process page table
 */
void
pgt_process_init(pgt_t *kpgt, pgt_t *upgt, void *pml4, size_t nr, uintptr_t p2v)
{
    union pgt_pml4_entry *kpml4;
    union pgt_pml4_entry *upml4;
    union pgt_pdpt_entry *kpdpt;
    union pgt_pdpt_entry *updpt;

    kassert( nr >= 3 );

    upgt->p2v = p2v;
    upgt->free = NULL;
    kmemset(pml4, 0, 8192);
    upgt->cr3 = _v2p(upgt, (uintptr_t)pml4);

    /* Kernel PDPT */
    kpml4 = (union pgt_pml4_entry *)_p2v(kpgt, MASK_PAGE(kpgt->cr3));
    kpdpt = (union pgt_pdpt_entry *)_p2v(kpgt, kpml4[0].v);

    /* User PDPT */
    updpt = (union pgt_pdpt_entry *)(pml4 + 4096);
    upml4 = (union pgt_pml4_entry *)pml4;
    upml4[0].ptr.present = 1;
    upml4[0].ptr.rw = 1;
    upml4[0].ptr.us = 1;
    upml4[0].v |= _v2p(upgt, (uint64_t)updpt);

    /* Copy the kernel region */
    updpt[3].v = kpdpt[3].v;
}

/*
 * Resolve the physical address from a virtual address
 */
void *
pgt_v2p(pgt_t *pgt, uintptr_t virtual)
{
    uintptr_t p;
    int idx;
    union pgt_pml4_entry *pml4;
    union pgt_pdpt_entry *pdpt;
    union pgt_pd_entry *pd;
    union pgt_pt_entry *pt;

    /* PML4 */
    p = MASK_PAGE(pgt->cr3);
    idx = (virtual >> 39) & 0x1ff;
    pml4 = (void *)_p2v(pgt, p);
    if ( !pml4[idx].ptr.present ) {
        /* Not present */
        return NULL;
    }
    /* PDPT */
    p = MASK_PAGE(pml4[idx].v);
    idx = (virtual >> 30) & 0x1ff;
    pdpt = (void *)_p2v(pgt, p);
    if ( !pdpt[idx].ptr.present ) {
        /* Not present */
        return NULL;
    }
    if ( pdpt[idx].ptr.page ) {
        /* 1 GiB paging */
        return (void *)MASK_SUPERPAGE(pdpt[idx].v);
    }
    /* PD */
    p = MASK_PAGE(pdpt->v);
    idx = (virtual >> 21) & 0x1ff;
    pd = (void *)_p2v(pgt, p);
    if ( !pd[idx].ptr.present ) {
        /* Not present */
        return NULL;
    }
    if ( pd[idx].ptr.page ) {
        /* 2 MiB paging */
        return (void *)MASK_SUPERPAGE(pd[idx].v);
    }
    /* PT */
    p = MASK_PAGE(pd->v);
    idx = (virtual >> 12) & 0x1ff;
    pt = (void *)_p2v(pgt, p);
    if ( !pt[idx].page.present ) {
        /* Not present */
        return NULL;
    }
    return (void *)MASK_PAGE(pt[idx].v);
}

/*
 * Get a page
 */
void *
pgt_pop(pgt_t *pgt)
{
    pgt_entry_t *pg;

    pg = pgt->free;
    if ( NULL != pg ) {
        pgt->free = pg->next;
    }

    return pg;
}

/*
 * Release a page
 */
void
pgt_push(pgt_t *pgt, pgt_entry_t *pg)
{
    pg->next = pgt->free;
    pgt->free = pg;
}

/*
 * Map the virtual address to the specified physical address
 */
int
pgt_map(pgt_t *pgt, uintptr_t virtual, uintptr_t physical, int superpage,
        int global, int rw, int user)
{
    uintptr_t p;
    union pgt_pml4_entry *pml4;
    union pgt_pdpt_entry *pdpt;
    union pgt_pd_entry *pd;
    union pgt_pt_entry *pt;
    int idx;

    /* Align */
    if ( superpage ) {
        virtual = virtual & ~((1ULL << 21) - 1);
        physical = physical & ~((1ULL << 21) - 1);
    } else {
        virtual = virtual & ~((1ULL << 12) - 1);
        physical = physical & ~((1ULL << 12) - 1);
    }

    /* PML4 */
    p = MASK_PAGE(pgt->cr3);
    idx = (virtual >> 39) & 0x1ff;
    pml4 = (void *)_p2v(pgt, p);
    if ( !pml4[idx].ptr.present ) {
        /* Not present, then add a page here */
        pdpt = pgt_pop(pgt);
        if ( NULL == pdpt ) {
            return -1;
        }
        kmemset(pdpt, 0, 4096);
        pml4[idx].ptr.present = 1;
        pml4[idx].ptr.rw = 1;
        pml4[idx].ptr.us = 1;
        pml4[idx].v |= _v2p(pgt, (uint64_t)pdpt);
    } else {
        p = MASK_PAGE(pml4[idx].v);
        pdpt = (void *)_p2v(pgt, p);
    }

    /* PDPT */
    idx = (virtual >> 30) & 0x1ff;
    if ( !pdpt[idx].ptr.present ) {
        /* Not present, then add a page here */
        pd = pgt_pop(pgt);
        if ( NULL == pd ) {
            return -1;
        }
        kmemset(pd, 0, 4096);
        pdpt[idx].ptr.present = 1;
        pdpt[idx].ptr.rw = 1;
        pdpt[idx].ptr.us = 1;
        pdpt[idx].v |= _v2p(pgt, (uint64_t)pd);
    } else {
        p = MASK_PAGE(pdpt[idx].v);
        pd = (void *)_p2v(pgt, p);
    }

    /* PD */
    idx = (virtual >> 21) & 0x1ff;
    if ( superpage ) {
        if ( pd[idx].ptr.present ) {
            return -1;
        }
        /* Not present, then map here */
        pd[idx].page.present = 1;
        pd[idx].page.page = 1;
        pd[idx].page.rw = rw;
        pd[idx].page.us = user;
        pd[idx].page.g = global;
        pd[idx].v |= physical;
        /* Invalidate */
        if ( MASK_PAGE(get_cr3()) == MASK_PAGE(pgt->cr3) ) {
            invlpg(virtual);
        }
    } else {
        if ( !pd[idx].ptr.present ) {
            /* Not present, then add a page here */
            pt = pgt_pop(pgt);
            if ( NULL == pt ) {
                return -1;
            }
            kmemset(pt, 0, 4096);
            pd[idx].ptr.present = 1;
            pd[idx].ptr.rw = 1;
            pd[idx].ptr.us = 1;
            pd[idx].v |= _v2p(pgt, (uint64_t)pt);
        } else {
            p = MASK_PAGE(pd[idx].v);
            pt = (void *)_p2v(pgt, p);
        }

        /* PT */
        idx = (virtual >> 12) & 0x1ff;
        if ( pt[idx].page.present ) {
            return -1;
        }
        /* Not present, then map here */
        pt[idx].page.present = 1;
        pt[idx].page.rw = rw;
        pt[idx].page.us = user;
        pt[idx].page.g = global;
        pt[idx].v |= physical;

        /* Invalidate */
        if ( MASK_PAGE(get_cr3()) == MASK_PAGE(pgt->cr3) ) {
            invlpg(virtual);
        }
    }

    return 0;
}

/*
 * Unmap the virtual address to the specified physical address
 */
int
pgt_unmap(pgt_t *pgt, uintptr_t virtual, int superpage)
{
    uintptr_t p;
    union pgt_pml4_entry *pml4;
    union pgt_pdpt_entry *pdpt;
    union pgt_pd_entry *pd;
    union pgt_pt_entry *pt;
    int idx;
    int i;

    /* Align */
    if ( superpage ) {
        virtual = virtual & ~((1ULL << 21) - 1);
    } else {
        virtual = virtual & ~((1ULL << 12) - 1);
    }

    /* PML4 */
    p = MASK_PAGE(pgt->cr3);
    idx = (virtual >> 39) & 0x1ff;
    pml4 = (void *)_p2v(pgt, p);
    if ( !pml4[idx].ptr.present ) {
        /* Not present */
        return -1;
    }

    /* PDPT */
    p = MASK_PAGE(pml4[idx].v);
    pdpt = (void *)_p2v(pgt, p);
    idx = (virtual >> 30) & 0x1ff;
    if ( !pdpt[idx].ptr.present ) {
        /* Not present */
        return -1;
    }

    /* PD */
    p = MASK_PAGE(pdpt[idx].v);
    pd = (void *)_p2v(pgt, p);
    idx = (virtual >> 21) & 0x1ff;
    if ( !pd[idx].ptr.present ) {
        /* Not present */
        return -1;
    }

    /* Release a superpage */
    if ( superpage ) {
        if ( !pd[idx].ptr.page ) {
            /* Not a page */
            return -1;
        }
        pd[idx].v = 0;

        /* Invalidate */
        if ( MASK_PAGE(get_cr3()) == MASK_PAGE(pgt->cr3) ) {
            invlpg(virtual);
        }
    } else {
        /* PT */
        p = MASK_PAGE(pd[idx].v);
        pt = (void *)_p2v(pgt, p);
        idx = (virtual >> 12) & 0x1ff;
        if ( !pt[idx].page.present ) {
            /* Not present */
            return -1;
        }
        pt[idx].v = 0;

        /* Invalidate */
        if ( MASK_PAGE(get_cr3()) == MASK_PAGE(pgt->cr3) ) {
            invlpg(virtual);
        }

        /* Check all entries of PT */
        for ( i = 0; i < 512; i++ ) {
            if ( pt[i].page.present ) {
                return 0;
            }
        }
        pgt_push(pgt, (void *)pt);
        idx = (virtual >> 21) & 0x1ff;
        pd[idx].v = 0;
    }

    /* Check all entries of PD */
    for ( i = 0; i < 512; i++ ) {
        if ( pd[i].ptr.present ) {
            return 0;
        }
    }
    pgt_push(pgt, (void *)pd);
    idx = (virtual >> 30) & 0x1ff;
    pdpt[idx].v = 0;

    /* Check all entries of PDPT */
    for ( i = 0; i < 512; i++ ) {
        if ( pdpt[i].ptr.present ) {
            return 0;
        }
    }
    pgt_push(pgt, (void *)pdpt);
    idx = (virtual >> 39) & 0x1ff;
    pml4[idx].v = 0;

    return 0;
}

/*
 * Invalidate page table
 */
void
pgt_invalidate(pgt_t *pgt, uintptr_t virtual)
{
    (void)pgt;
    invlpg(virtual);
}

/*
 * Set the cr3 register
 */
void
pgt_set_cr3(pgt_t *pgt)
{
    set_cr3(pgt->cr3);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
