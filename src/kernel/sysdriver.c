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

/*
 * mmap
 */
static int
_mmap(task_t *t, void *args)
{
    return -1;
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
    case SYSDRIVER_IN8:
    case SYSDRIVER_IN16:
    case SYSDRIVER_IN32:
    case SYSDRIVER_OUT8:
    case SYSDRIVER_OUT16:
    case SYSDRIVER_OUT32:
        return -1;
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
