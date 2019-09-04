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
#include "msg.h"
#include <mki/driver.h>

#define SLAB_DEVFS_ENTRY    "devfs_entry"
#define DEVFS_FIFO_BUFSIZE  8192

/*
 * Ring buffer
 */
struct devfs_fifo {
    uint8_t buf[DEVFS_FIFO_BUFSIZE];
    volatile off_t head;
    volatile off_t tail;
};

/*
 * Device file
 */
struct devfs_device_chr {
    struct devfs_fifo ibuf;
    struct devfs_fifo obuf;
};
struct devfs_device {
    int type;
    union {
        struct devfs_device_chr chr;
    } dev;
};

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
    char name[PATH_MAX];
    /* Flags */
    int flags;
    /* Device */
    struct devfs_device device;
    /* Owner process (driver) */
    proc_t *proc;
};

/*
 * devfs
 */
struct devfs {
    struct devfs_entry *head;
    struct devfs_entry *entries[DEVFS_MAXDEVS];
};

struct devfs devfs;

/*
 * Put one character to the input buffer
 */
static __inline__ int
_chr_ibuf_putc(struct devfs_device *dev, int c)
{
    off_t cur;
    off_t next;

    __sync_synchronize();

    cur = dev->dev.chr.ibuf.tail;
    next = cur + 1 < SYSDRIVER_DEV_BUFSIZE ? cur + 1 : 0;

    if  ( dev->dev.chr.ibuf.head == next ) {
        /* Buffer is full */
        return -1;
    }

    dev->dev.chr.ibuf.buf[cur] = c;
    dev->dev.chr.ibuf.tail = next;

    __sync_synchronize();

    return c;
}

/*
 * Get one character from the input buffer
 */
static __inline__ int
_chr_ibuf_getc(struct devfs_device *dev)
{
    int c;
    off_t cur;
    off_t next;

    __sync_synchronize();

    if  ( dev->dev.chr.ibuf.head == dev->dev.chr.ibuf.tail ) {
        /* Buffer is empty */
        return -1;
    }
    cur = dev->dev.chr.ibuf.head;
    next = cur + 1 < SYSDRIVER_DEV_BUFSIZE ? cur + 1 : 0;

    c = dev->dev.chr.ibuf.buf[cur];
    dev->dev.chr.ibuf.head = next;

    __sync_synchronize();

    return c;
}

/*
 * Get the queued length for the input buffer of a character device
 */
static __inline__ int
_chr_ibuf_length(struct devfs_device *dev)
{
    __sync_synchronize();

    if ( dev->dev.chr.ibuf.tail >= dev->dev.chr.ibuf.head ) {
        return dev->dev.chr.ibuf.tail - dev->dev.chr.ibuf.head;
    } else {
        return SYSDRIVER_DEV_BUFSIZE + dev->dev.chr.ibuf.tail
            - dev->dev.chr.ibuf.head;
    }
}

/*
 * Initialize devfs
 */
int
devfs_init(void)
{
    int ret;
    int i;

    for ( i = 0; i < DEVFS_MAXDEVS; i++ ) {
        devfs.entries[i] = NULL;
    }

    /* Ensure the filesystem-specific data structure is smaller than
       fildes_storage_t */
    if ( sizeof(fildes_storage_t) < sizeof(struct devfs_fildes) ) {
        return -1;
    }

    /* Prepare devfs slab */
    ret = kmem_slab_create_cache(SLAB_DEVFS_ENTRY, sizeof(struct devfs_entry));
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Add an entry
 */
int
devfs_register(const char *name, int type, proc_t *proc)
{
    struct devfs_entry *e;
    int i;

    /* Check the device type */
    if ( DEVFS_CHAR != type && DEVFS_BLOCK != type ) {
        return -1;
    }

    for ( i = 0; i < DEVFS_MAXDEVS; i++ ) {
        if ( NULL == devfs.entries[i] ) {
            /* Available */
            break;
        }
    }
    if ( i >= DEVFS_MAXDEVS ) {
        /* No available entry */
        return -1;
    }

    /* Allocate an entry */
    e = kmem_slab_alloc(SLAB_DEVFS_ENTRY);
    if ( NULL == e ) {
        return -1;
    }
    kstrlcpy(e->name, name, PATH_MAX);
    e->device.type = type;
    e->flags = 0;
    e->proc = proc;
    devfs.entries[i] = e;

    return i;
}

/*
 * Remove an entry
 */
int
devfs_unregister(int index, proc_t *proc)
{
    struct devfs_entry *e;

    /* Range check */
    if ( index >= DEVFS_MAXDEVS ) {
        return -1;
    }

    e = devfs.entries[index];
    if ( NULL == e ) {
        return -1;
    }
    if ( proc != e->proc ) {
        return -1;
    }

    kmem_slab_free(SLAB_DEVFS_ENTRY, e);
    devfs.entries[index] = NULL;

    return 0;
}


/*
 * Message handler
 */
int
devfs_recv_msg(int index, proc_t *proc, msg_t *msg)
{
    struct devfs_entry *e;

    /* Range check */
    if ( index >= DEVFS_MAXDEVS ) {
        return -1;
    }

    /* Search the devfs_entry corresponding to the name */
    e = devfs.entries[index];
    if ( NULL == e ) {
        /* Not found */
        return -1;
    }

    /* Check the process */
    if ( proc != e->proc ) {
        /* Message from a non-owner process */
        return -1;
    }

    switch ( msg->type ) {
    case MSG_BYTE:
        break;
    default:
        return -1;
    }

    return -1;
}

/*
 * read
 */
ssize_t
devfs_read(fildes_t *fildes, void *buf, size_t nbyte)
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

    spec = (struct devfs_fildes *)&fildes->fsdata;
    switch ( spec->entry->device.type ) {
    case DRIVER_DEVICE_CHAR:
        /* Character device */
        while ( 0 == _chr_ibuf_length(&spec->entry->device) ) {
            /* Empty buffer, then add this task to the blocking task list for
               this file descriptor and switch to another task. */
            t->state = TASK_BLOCKED;

            /* Set this task to the blocking task list of the file descriptor */
            tle = kmem_slab_alloc(SLAB_TASK_LIST);
            if ( NULL == tle ) {
                return -1;
            }
            tle->task = t;
            tle->next = fildes->head;
            fildes->head = tle;

            /* Switch to another task */
            task_switch();

            /* Will resume from this point */
        }

        len = 0;
        while ( len < (ssize_t)nbyte ) {
            if ( (c = _chr_ibuf_getc(&spec->entry->device)) < 0 ) {
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
devfs_write(fildes_t *fildes, const void *buf, size_t nbyte)
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
