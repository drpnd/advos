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
#include "kvar.h"

/*
 * initrd
 */
struct initrd_entry {
    char name[15];
    uint8_t attr;
    union {
        struct {
            uint64_t offset;
            uint64_t size;
        } file;
        struct {
            uint64_t offset;
            uint64_t reserved;
        } dir;
    } u;
};

/*
 * File descriptor
 */
struct initramfs_fildes {
    int inode;
    uint64_t offset;
    uint64_t size;
};

/*
 * File system
 */
struct initramfs {
    void *base;
};

#define INITRAMFS_TYPE          "initramfs"
#define INITRAMFS_BASE          0xc0030000

int initramfs_mount(void *, const char *, int , void *);

/*
 * Initialize initramfs
 */
int
initramfs_init(void)
{
    int ret;
    vfs_interfaces_t ifs;

    /* Ensure the filesystem-specific data structure is smaller than
       fildes_storage_t */
    if ( sizeof(fildes_storage_t) < sizeof(struct initramfs_fildes) ) {
        return -1;
    }

    /* Register initramfs to the virtual filesystem management */
    kmemset(&ifs, 0, sizeof(vfs_interfaces_t));
    ifs.mount = initramfs_mount;
    ret = vfs_register("initramfs", &ifs, NULL);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Mount initramfs
 */
int
initramfs_mount(void *spec, const char *mp, int flags, void *data)
{
    struct initramfs *fs;

    if ( 0 == kstrcmp(mp, "/") ) {
        /* Rootfs */
        fs = kmalloc(sizeof(struct initramfs));
        if ( NULL == fs ) {
            return -1;
        }
        fs->base = (void *)INITRAMFS_BASE;

#if 0
        if ( NULL != g_kvar->rootfs ) {
            /* Already mounted */
            return -1;
        }
        g_kvar->rootfs = fs;
#endif
        return 0;
    } else {
        /* ToDo: Search the mount point */
        return -1;
    }
}

/*
 * Find an entry
 */
int
initramfs_find(void *spec, vfs_inode_storage_t *inode, const char *name)
{
    struct initrd_entry *e;
    int i;

    /* Search the specified file */
    e = (void *)INITRAMFS_BASE;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(name, e->name) ) {
            /* Found, then create an inode data structure */
            return 0;
        }
        e++;
    }

    return -1;
}

/*
 * open
 */
int
initramfs_open(fildes_t *fildes, const char *path, int oflag, ...)
{
    struct initramfs_fildes *spec;
    struct initrd_entry *e;
    int i;

#if 0
    /* Allocate a VFS-specific file descriptor */
    fildes = kmem_slab_alloc(SLAB_FILDES);
    if ( NULL == fildes ) {
        return NULL;
    }
    fildes->head = NULL;
    fildes->refs = 1;
#endif

    /* Search the specified file */
    e = (void *)INITRAMFS_BASE;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(path, e->name) ) {
            /* Found */
            spec = (struct initramfs_fildes *)&fildes->fsdata;
            spec->inode = i;
            spec->offset = e->u.file.offset;
            spec->size = e->u.file.size;
            return 0;
        }
        e++;
    }

    /* Not found */
    kmem_slab_free(SLAB_FILDES, fildes);

    return -1;
}

/*
 * close
 */
int
initramfs_close(fildes_t *fildes)
{
    kmem_slab_free(SLAB_FILDES, fildes);

    return 0;
}

/*
 * fstat
 */
int
initramfs_fstat(fildes_t *fildes, struct stat *buf)
{
    struct initramfs_fildes *spec;

    spec = (struct initramfs_fildes *)&fildes->fsdata;
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
            ptr = (void *)INITRAMFS_BASE + e->u.file.offset;
            if ( (off_t)e->u.file.size <= off ) {
                /* No data to copy */
                return 0;
            }
            if ( e->u.file.size - off > size ) {
                /* Exceed the buffer size, then copy the buffer-size data */
                kmemcpy(buf, ptr + off, size);
                return size;
            } else {
                /* Copy  */
                kmemcpy(buf, ptr + off, e->u.file.size - off);
                return e->u.file.size - off;
            }
        }
        e++;
    }

    /* Not found */
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
