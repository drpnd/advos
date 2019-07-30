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

#ifndef _TERMIOS_H
#define _TERMIOS_H

/* Baud rate definitions */
#define B0      0
#define B50     50
#define B75     75
#define B110    110
#define B134    134
#define B150    150
#define B200    200
#define B300    300
#define B600    600
#define B1200   1200
#define B1800   1800
#define B2400   2400
#define B4800   4800
#define B9600   9600
#define B19200  19200
#define B38400  38400

/* Special characters */
#define VEOF        0           /* ICANON */
#define VEOL        1           /* ICANON */
#define VERASE      3           /* ICANON */
#define VKILL       5           /* ICANON */
#define VINTR       8           /* ISIG */
#define VQUIT       9           /* ISIG */
#define VSUSP       10          /* ISIG */
#define VSTART      12          /* IXON, IXOFF */
#define VSTOP       13          /* IXON, IXOFF */
#define VMIN        16          /* !ICANON */
#define VTIME       17          /* !ICANON */
#define NCCS        20

/* Input flags */
#define IGNBRK      0x00000001  /* ignore BREAK condition */
#define BRKINT      0x00000002  /* map BREAK to SIGINTR */
#define IGNPAR      0x00000004  /* ignore (discard) parity errors */
#define PARMRK      0x00000008  /* mark parity and framing errors */
#define INPCK       0x00000010  /* enable checking of parity errors */
#define ISTRIP      0x00000020  /* strip 8th bit off chars */
#define INLCR       0x00000040  /* map NL into CR */
#define IGNCR       0x00000080  /* ignore CR */
#define ICRNL       0x00000100  /* map CR to NL (ala CRMOD) */
#define IXON        0x00000200  /* enable output flow control */
#define IXOFF       0x00000400  /* enable input flow control */

/* Output flags */
#define OPOST       0x00000001  /* enable following output processing */

/* Control flags */
#define CSIZE       0x00000300  /* character size mask */
#define     CS5         0x00000000  /* 5 bits (pseudo) */
#define     CS6         0x00000100  /* 6 bits */
#define     CS7         0x00000200  /* 7 bits */
#define     CS8         0x00000300  /* 8 bits */
#define CSTOPB      0x00000400  /* send 2 stop bits */
#define CREAD       0x00000800  /* enable receiver */
#define PARENB      0x00001000  /* parity enable */
#define PARODD      0x00002000  /* odd parity, else even */
#define HUPCL       0x00004000  /* hang up on last close */
#define CLOCAL      0x00008000  /* ignore modem status lines */

/* Local flags */
#define ECHOE   0x00000002      /* visually erase chars */
#define ECHOK   0x00000004      /* echo NL after line kill */
#define ECHO    0x00000008      /* enable echoing */
#define ECHONL  0x00000010      /* echo NL even if ECHO is off */

#define ISIG    0x00000080      /* enable signals INTR, QUIT, [D]SUSP */
#define ICANON  0x00000100      /* canonicalized input lines */
#define IEXTEN  0x00000400      /* enable DISCARD and LNEXT */
#define EXTPROC 0x00000800      /* external processing */
#define TOSTOP  0x00400000      /* stop background jobs from output */
#define NOFLSH  0x80000000      /* don't flush after interrupt */

#define TCSANOW     0
#define TCSADRAIN   1
#define TCSAFLUSH   2

typedef unsigned char   cc_t;
typedef unsigned long   tcflag_t;
typedef unsigned long   speed_t;
struct termios {
    tcflag_t c_iflag;           /* input flags */
    tcflag_t c_oflag;           /* output flags */
    tcflag_t c_cflag;           /* control flags */
    tcflag_t c_lflag;           /* local flags */
    cc_t c_cc[20];              /* control chars */
    speed_t ispeed;             /* input speed */
    speed_t ospeed;             /* output speed */
};

int tcgetattr(int, struct termios *);
int tcsetattr(int, int, const struct termios *);

#endif /* _TERMIOS_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
