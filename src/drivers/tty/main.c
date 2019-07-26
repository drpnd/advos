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

unsigned long long syscall(int, ...);

#define VIDEO_PORT      0x3d4

/*
 * Entry point for the tty program
 */
int
main(int argc, char *argv[])
{
    unsigned long long cnt = 0;

    sysdriver_io_t io;
    sysdriver_mmio_t mmio;
    int pos;
    int ret;

    pos = 80 * 20;
    mmio.addr = (void *)0xb8000;
    mmio.size = 4096;
    ret = syscall(SYS_driver, SYSDRIVER_MMAP, &mmio);
    if ( 0 == ret ) {
        uint16_t *video;
        video = mmio.addr;
        *(video + pos) = (0x07 << 8) | 'a';
    }
    pos++;
    io.port = VIDEO_PORT;
    io.data = ((pos & 0xff) << 8) | 0x0f;
    syscall(SYS_driver, SYSDRIVER_OUT16, &io);
    __sync_synchronize();
    io.port = VIDEO_PORT;
    io.data = (((pos >> 8) & 0xff) << 8) | 0x0e;
    syscall(SYS_driver, SYSDRIVER_OUT16, &io);

    for ( ;; ) {
        syscall(766, 21, cnt);
        cnt++;
    }

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
