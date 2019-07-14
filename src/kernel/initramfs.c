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

#include "vfs.h"
#include "kernel.h"

/*
 * initrd
 */
struct initrd_entry {
    char name[16];
    uint64_t offset;
    uint64_t size;
};

struct initramfs_fildes {
    int inode;
};

#define INITRAMFS_BASE  0xc0030000

/*
 * open
 */
int
initramfs_open(const char *path, int oflag, ...)
{
    struct initrd_entry *e;
    int i;

    e = (void *)INITRAMFS_BASE;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(path, e->name) ) {
            /* Found */
            (void *)INITRAMFS_BASE + e->offset;
            e->size;
        }
        e++;
    }

    return -1;
}

/*
 * close
 */
int
initramfs_close(int fildes)
{
    return -1;
}

/*
 * fstat
 */
int
initramfs_fstat(int fildes, struct stat *buf)
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