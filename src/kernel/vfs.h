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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _ADVOS_VFS_H
#define _ADVOS_VFS_H

#include "kernel.h"
#include <sys/stat.h>

#define VFS_MAXTYPE     64
#define VFS_MAXFS       32

#define VFS_FILE        0
#define VFS_DIR         1

typedef struct _vfs_vnode vfs_vnode_t;
typedef struct _vfs_inode_storage vfs_inode_storage_t;
typedef void vfs_module_spec_t;
typedef void vfs_mount_spec_t;
typedef struct _vfs_mount vfs_mount_t;

/*
 * Virtual filesystem interfaces
 */
typedef struct {
    /* Module operation */
    vfs_mount_spec_t * (*mount)(vfs_module_spec_t *, int, void *);
    /* Mounted filesystem operations */
    int (*unmount)(vfs_mount_spec_t *, int);
    /* Lookup */
    vfs_vnode_t * (*lookup)(vfs_mount_t *, vfs_vnode_t *, const char *);
    /* Name creation */
    vfs_vnode_t * (*create)(vfs_mount_t *, vfs_vnode_t *, const char *);
    vfs_vnode_t * (*mknod)(vfs_mount_t *, vfs_vnode_t *, const char *);
    vfs_vnode_t * (*link)(vfs_mount_t *, vfs_vnode_t *, const char *);
    vfs_vnode_t * (*symlink)(vfs_mount_t *, vfs_vnode_t *, const char *);
    vfs_vnode_t * (*mkdir)(vfs_mount_t *, vfs_vnode_t *, const char *);
    /* Name change/deletion */
    int (*rename)(vfs_mount_t *, vfs_vnode_t *, const char *);
    int (*remove)(vfs_mount_t *, vfs_vnode_t *, const char *);
    int (*rmdir)(vfs_mount_t *, vfs_vnode_t *, const char *);
    /* Attribute maniplulation */
    vfs_vnode_t * (*access)(vfs_mount_t *, vfs_vnode_t *, int, int *);
    vfs_vnode_t * (*getattr)(vfs_mount_t *, vfs_vnode_t *, int, int *);
    vfs_vnode_t * (*setattr)(vfs_mount_t *, vfs_vnode_t *, int, int *);
    /* Object interpretation */
    vfs_vnode_t * (*open)(vfs_mount_t *, vfs_vnode_t *, const char *);
    vfs_vnode_t * (*readdir)(vfs_mount_t *, vfs_vnode_t *);
    vfs_vnode_t * (*readlink)(vfs_mount_t *, vfs_vnode_t *);
    vfs_vnode_t * (*mmap)(vfs_mount_t *, vfs_vnode_t *);
    vfs_vnode_t * (*close)(vfs_mount_t *, vfs_vnode_t *);
    /* Process control (+advlock) */
    int (*ioctl)(vfs_mount_t *, vfs_vnode_t *, int, void *);
    int (*poll)(vfs_mount_t *, vfs_vnode_t *);
    /* Object management (+inactive, reclaim) */
    int (*lock)(vfs_mount_t *, vfs_vnode_t *);
    int (*unlock)(vfs_mount_t *, vfs_vnode_t *);
} vfs_interfaces_t;

/*
 * Virtual filesystem module (filesystem-specific information)
 */
typedef struct {
    void *spec;
    char type[VFS_MAXTYPE];
    vfs_interfaces_t ifs;
} vfs_module_t;

/*
 * Virtual filesystem mount point
 */
struct _vfs_mount {
    /* Filesystem specific data structure */
    void *spec;
    /* Vnode of the mount point */
    vfs_vnode_t *vnode;
    /* Filesystem specific information */
    vfs_module_t *module;
    /* Cache */
    vfs_vnode_t *vnode_cache;
};

/*
 * Inode storage
 */
struct _vfs_inode_storage {
    union {
        void *ptr;
        uint8_t storage[96];
    } u;
};

/*
 * vnode
 */
struct _vfs_vnode {
    /* Inode information */
    vfs_inode_storage_t inode;
    /* Flags */
    int flags;
    /* Module */
    vfs_module_t *module;
    /* Mount data structure if this vnode is a mount point */
    vfs_mount_t *mount;
    /* Linked list */
    vfs_vnode_t *next;
};

/*
 * Virtual filesystem
 */
typedef struct {
    vfs_module_t *modules[VFS_MAXFS];
} vfs_t;

/* Prototype declarations */
int vfs_init(void);
int vfs_register(const char *, vfs_interfaces_t *, void *);
int vfs_mount(const char *, const char *, int, void *);
vfs_vnode_t * vfs_vnode_alloc(void);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
