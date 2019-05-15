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

#include "arch.h"
#include "../../kernel.h"
#include "../../console.h"
#include "../../memory.h"
#include <stdint.h>

#define VIDEO_PORT      0x3d4

/*
 * Kernel console information
 */
typedef struct {
    uint16_t *video;
    int pos;
    int column;
    int column_save;
} arch_video_console_t;


/*
 * Initialize the console
 */
console_dev_t *
vconsole_init(void)
{
    console_dev_t *dev;
    arch_video_console_t *vcon;

    /* Allocate for the video console data structure */
    dev = kmalloc(sizeof(console_dev_t));
    if ( NULL == dev ) {
        return NULL;;
    }
    vcon = kmalloc(sizeof(arch_video_console_t));
    if ( NULL == vcon ) {
        kfree(dev);
        return NULL;
    }

    /* Video RAM */
    vcon->video = (uint16_t *)VIDEO_RAM_80X25;
    vcon->pos = 0;
    vcon->column = 0;
    vcon->column_save = 0;

    /* Setup the console device */
    dev->write = vconsole_write;
    dev->next = NULL;
    dev->spec = vcon;

    return dev;
}

/*
 * Update the position of the cursor
 */
static void
_update_cursor(int pos)
{
    /* Low */
    outw(VIDEO_PORT, ((pos & 0xff) << 8) | 0x0f);
    /* High */
    outw(VIDEO_PORT, (((pos >> 8) & 0xff) << 8) | 0x0e);
}

/*
 * console_write
 */
int
vconsole_write(console_dev_t *dev, const void *buf, size_t nbyte)
{
    ssize_t i;
    int c;
    arch_video_console_t *vcon;
    int next;

    vcon = dev->spec;
    for ( i = 0; i < (ssize_t)nbyte; i++ ) {
        c = ((const char *)buf)[i];
        switch ( c ) {
        case '\r':
            /* CR */
            vcon->pos -= vcon->column;
            vcon->column = 0;
            break;
        case '\n':
            /* LF */
            next = ((vcon->pos + vcon->column_save + 79) / 80) * 80;
            if ( next >= 80 * 25 ) {
                kmemmove(vcon->video, vcon->video + 80, 80 * 24);
                kmemset(vcon->video + 80 * 25, 0, 80);
                vcon->pos = 80 * 24;
            } else {
                vcon->pos = next;
            }
            vcon->column_save = 0;
            break;
        default:
            if ( c >= 0x20 && c <= 0x7e ) {
                /* Printable characters */
                *(volatile uint16_t *)(vcon->video + vcon->pos) = 0x0700 | c;
                vcon->pos++;
                vcon->column++;
                vcon->column_save = vcon->column;
                if ( vcon->pos >= 80 * 25 ) {
                    kmemmove(vcon->video, vcon->video + 80, 80 * 24);
                    vcon->pos -= 80;
                }
            } else if ( c == '\t' ) {
                /* Tab */
                do {
                    *(volatile uint16_t *)(vcon->video + vcon->pos) = 0x0720;
                    vcon->pos++;
                    vcon->column++;
                    vcon->column_save = vcon->column;
                    if ( vcon->pos >= 80 * 25 ) {
                        kmemmove(vcon->video, vcon->video + 80, 80 * 24);
                        vcon->pos -= 80;
                    }
                } while ( (vcon->column % 4) != 0 );
            }
            break;
        }
    }

    /* Update the cursor */
    _update_cursor(vcon->pos);

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
