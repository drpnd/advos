/*_
 * Copyright (c) 2019 Hirochika Asai <asai@jar.jp>
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

#include "memory.h"
#include "kernel.h"

static int kmalloc_sizes[] = { 8, 16, 32, 64, 96, 128, 192, 256, 512, 1024,
                               2048, 4096, 8192 };

static memory_slab_allocator_t *_slab;

/*
 * Initialize kmalloc()
 */
int
kmalloc_init(memory_slab_allocator_t *slab)
{
    int i;
    int ret;
    char cachename[MEMORY_SLAB_CACHE_NAME_MAX];

    _slab = slab;

    /* Initialize the kmalloc slab caches */
    for ( i = 0; i < (int)(sizeof(kmalloc_sizes) / sizeof(int)); i++ ) {
        ksnprintf(cachename, MEMORY_SLAB_CACHE_NAME_MAX, "kmalloc-%d",
                  kmalloc_sizes[i]);
        ret = memory_slab_create_cache(slab, cachename, kmalloc_sizes[i]);
        if ( ret < 0 ) {
            return -1;
        }
    }
    return 0;
}

/*
 * kmalloc
 */
void *
kmalloc(size_t sz)
{
    char cachename[MEMORY_SLAB_CACHE_NAME_MAX];
    size_t i;
    int aligned_size;

    /* Search fitting size */
    aligned_size = -1;
    for ( i = 0; i < sizeof(kmalloc_sizes) / sizeof(int); i++ ) {
        if ( (int)sz <= kmalloc_sizes[i] ) {
            aligned_size = kmalloc_sizes[i];
            break;
        }
    }
    if ( aligned_size < 0 ) {
        /* The requested size is too large. */
        return NULL;
    }
    ksnprintf(cachename, MEMORY_SLAB_CACHE_NAME_MAX, "kmalloc-%d",
              aligned_size);

    return memory_slab_alloc(_slab, cachename);
}

/*
 * kfree
 */
void
kfree(void *obj)
{
    int ret;
    char cachename[MEMORY_SLAB_CACHE_NAME_MAX];
    size_t i;

    /* Search the slab cache name corresponding to the object */
    for ( i = 0; i < sizeof(kmalloc_sizes) / sizeof(int); i++ ) {
        ksnprintf(cachename, MEMORY_SLAB_CACHE_NAME_MAX, "kmalloc-%d",
                  kmalloc_sizes[i]);
        ret = memory_slab_free(_slab, cachename, obj);
        if ( 0 == ret ) {
            /* Found */
            break;
        }
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
