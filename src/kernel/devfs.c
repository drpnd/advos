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

#include "devfs.h"
#include "proc.h"
#include <mki/driver.h>

/*
 * File descriptor
 */
struct devfs_fildes {
    struct devfs_entry *entry;
};

/*
 * devfs entry
 */
struct devfs_entry {
    /* Name of the entry */
    char *name;
    /* Flags */
    int flags;
    /* Device */
    driver_device_t *device;
    /* Owner process (driver) */
    proc_t *proc;
    /* Pointer to the next entry */
    struct devfs_entry *next;
};

/*
 * devfs
 */
struct devfs {
    struct devfs_entry *head;
};

#define DEVFS_FILDES_SLAB       "devfs_fildes"

struct devfs devfs;

/*
 * Initialize devfs
 */
int
devfs_init(void)
{
    int ret;

    devfs.head = NULL;

    /* Allocate the process slab */
    ret = kmem_slab_create_cache(DEVFS_FILDES_SLAB,
                                 sizeof(struct devfs_fildes));
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * read
 */
ssize_t
devfs_read(void *fildes, void *buf, size_t nbyte)
{
    struct devfs_fildes *spec;
    ssize_t len;
    int c;
    task_t *t;
    task_list_t *tle;

    /* Get the currently running task */
    t = this_task();
    if ( NULL == t ) {
        return -1;
    }

    spec = (struct devfs_fildes *)fildes;
    switch ( spec->entry->device->type ) {
    case DRIVER_DEVICE_CHAR:
        /* Character device */
        while ( 0 == driver_chr_ibuf_length(spec->entry->device) ) {
            /* Empty buffer, then add this task to the blocking task list for
               this file descriptor and switch to another task. */
            t->state = TASK_BLOCKED;

            /* Set this task to the blocking task list of the file descriptor */
            tle = kmem_slab_alloc(SLAB_TASK_LIST);
            if ( NULL == tle ) {
                return -1;
            }
            tle->task = t;
            tle->next = NULL;

            /* Switch to another task */

            /* Will resume from this point */
        }

        len = 0;
        while ( len < (ssize_t)nbyte ) {
            if ( (c = driver_chr_ibuf_getc(spec->entry->device)) < 0 ) {
                /* No buffer available */
                break;
            }
            *(char *)(buf + len) = c;
            len++;
        }

        return len;
    case DRIVER_DEVICE_BLOCK:
        /* Block device */
        break;
    default:
        return -1;
    }

    return -1;
}

/*
 * write
 */
ssize_t
devfs_write(void *fildes, const void *buf, size_t nbyte)
{
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
