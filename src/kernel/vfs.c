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

#include "kernel.h"
#include "vfs.h"

#define SLAB_VFS_ENTRY      "vfs_entry"

/*
 * Initialize the vfs
 */
int
vfs_init(void)
{
    int ret;

    ret = kmem_slab_create_cache(SLAB_VFS_ENTRY, sizeof(vfs_entry_t));
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Get directory
 */
char *
vfs_open(const char *path)
{
    const char *dir;

    /* Resolve the filesystem */
    dir = path;
    while ( '\0' != *path ) {
        if ( '/' == *path ) {
            /* Delimiter */
            break;
        }
        path++;
    }

    return NULL;
}

/*
 * Register filesystem
 */
int
vfs_register(const char *type, vfs_interfaces_t *ifs)
{
    vfs_entry_t *e;

    if ( kstrlen(type) >= VFS_MAXTYPE ) {
        return -1;
    }

    /* Allocate a vfs entry */
    e = kmem_slab_alloc(SLAB_VFS_ENTRY);
    if ( NULL == e ) {
        return -1;
    }

    return -1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
