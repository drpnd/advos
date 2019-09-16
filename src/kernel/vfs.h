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
#include "proc.h"
#include <sys/stat.h>

#define VFS_MAXTYPE     64
#define VFS_MAXFS       32

typedef struct _vfs_vnode vfs_vnode_t;

/*
 * Virtual filesystem interfaces
 */
typedef struct {
    int (*open)(fildes_t *, const char *, int, ...);
    int (*close)(fildes_t *);
    int (*fstat)(fildes_t *, struct stat *);
    ssize_t (*read)(fildes_t *, void *, size_t);
    ssize_t (*write)(fildes_t *, const void *, size_t);
    ssize_t (*readfile)(const char *, char *, size_t, off_t);
    int (*mount)(void *, const char *, int, void *);
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
typedef struct {
    /* Vnode of the mount point */
    vfs_vnode_t *vnode;
    /* Filesystem specific information */
    vfs_module_t *module;
    /* Cache */
    vfs_vnode_t *vnode_cache;
} vfs_mount_t;

/*
 * vnode
 */
struct _vfs_vnode {
    /* Inode information */
    void *inode;
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

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
