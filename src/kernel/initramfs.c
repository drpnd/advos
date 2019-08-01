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

#include "vfs.h"
#include "kernel.h"
#include "memory.h"

/*
 * initrd
 */
struct initrd_entry {
    char name[16];
    uint64_t offset;
    uint64_t size;
};

struct initramfs_fildes {
    int inode;
    uint64_t offset;
    uint64_t size;
};

#define INITRAMFS_BASE          0xc0030000
#define INITRAMFS_FILDES_SLAB   "initramfs_fildes"

/*
 * Initialize initramfs
 */
int
initramfs_init(void)
{
    int ret;

    /* Allocate the process slab */
    ret = kmem_slab_create_cache(INITRAMFS_FILDES_SLAB,
                                 sizeof(struct initramfs_fildes));
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * open
 */
void *
initramfs_open(const char *path, int oflag, ...)
{
    struct initramfs_fildes *fildes;
    struct initrd_entry *e;
    int i;

    /* Allocate a VFS-specific file descriptor */
    fildes = kmem_slab_alloc(INITRAMFS_FILDES_SLAB);
    if ( NULL == fildes ) {
        return NULL;
    }

    /* Search the specified file */
    e = (void *)INITRAMFS_BASE;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(path, e->name) ) {
            /* Found */
            fildes->inode = i;
            fildes->offset = e->offset;
            fildes->size = e->size;
            return fildes;
        }
        e++;
    }

    /* Not found */
    kmem_slab_free(INITRAMFS_FILDES_SLAB, fildes);

    return NULL;
}

/*
 * close
 */
int
initramfs_close(void *fildes)
{
    kmem_slab_free(INITRAMFS_FILDES_SLAB, fildes);

    return 0;
}

/*
 * fstat
 */
int
initramfs_fstat(void *fildes, struct stat *buf)
{
    struct initramfs_fildes *spec;

    spec = (struct initramfs_fildes *)fildes;
    kmemset(buf, 0, sizeof(struct stat));
    buf->st_size = spec->size;

    return 0;
}

/*
 * readfile
 */
ssize_t
initramfs_readfile(const char *path, char *buf, size_t size, off_t off)
{
    struct initrd_entry *e;
    char *ptr;
    int i;

    e = (void *)INITRAMFS_BASE;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(path, e->name) ) {
            /* Found */
            ptr = (void *)INITRAMFS_BASE + e->offset;
            if ( (off_t)e->size <= off ) {
                /* No data to copy */
                return 0;
            }
            if ( e->size - off > size ) {
                /* Exceed the buffer size, then copy the buffer-size data */
                kmemcpy(buf, ptr + off, size);
                return size;
            } else {
                /* Copy  */
                kmemcpy(buf, ptr + off, e->size - off);
                return e->size - off;
            }
        }
        e++;
    }

    /* Not found */
    return -1;
}

vfs_interfaces_t initramfs = {
    .open = initramfs_open,
    .close = initramfs_close,
    .fstat = initramfs_fstat,
    .readfile = initramfs_readfile,
};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
