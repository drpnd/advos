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

#include <sys/syscall.h>
#include <mki/driver.h>
#include "kernel.h"
#include "proc.h"
#include "kvar.h"
#include "devfs.h"

/*
 * mmap
 */
static int
_mmap(task_t *t, sysdriver_mmio_t *mmio)
{
    proc_t *proc;
    virt_memory_t *vmem;
    size_t npg;
    void *ptr;

    /* Process */
    proc = t->proc;
    if ( NULL == proc ) {
        /* Invalid process */
        return -1;
    }

    /* Virtual memory */
    vmem = proc->vmem;
    if ( NULL == vmem ) {
        /* Invalid virtual memory */
        return -1;
    }

    if ( (uintptr_t)mmio->addr & (MEMORY_PAGESIZE - 1) ) {
        /* Address is not page aligned */
        return -1;
    }
    if ( mmio->size & (MEMORY_PAGESIZE - 1) ) {
        /* Size is not page aligned */
        return -1;
    }
    npg = mmio->size / MEMORY_PAGESIZE;

    ptr = virt_memory_wire2(vmem, (uintptr_t)mmio->addr, npg);
    if ( NULL == ptr ) {
        return -1;
    }
    mmio->addr = ptr;

    return 0;
}

/*
 * munmap
 */
static int
_munmap(task_t *t, void *args)
{
    return -1;
}

/*
 * I/O
 */
static int
_io(int nr, sysdriver_io_t *io)
{
    switch ( nr ) {
    case SYSDRIVER_IN8:
        io->data = in8(io->port);
        break;
    case SYSDRIVER_IN16:
        io->data = in16(io->port);
        break;
    case SYSDRIVER_IN32:
        io->data = in32(io->port);
        break;
    case SYSDRIVER_OUT8:
        out8(io->port, io->data);
        break;
    case SYSDRIVER_OUT16:
        out16(io->port, io->data);
        break;
    case SYSDRIVER_OUT32:
        out32(io->port, io->data);
        break;
    default:
        return -1;
    }

    return 0;
}

/*
 * Register driver device
 */
static int
_register_device(task_t *t, sysdriver_devfs_t *msg)
{
    proc_t *proc;
    virt_memory_t *vmem;
    driver_device_t *dev;
    size_t sz;
    int ret;

    /* Get the process */
    proc = t->proc;
    if ( NULL == proc ) {
        return -1;
    }

    /* Virtual memory */
    vmem = proc->vmem;
    if ( NULL == vmem ) {
        /* Invalid virtual memory */
        return -1;
    }

    /* Calculate the size to be allocated */
    sz = sizeof(driver_device_t);
    sz = (sz + MEMORY_PAGESIZE - 1) / MEMORY_PAGESIZE;
    dev = virt_memory_alloc_pages(vmem, sz, MEMORY_ZONE_NUMA_AWARE, 0);
    if ( NULL == dev ) {
        return -1;
    }

    /* Register */
    ret = devfs_register(msg->name, msg->flags, proc, dev);
    if ( ret < 0 ) {
        virt_memory_free_pages(vmem, dev);
        return -1;
    }

    /* Set the return value */
    msg->device = dev;

    return 0;
}

/*
 * Driver-related system call
 */
int
sys_driver(int nr, void *args)
{
    task_t *t;

    /* Get the current task */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }

    switch ( nr ) {
    case SYSDRIVER_MMAP:
        return _mmap(t, args);
    case SYSDRIVER_MUNMAP:
        return _munmap(t, args);
    case SYSDRIVER_REG_DEV:
        return _register_device(t, args);
    case SYSDRIVER_IN8:
    case SYSDRIVER_IN16:
    case SYSDRIVER_IN32:
    case SYSDRIVER_OUT8:
    case SYSDRIVER_OUT16:
    case SYSDRIVER_OUT32:
        return _io(nr, args);
    default:
        return -1;
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
