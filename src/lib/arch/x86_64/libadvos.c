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
#include <sys/syscall.h>
#include <mki/driver.h>
#include <unistd.h>

unsigned long long syscall(int, ...);

/*
 * execve
 */
int
initexec(const char *path, char *const argv[], char *const envp[])
{
    return syscall(SYS_initexec, path, argv, envp);
}

/*
 * MMIO
 */
int
driver_mmap(sysdriver_mmio_t *mmio)
{
    return syscall(SYS_driver, SYSDRIVER_MMAP, mmio);
}

/*
 * I/O
 */
int
driver_in8(int port)
{
    sysdriver_io_t io;

    io.port = port;
    syscall(SYS_driver, SYSDRIVER_IN8, &io);
    return io.data;
}
int
driver_in16(int port)
{
    sysdriver_io_t io;

    io.port = port;
    syscall(SYS_driver, SYSDRIVER_IN16, &io);
    return io.data;
}
int
driver_in32(int port)
{
    sysdriver_io_t io;

    io.port = port;
    syscall(SYS_driver, SYSDRIVER_IN32, &io);
    return io.data;
}
void
driver_out8(int port, int data)
{
    sysdriver_io_t io;

    io.port = port;
    io.data = data;
    syscall(SYS_driver, SYSDRIVER_OUT8, &io);
}
void
driver_out16(int port, int data)
{
    sysdriver_io_t io;

    io.port = port;
    io.data = data;
    syscall(SYS_driver, SYSDRIVER_OUT16, &io);
}
void
driver_out32(int port, int data)
{
    sysdriver_io_t io;

    io.port = port;
    io.data = data;
    syscall(SYS_driver, SYSDRIVER_OUT32, &io);
}

/*
 * Driver device registration
 */
driver_device_t *
driver_register_device(const char *name, int type)
{
    sysdriver_devfs_t msg;

    msg.name = name;
    msg.type = type;
    syscall(SYS_driver, SYSDRIVER_REG_DEV, &msg);

    return msg.device;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
