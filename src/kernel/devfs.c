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
#include "vfs.h"
#include "proc.h"
#include "msg.h"
#include <mki/driver.h>

#define DEVFS_TYPE          "devfs"
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
    /* Lock */
    int lock;
};

/*
 * devfs
 */
struct devfs {
    struct devfs_entry *head;
    struct devfs_entry *entries[DEVFS_MAXDEVS];
    int lock;
};

/*
 * inode
 */
struct devfs_inode {
    struct devfs_entry *e;
};

struct devfs devfs;

/* Prototype declarations */
vfs_mount_spec_t * devfs_mount(vfs_module_spec_t *, int, void *);
vfs_vnode_t * devfs_lookup(vfs_mount_spec_t *, vfs_vnode_t *, const char *);
ssize_t devfs_read(fildes_t *, void *, size_t);
ssize_t devfs_write(fildes_t *, const void *, size_t);

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
 * Put one character to the output buffer
 */
static __inline__ int
_chr_obuf_putc(struct devfs_device *dev, int c)
{
    off_t cur;
    off_t next;

    __sync_synchronize();

    cur = dev->dev.chr.obuf.tail;
    next = cur + 1 < SYSDRIVER_DEV_BUFSIZE ? cur + 1 : 0;

    if  ( dev->dev.chr.obuf.head == next ) {
        /* Buffer is full */
        return -1;
    }

    dev->dev.chr.obuf.buf[cur] = c;
    dev->dev.chr.obuf.tail = next;

    __sync_synchronize();

    return c;
}

/*
 * Get one character from the output buffer
 */
static __inline__ int
_chr_obuf_getc(struct devfs_device *dev)
{
    int c;
    off_t cur;
    off_t next;

    __sync_synchronize();

    if  ( dev->dev.chr.obuf.head == dev->dev.chr.obuf.tail ) {
        /* Buffer is empty */
        return -1;
    }
    cur = dev->dev.chr.obuf.head;
    next = cur + 1 < SYSDRIVER_DEV_BUFSIZE ? cur + 1 : 0;

    c = dev->dev.chr.obuf.buf[cur];
    dev->dev.chr.obuf.head = next;

    __sync_synchronize();

    return c;
}

/*
 * Get the queued length for the output buffer of a character device
 */
