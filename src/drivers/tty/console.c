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

#include <unistd.h>
#include <mki/driver.h>
#include <sys/syscall.h>
#include "tty.h"

#define VIDEO_RAM   0x000b8000
#define VIDEO_PORT  0x3d4

/*
 * console_write
 */
ssize_t
console_write(console_t *con, const void *buf, size_t count)
{
    ssize_t n;
    uint16_t val;

    for ( n = 0; n < (ssize_t)count; n++ ) {
        *(con->video.vram + con->video.pos + n)
            = 0x0700 | ((const char *)buf)[n];
    }

    /* Move the cursor (low -> high) */
    val = ((con->video.pos & 0xff) << 8) | 0x0f;
    driver_out16(VIDEO_PORT, val);
    val = (con->video.pos & 0xff00) | 0x0e;
    driver_out16(VIDEO_PORT, val);

    return 0;
}

/*
 * Initialize the console
 */
int
console_init(console_t *con, const char *ttyname)
{
    sysdriver_mmio_t mmio;
    int ret;

    /* Initialzie the keyboard */
    ret = kbd_init(&con->kbd);
    if ( ret < 0 ) {
        return -1;
    }

    /* Initialize the video driver */
    mmio.addr = (void *)VIDEO_RAM;
    mmio.size = 4096;
    ret = driver_mmap(&mmio);
    if ( ret < 0 ) {
        return -1;
    }
    con->video.vram = (uint16_t *)mmio.addr;
    con->video.pos = 0;

    /* Register */
    ret = driver_register_device("console", DRIVER_DEVICE_CHAR);
    if ( ret < 0 ) {
        /* Failed to register the device */
        return -1;
    }

    char *msg = "Registered.";
    console_write(con, msg, 11);

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
