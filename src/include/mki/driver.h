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

#define SYSDRIVER_MSG           1

#define SYSDRIVER_MMAP          11
#define SYSDRIVER_MUNMAP        12
#define SYSDRIVER_REG_DEV       21

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
 * Data structure for the character I/O
 */
typedef enum {
    SYSDRIVER_MSG_PUTC,
    SYSDRIVER_MSG_GETC,
    SYSDRIVER_MSG_READ,
    SYSDRIVER_MSG_WRITE,
} sysdriver_msg_type_t;
typedef struct {
    char *buf;
    size_t nbytes;
} sysdriver_msg_buf_t;
typedef struct {
    sysdriver_msg_type_t type;
    int dev;
    union {
        char c;
        sysdriver_msg_buf_t buf;
    } u;
} sysdriver_msg_t;

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
 * Data structure for the device management
 */
typedef struct {
    /* Arguments */
    const char *name;
    driver_device_type_t type;
} sysdriver_devfs_t;

/* Defined in the user library */
int driver_mmap(sysdriver_mmio_t *);
int driver_in8(int);
int driver_in16(int);
int driver_in32(int);
void driver_out8(int, int);
void driver_out16(int, int);
void driver_out32(int, int);

int driver_putc(int, int);
int driver_write(int, char *, size_t);
int driver_getc(int);

int driver_register_device(const char *, driver_device_type_t);

#endif /* _MKI_DRIVER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
