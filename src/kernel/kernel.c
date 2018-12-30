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

#include "kasm.h"
#include "kernel.h"
#include "memory.h"
#include <stdint.h>

/*
 * kstrcmp
 */
int
kstrcmp(const char *s1, const char *s2)
{
    size_t i;
    int diff;

    i = 0;
    while ( s1[i] != '\0' || s2[i] != '\0' ) {
        diff = s1[i] - s2[i];
        if ( diff ) {
            return diff;
        }
        i++;
    }

    return 0;
}

/*
 * kstrncmp
 */
int
kstrncmp(const char *s1, const char *s2, size_t n)
{
    size_t i;
    int diff;

    i = 0;
    while ( (s1[i] != '\0' || s2[i] != '\0') && i < n ) {
        diff = s1[i] - s2[i];
        if ( diff ) {
            return diff;
        }
        i++;
    }

    return 0;
}

/*
 * kstrcpy
 */
char *
kstrcpy(char *dst, const char *src)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = src[i];

    return dst;
}

/*
 * kstrncpy
 */
char *
kstrncpy(char *dst, const char *src, size_t n)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' && i < n ) {
        dst[i] = src[i];
        i++;
    }
    for ( ; i < n; i++ ) {
        dst[i] = '\0';
    }

    return dst;
}

/*
 * kstrlcpy
 */
size_t
kstrlcpy(char *dst, const char *src, size_t n)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' && i < n - 1 ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';

    while ( '\0' != src[i] ) {
        i++;
    }

    return i;
}

/*
 * kmalloc
 */
void *
kmalloc(size_t sz)
{
    static int kmalloc_sizes[] = { 8, 16, 32, 64, 96, 128, 192, 256, 512, 1024,
                                   2048, 4096, 8192 };
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

    return memory_slab_alloc(NULL, cachename);
}

/*
 * kfree
 */
void
kfree(void *obj)
{
    /* Search the slab cache corresponding to the specified object */
    (void)obj;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
