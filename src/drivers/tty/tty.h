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

#ifndef _TTY_H
#define _TTY_H

#include <stdint.h>
#include <sys/types.h>
#include <termios.h>

#define TTY_LINEBUFSIZE 4096

/*
 * Line buffer
 */
typedef struct {
    /* Cursor */
    off_t cur;
    /* Length */
    size_t len;
    /* Buffer */
    char buf[TTY_LINEBUFSIZE];
} tty_line_buffer_t;

/*
 * TTY
 */
typedef struct {
    /* Line buffer */
    tty_line_buffer_t lnbuf;
    /* Termios */
    struct termios term;
} tty_t;

/*
 * Keyboard state
 */
typedef struct {
    /* Left control */
    int lctrl:1;
    /* Right control */
    int rctrl:1;
    /* Left shift */
    int lshift:1;
    /* Right shift */
    int rshift:1;
    /* Capslock */
    int capslock:1;
    /* Numlock */
    int numlock:1;
    /* Scroll lock */
    int scrplllock:1;
    /* Insert */
    int insert:1;
} kbd_state_t;

/*
 * Keyboard
 */
typedef struct {
    int disabled;
    kbd_state_t state;
} kbd_t;

/*
 * Video
 */
typedef struct {
    uint16_t *vram;
    int pos;
} video_t;

/*
 * Text screen
 */
typedef struct {
    /* Buffer size */
    size_t size;
    /* Width */
    size_t width;
    /* Height */
    size_t height;
    /* Cursor position */
    off_t cur;
    /* End of buffer */
    off_t eob;
    /* Line buffer marker */
    off_t lmark;
} screen_t;

/*
 * Console
 */
typedef struct {
    /* Keyboard */
    kbd_t kbd;
    /* Video */
    video_t video;
    /* Screen */
    screen_t screen;
} console_t;

/* in kbd.c */
int kbd_init(kbd_t *);
int kbd_set_led(kbd_t *);
int kbd_getchar(kbd_t *);

/* in console.c */
int console_init(console_t *, const char *);
int console_proc(console_t *, tty_t *);

/* in tty.c */
int tty_line_buffer_init(tty_line_buffer_t *);
int tty_line_buffer_putc(tty_line_buffer_t *, int);

#endif /* _TTY_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
