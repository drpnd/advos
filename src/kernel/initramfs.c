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
 * inode
 */
struct initramfs_inode {
    uint64_t offset;
};

/*
 * File system device
 */
struct initramfs_device {
    void *base;
};

/*
 * File system module
 */
struct initramfs_module {
    int lock;
};

#define INITRAMFS_TYPE          "initramfs"
#define INITRAMFS_BASE          0xc0030000

#define INITRAMFS_ATTR_DIR      0x01

vfs_mount_spec_t *
initramfs_mount(vfs_module_spec_t *, int , void *);
vfs_vnode_t * initramfs_lookup(vfs_mount_spec_t *, vfs_vnode_t *, const char *);

static struct initramfs_module initramfs;

/*
 * Initialize initramfs module
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
    ifs.lookup = initramfs_lookup;
    ret = vfs_register("initramfs", &ifs, NULL);
    if ( ret < 0 ) {
        return -1;
    }
    initramfs.lock = 0;

    return 0;
}

/*
 * Mount initramfs
 */
vfs_mount_spec_t *
initramfs_mount(vfs_module_spec_t *spec, int flags, void *data)
{
    struct initramfs_device *fs;

    /* Rootfs */
    fs = kmalloc(sizeof(struct initramfs_device));
    if ( NULL == fs ) {
        return NULL;
    }
    fs->base = (void *)INITRAMFS_BASE;

    return (vfs_mount_spec_t *)fs;
}

/*
 * Lookup an entry
 */
vfs_vnode_t *
initramfs_lookup(vfs_mount_spec_t *spec, vfs_vnode_t *parent, const char *name)
{
    struct initramfs_device *fs;
    struct initrd_entry *e;
    struct initramfs_inode *in;
    vfs_vnode_t *vnode;
    int i;

    fs = (struct initramfs_device *)spec;

    /* Search the specified file */
    e = (void *)INITRAMFS_BASE;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(name, e->name) ) {
            /* Found, then create an inode data structure */
            vnode = vfs_vnode_alloc();
            if ( NULL == vnode ) {
                return NULL;
            }
            in = (struct initramfs_inode *)&vnode->inode;
            in->offset = e->u.file.offset;
            if ( e->attr & INITRAMFS_ATTR_DIR ) {
                /* Directory */
                return vnode;
            } else {
                /* File */
                return vnode;
            }
        }
        e++;
    }

    return NULL;
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