static __inline__ int
_chr_obuf_length(struct devfs_device *dev)
{
    __sync_synchronize();

    if ( dev->dev.chr.obuf.tail >= dev->dev.chr.obuf.head ) {
        return dev->dev.chr.obuf.tail - dev->dev.chr.obuf.head;
    } else {
        return SYSDRIVER_DEV_BUFSIZE + dev->dev.chr.obuf.tail
            - dev->dev.chr.obuf.head;
    }
}
static __inline__ int
_chr_obuf_available(struct devfs_device *dev)
{
    if ( _chr_obuf_length(dev) >= SYSDRIVER_DEV_BUFSIZE - 1 ) {
        return 0;
    } else {
        return 1;
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
    vfs_interfaces_t ifs;

    /* Clear the lock */
    devfs.lock = 0;

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

    /* Register devfs to the virtual filesystem management */
    kmemset(&ifs, 0, sizeof(vfs_interfaces_t));
    ifs.mount = devfs_mount;
    ifs.lookup = devfs_lookup;
    ret = vfs_register("devfs", &ifs, NULL);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Mount devfs
 */
vfs_mount_spec_t *
devfs_mount(vfs_module_spec_t *spec, int flags, void *data)
{
    return (vfs_mount_spec_t *)&devfs;
}

/*
 * Lookup
 */
vfs_vnode_t *
devfs_lookup(vfs_mount_spec_t *spec, vfs_vnode_t *parent, const char *name)
{
    struct devfs *fs;
    int i;
    struct devfs_entry *e;
    vfs_vnode_t *vnode;
    struct devfs_inode *in;

    fs = (struct devfs *)spec;

    /* Lock */
    spin_lock(&fs->lock);

    for ( i = 0; i < DEVFS_MAXDEVS; i++ ) {
        e = devfs.entries[i];
        if ( NULL == e ) {
            continue;
        }
        if ( 0 == kstrcmp(name, e->name) ) {
            /* Found, then create an inode data structure */
            vnode = vfs_vnode_alloc();
            if ( NULL == vnode ) {
                spin_unlock(&fs->lock);
                return NULL;
            }
            in = (struct devfs_inode *)&vnode->inode;
            in->e = e;
            spin_unlock(&fs->lock);
            return vnode;
        }
    }

    spin_unlock(&fs->lock);
    return NULL;
}

/*
 * Add an entry
 */
int
devfs_register(const char *name, int type, proc_t *proc)
{
    struct devfs_entry *e;
    int i;
    struct devfs *fs;

    /* Check the device type */
    if ( DEVFS_CHAR != type && DEVFS_BLOCK != type ) {
        return -1;
    }

    fs = (struct devfs *)&devfs;
    spin_lock(&fs->lock);

    for ( i = 0; i < DEVFS_MAXDEVS; i++ ) {
        if ( NULL == devfs.entries[i] ) {
            /* Available */
            break;
        }
    }
    if ( i >= DEVFS_MAXDEVS ) {
        /* No available entry */
        spin_unlock(&fs->lock);
        return -1;
    }

    /* Allocate an entry */
    e = kmem_slab_alloc(SLAB_DEVFS_ENTRY);
    if ( NULL == e ) {
        spin_unlock(&fs->lock);
        return -1;
    }
    kstrlcpy(e->name, name, PATH_MAX);
    e->device.type = type;
    e->flags = 0;
    e->proc = proc;
    e->lock = 0;
    devfs.entries[i] = e;

    spin_unlock(&fs->lock);

    return i;
}

/*
 * Remove an entry
 */
int
devfs_unregister(int index, proc_t *proc)
{
    struct devfs_entry *e;
    struct devfs *fs;

    fs = (struct devfs *)&devfs;

    /* Range check */
    if ( index >= DEVFS_MAXDEVS ) {
        return -1;
    }

    spin_lock(&fs->lock);
    e = devfs.entries[index];
    if ( NULL == e ) {
        spin_unlock(&fs->lock);
        return -1;
    }
    if ( proc != e->proc ) {
        spin_unlock(&fs->lock);
        return -1;
    }

    kmem_slab_free(SLAB_DEVFS_ENTRY, e);
    devfs.entries[index] = NULL;

    spin_unlock(&fs->lock);

    return 0;
}

/*
 * putc
 */
int
devfs_driver_putc(int index, proc_t *proc, char c)
{
    struct devfs_entry *e;
    int ret;

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

    spin_lock(&e->lock);
    ret = _chr_ibuf_putc(&e->device, c);
    if ( ret < 0 ) {
        spin_unlock(&e->lock);
        return -1;
    }

    return 0;
}

/*
 * write
 */
ssize_t
devfs_driver_write(int index, proc_t *proc, char *buf, size_t n)
{
    struct devfs_entry *e;
    int ret;
    ssize_t i;

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

    spin_lock(&e->lock);
    for ( i = 0; i < (ssize_t)n; i++ ) {
        ret = _chr_ibuf_putc(&e->device, buf[i]);
        if ( ret < 0 ) {
            spin_unlock(&e->lock);
            return i;
        }
    }

    spin_unlock(&e->lock);

    return i;
}

/*
 * getc
 */
int
devfs_driver_getc(int index, proc_t *proc)
{
    struct devfs_entry *e;
    int ret;

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

    spin_lock(&e->lock);
    ret = _chr_obuf_getc(&e->device);
    spin_unlock(&e->lock);
    return ret;
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
    case DEVFS_CHAR:
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
    case DEVFS_BLOCK:
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
    struct devfs_fildes *spec;
    task_t *t;
    task_t *tmp;
    ssize_t len;
    int c;

    /* Get the currently running task */
    t = this_task();
    if ( NULL == t ) {
        return -1;
    }

    spec = (struct devfs_fildes *)&fildes->fsdata;
    switch ( spec->entry->device.type ) {
    case DEVFS_CHAR:
        /* Character device */
        if ( !_chr_obuf_available(&spec->entry->device) ) {
            /* Buffer is full.  FIXME: Implement blocking here */
            return 0;
        }
        len = 0;
        while ( len < (ssize_t)nbyte ) {
            c = *(const char *)(buf + len);
            if ( _chr_obuf_putc(&spec->entry->device, c) < 0 ) {
                /* Buffer becomes full, then exit from the loop. */
                break;
            }
            len++;
        }
        /* Wake up the driver process */
        tmp = spec->entry->proc->task;
        tmp->state = TASK_READY;
        return len;
    case DEVFS_BLOCK:
        break;
    default:
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
