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

#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <mki/driver.h>
#include <sys/syscall.h>
#include "tty.h"

unsigned long long syscall(int, ...);

#define TTY_CONSOLE_PREFIX  "console"
#define TTY_SERIAL_PREFIX   "ttys"
#define VIDEO_PORT      0x3d4

/*
 * Entry point for the tty program
 */
int
main(int argc, char *argv[])
{
    unsigned long long cnt = 0;
    int ret;
    char *pash_args[] = {"/bin/pash", NULL};
    tty_t tty;

    sysdriver_io_t io;
    int pos;
    console_t con;

    /* Initialize the tty */
    tty.term.c_iflag = 0;
    tty.term.c_oflag = 0;
    tty.term.c_cflag = 0;
    tty.term.c_lflag = ECHO;
    tty.term.ispeed = 0;
    tty.term.ospeed = 0;
    ret = tty_line_buffer_init(&tty.lnbuf);
    if ( ret < 0 ) {
        return -1;
    }

    /* Initialize the console */
    ret = console_init(&con, "console");
    if ( ret < 0 ) {
        return -1;
    }

    pos = 80 * 20;
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
