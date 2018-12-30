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
 * Allocate a slab
 */
static memory_slab_hdr_t *
_new_slab(memory_slab_allocator_t *slab, size_t objsize)
{
    void *pages;
    uintptr_t addr;
    memory_slab_hdr_t *hdr;
    size_t size;
    int i;

    /* Allocate pages for slab */
    pages = memory_alloc_pages(slab->mem, MEMORY_SLAB_NUM_PAGES,
                               MEMORY_ZONE_NUMA_AWARE, 0);
    if ( NULL == pages ) {
        return NULL;
    }

    size = MEMORY_PAGESIZE * MEMORY_SLAB_NUM_PAGES;
    kmemset(pages, 0, size);
    hdr = pages;

    hdr->next = NULL;
    hdr->cache = NULL;

    /* Calculate the number of objects (64 byte alignment) */
    size = size - sizeof(memory_slab_hdr_t) - MEMORY_SLAB_ALIGNMENT;
    /* object and mark */
    hdr->nobjs = size / (objsize + 1);

    /* Calculate the pointer to the object array */
    addr = (uintptr_t)pages + sizeof(memory_slab_hdr_t) + hdr->nobjs;
    addr = ((addr + MEMORY_SLAB_ALIGNMENT - 1) / MEMORY_SLAB_ALIGNMENT)
        * MEMORY_SLAB_ALIGNMENT;
    hdr->obj_head = (void *)addr;
    /* Mark all objects free */
    for ( i = 0; i < hdr->nobjs; i++ ) {
        hdr->marks[i] = 1;
    }

    return hdr;
}

/*
 * Find the corresponding cache
 */
static memory_slab_cache_t *
_find_slab_cache(memory_slab_cache_t *n, const char *name)
{
    int ret;

    ret = kstrncmp(n->name, name, MEMORY_SLAB_CACHE_NAME_MAX);
    if ( 0 == ret ) {
        return n;
    } else if ( ret < 0 ) {
        /* Search left */
        return _find_slab_cache(n->left, name);
    } else {
        /* Search right */
        return _find_slab_cache(n->right, name);
    }
}

/*
 * Add a slab cache
 */
static int
_add_slab_cache(memory_slab_cache_t **t, memory_slab_cache_t *n)
{
    int ret;

    if ( NULL == *t ) {
        /* Insert here */
        *t = n;
        return 0;
    }

    /* Compare and search */
    ret = kstrncmp((*t)->name, n->name, MEMORY_SLAB_CACHE_NAME_MAX);
    if ( 0 == ret ) {
        return -1;
    } else if ( ret < 0 ) {
        /* Search left */
        return _add_slab_cache(&(*t)->left, n);
    } else {
        /* Search right */
        return _add_slab_cache(&(*t)->right, n);
    }
}


/*
 * Allocate an object from the slab cache specified by the name
 */
void *
memory_slab_alloc(memory_slab_allocator_t *slab, const char *name)
{
    memory_slab_cache_t *c;
    memory_slab_hdr_t *s;
    int i;
    void *obj;

    /* Find the slab cache corresponding to the name */
    c = _find_slab_cache(slab->root, name);
    if ( NULL == c ) {
        /* Not found */
        return NULL;
    }

    if ( NULL == c->freelist.partial && NULL == c->freelist.full ) {
        /* No object found, then try to allocate a new slab */
        s = _new_slab(slab, c->size);
        if ( NULL == s ) {
            /* Could not allocate a new slab */
            return NULL;
        }
        s->cache = c;
        c->freelist.full = s;
    }

    if ( NULL == c->freelist.partial ) {
        kassert( c->freelist.full != NULL );
        /* Take one full slab to the partial */
        c->freelist.partial = c->freelist.full;
        c->freelist.full = c->freelist.full->next;
    }

    /* Get an object */
    s = c->freelist.partial;
    obj = NULL;
    for ( i = 0; i < s->nobjs; i ++ ) {
        if ( s->marks[i] ) {
            /* Found a free object */
            obj = s->obj_head + c->size * i;
            s->marks[i] = 0;
            s->nused++;
            break;
        }
    }

    /* Check if the slab is still partial or becomes empty */
    if ( s->nused == s->nobjs ) {
        /* Move this partial slab to the empty free list */
        c->freelist.partial = s->next;
        s->next = c->freelist.empty;
        c->freelist.empty = s;
    }

    return obj;
}

/*
 * Create a new slab cache
 */
int
memory_slab_create_cache(memory_slab_allocator_t *slab, const char *name,
                         size_t size)
{
    memory_slab_cache_t *cache;
    memory_slab_hdr_t *s;
    int ret;

    /* Duplicate check */
    cache = memory_slab_alloc(slab, name);
    if ( NULL != cache ) {
        /* Already exists */
        return -1;
    }

    /* Try to allocate a memory_slab_cache_t from the named slab cache */
    cache = memory_slab_alloc(slab, MEMORY_SLAB_CACHE_NAME);
    if ( NULL == cache ) {
        return -1;
    }
    kstrlcpy(cache->name, name, MEMORY_SLAB_CACHE_NAME_MAX);
    cache->size = size;
    cache->freelist.partial = NULL;
    cache->freelist.full = NULL;
    cache->freelist.empty = NULL;
    cache->left = NULL;
    cache->right = NULL;

    /* Allocate one slab for the full free list */
    s = _new_slab(slab, size);
    s->cache = cache;
    cache->freelist.full = s;

    /* Add to the cache tree */
    ret = _add_slab_cache(&slab->root, cache);
    kassert( ret == 0 );

    return 0;
}

/*
 * Prepare a slab_cache slab cache
 */
static int
_slab_cache_init(memory_slab_allocator_t *slab)
{
    memory_slab_hdr_t *s;
    memory_slab_cache_t *cache;
    int ret;

    /* Allocate one slab for the full free list */
    s = _new_slab(slab, sizeof(memory_slab_cache_t));
    if ( NULL == s ) {
        return -1;
    }

    /* Take the first object */
    s->marks[0] = 0;
    s->nused++;
    cache = s->obj_head;
    kstrlcpy(cache->name, MEMORY_SLAB_CACHE_NAME, MEMORY_SLAB_CACHE_NAME_MAX);
    cache->size = sizeof(memory_slab_cache_t);
    cache->freelist.partial = s;
    cache->freelist.full = NULL;
    cache->freelist.empty = NULL;
    cache->left = NULL;
    cache->right = NULL;
    s->cache = cache;

    /* Add to the cache tree */
    ret = _add_slab_cache(&slab->root, cache);
    kassert( ret == 0 );

    return 0;
}


/*
 * Initialize the slab allocator
 */
int
memory_slab_init(memory_slab_allocator_t *slab, memory_t *mem)
{
    int ret;

    slab->mem = mem;
    slab->root = NULL;

    /* Create a slab cache for slab cache */
    ret = _slab_cache_init(slab);
    if ( ret < 0 ) {
        return -1;
    }

#if 0
    static int kmalloc_sizes[] = { 8, 16, 32, 64, 96, 128, 192, 256, 512, 1024,
                                   2048, 4096, 8192 };
    ncaches = sizeof(kmalloc_sizes) / sizeof(int);
#endif
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
