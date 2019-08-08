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

#ifndef _MKI_DRIVER_H
#define _MKI_DRIVER_H

#include <unistd.h>

#define SYSDRIVER_MMAP          11
#define SYSDRIVER_MUNMAP        12

#define SYSDRIVER_IN8           101
#define SYSDRIVER_IN16          102
#define SYSDRIVER_IN32          103
#define SYSDRIVER_OUT8          111
#define SYSDRIVER_OUT16         112
#define SYSDRIVER_OUT32         113

#define SYSDRIVER_DEV_BUFSIZE   8192

/*
 * Data structure for the I/O interface
 */
typedef struct {
    long long port;
    long long data;
} sysdriver_io_t;

/*
 * Data structure for the memory mapped I/O interface
 */
typedef struct {
    void *addr;
    size_t size;
} sysdriver_mmio_t;

/*
 * Ring buffer
 */
struct driver_device_fifo {
    uint8_t buf[SYSDRIVER_DEV_BUFSIZE];
    volatile off_t head;
    volatile off_t tail;
};

/*
 * Character device
 */
struct driver_mapped_device_chr {
    /* FIFO */
    struct driver_device_fifo ibuf;
    struct driver_device_fifo obuf;
};

/*
 * Device type
 */
typedef enum {
    DRIVER_DEVICE_CHAR,
    DRIVER_DEVICE_BLOCK,
} driver_device_type_t;

/*
 * Mapped device (also referred from struct devfs_entry)
 */
typedef struct {
    driver_device_type_t type;
    union {
        struct driver_mapped_device_chr chr;
    } dev;
} driver_device_t;

/*
 * Put one character to the input buffer
 */
static __inline__ int
driver_chr_ibuf_putc(driver_device_t *dev, int c)
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
driver_chr_ibuf_getc(driver_device_t *dev)
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
driver_chr_ibuf_length(driver_device_t *dev)
{
    __sync_synchronize();

    if ( dev->dev.chr.ibuf.tail >= dev->dev.chr.ibuf.head ) {
        return dev->dev.chr.ibuf.tail - dev->dev.chr.ibuf.head;
    } else {
        return SYSDRIVER_DEV_BUFSIZE + dev->dev.chr.ibuf.tail
            - dev->dev.chr.ibuf.head;
    }
}

/* Defined in the user library */
int driver_mmap(sysdriver_mmio_t *);
int driver_in8(int);
int driver_in16(int);
int driver_in32(int);
void driver_out8(int, int);
void driver_out16(int, int);
void driver_out32(int, int);

#endif /* _MKI_DRIVER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
