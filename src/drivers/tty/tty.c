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

#include <string.h>
#include <termios.h>
#include "tty.h"

#define ASCII_UP            0x86
#define ASCII_LEFT          0x83
#define ASCII_RIGHT         0x84
#define ASCII_DOWN          0x85

/*
 * Initialize line buffer
 */
int
tty_line_buffer_init(tty_line_buffer_t *buf)
{
    buf->cur = 0;
    buf->len = 0;

    return 0;
}

/*
 * Insert a character to the buffer
 */
static int
_insert_char(tty_line_buffer_t *buf, int c)
{
    if ( buf->len >= TTY_LINEBUFSIZE ) {
        /* Buffer is full. */
        return -1;
    }
    if ( buf->len < (size_t)buf->cur ) {
        /* Invalid parameter */
        return -1;
    }

    if ( (size_t)buf->cur != buf->len ) {
        memmove(buf->buf + buf->cur + 1, buf->buf + buf->cur,
                buf->len - buf->cur);
        buf->buf[buf->cur] = c;
        buf->cur++;
        buf->len++;
    } else {
        buf->buf[buf->cur] = c;
        buf->cur++;
        buf->len++;
    }

    return 0;
}

/*
 * Backspace
 */
static int
_backspace(tty_line_buffer_t *buf)
{
    if ( buf->len < (size_t)buf->cur ) {
        /* Invalid parameter */
        return -1;
    }
    if ( 0 == buf->cur ) {
        /* No character to delete */
        return 0;
    }

    if ( (size_t)buf->cur != buf->len ) {
        memmove(buf->buf + buf->cur - 1, buf->buf + buf->cur,
                buf->len - buf->cur);
        buf->cur--;
        buf->len--;
    } else {
        buf->cur--;
        buf->len--;
    }

    return 0;
}

/*
 * Try to move backward
 */
static int
_move_left(tty_line_buffer_t *buf)
{
    if ( buf->len < (size_t)buf->cur ) {
        /* Invalid parameter */
        return -1;
    }
    if ( 0 == buf->cur ) {
        /* Already at the head */
        return 0;
    }

    buf->cur--;

    return 0;
}

/*
 * Try to move forward
 */
static int
_move_right(tty_line_buffer_t *buf)
{
    if ( buf->len < (size_t)buf->cur ) {
        /* Invalid parameter */
        return -1;
    }
    if ( buf->len == (size_t)buf->cur ) {
        /* Already at the head */
        return 0;
    }

    buf->cur++;

    return 0;
}

/*
 * Put a character
 */
int
tty_line_buffer_putc(tty_line_buffer_t *buf, int c)
{
    switch ( c ) {
    case '\x8':
        /* Backspace */
        _backspace(buf);
        break;
    case '\n':
        /* New line */
        buf->cur = 0;
        break;
    case ASCII_LEFT:
        /* Move backward */
        _move_left(buf);
        break;
    case ASCII_RIGHT:
        /* Move forward */
        _move_right(buf);
        break;
    default:
        /* Other character */
        _insert_char(buf, c);
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
