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

#ifndef _ADVOS_KERNEL_MEMORY_H
#define _ADVOS_KERNEL_MEMORY_H

#include <stdint.h>

#define MEMORY_ZONE_UNKNOWN             -1
#define MEMORY_ZONE_DMA                 0
#define MEMORY_ZONE_KERNEL              1
#define MEMORY_ZONE_CORE_NUM            2
#define MEMORY_ZONE_NUMA_AWARE          2

#define MEMORY_ZONE_KERNEL_LB           0x01000000
#define MEMORY_ZONE_NUMA_AWARE_LB       0x04000000

#define MEMORY_PHYS_BUDDY_ORDER         18

#define MEMORY_PAGESIZE_SHIFT           12
#define MEMORY_PAGESIZE                 (1ULL << MEMORY_PAGESIZE_SHIFT)
#define MEMORY_SUPERPAGESIZE_SHIFT      21
#define MEMORY_SUPERPAGESIZE            (1ULL << MEMORY_SUPERPAGESIZE_SHIFT)

/*
 * Page
 */
struct _phys_memory_buddy_page {
    /* Linked list's next pointer (virtual address) */
    struct _phys_memory_buddy_page *next;
} __attribute__ ((packed));
typedef struct _phys_memory_buddy_page phys_memory_buddy_page_t;

/*
 * Physical memory zone
 */
typedef struct {
    /* Flag to note the zone is initialized or not */
    int valid;

    /* Head pointers to page blocks at each order of buddy system */
    phys_memory_buddy_page_t *heads[MEMORY_PHYS_BUDDY_ORDER + 1];
} phys_memory_zone_t;

/*
 * Physical memory management
 */
typedef struct {
    /* Offset to calculate virtual address from physical address */
    uintptr_t p2v;

    /* Core zones */
    phys_memory_zone_t czones[MEMORY_ZONE_CORE_NUM];

    /* NUMA zones */
    int max_domain;
    phys_memory_zone_t *numazones;
} phys_memory_t;

/*
 * System memory map entry
 */
typedef struct {
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t attr;
} __attribute__ ((packed)) memory_sysmap_entry_t;

/*
 * Page
 */
typedef struct page page_t;
struct page {
    /* Physical address */
    uintptr_t physical;
    /* Zone of the buddy system */
    uint8_t zone;
    /* Order of the buddy system */
    uint8_t order;
    /* Numa-domain if available */
    uint32_t numadomain;
    /* Pointer to the next page in the same object */
    page_t *next;
};

/*
 * Object
 */
typedef struct virt_memory_object virt_memory_object_t;
struct virt_memory_object {
    /* Pointer to the list of pages */
    page_t *pages;
    /* Size */
    size_t size;
};

/*
 * Virtual memory entry (allocated)
 */
typedef struct virt_memory_entry virt_memory_entry_t;
struct virt_memory_entry {
    /* Start virtual address of this entry */
    uintptr_t start;
    /* Size */
    size_t size;
    /* Pointer to the object */
    virt_memory_object_t *object;

    /* Binary tree for start address ordering */
    struct {
        virt_memory_entry_t *left;
        virt_memory_entry_t *right;
    } atree;
};

/*
 * Free spacw management
 */
typedef struct virt_memory_free virt_memory_free_t;
struct virt_memory_free {
    /* Start address of this space */
    uintptr_t start;
    /* Size of this space */
    size_t size;

    /* Binary tree for start address ordering */
    struct {
        virt_memory_free_t *left;
        virt_memory_free_t *right;
    } atree;
    /* Binary tree for size ordering */
    struct {
        virt_memory_free_t *left;
        virt_memory_free_t *right;
    } stree;
};

/*
 * Memory block
 */
typedef struct virt_memory_block virt_memory_block_t;
struct virt_memory_block {
    /* Start virtual address of this block */
    uintptr_t start;
    /* Length of this block */
    size_t length;
    /* Pointer to the next block */
    virt_memory_block_t *next;

    /* Allocated entries */
    virt_memory_entry_t *entries;

    /* Free space list */
    virt_memory_free_t *frees;

    /* Architecture-specific defintions */
    void *arch;
    void (*map)(void *, page_t *, uintptr_t);
};

/*
 * Data structure used for virtual memory management
 */
union virt_memory_data {
    page_t page;
    virt_memory_object_t object;
    virt_memory_entry_t entry;
    virt_memory_free_t free;
};

/*
 * Memory
 */
typedef struct {
    /* List of blocks */
    virt_memory_block_t *blocks;
} memory_t;

void
phys_mem_buddy_add_region(phys_memory_buddy_page_t **, uintptr_t, uintptr_t);
void * phys_mem_buddy_alloc(phys_memory_buddy_page_t **, int);
void phys_mem_buddy_free(phys_memory_buddy_page_t **, void *, int);
int phys_memory_init(phys_memory_t *, int, memory_sysmap_entry_t *, uint64_t);

page_t * memory_alloc_pages(memory_t *, size_t);
void memory_free_pages(memory_t *, page_t *);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
