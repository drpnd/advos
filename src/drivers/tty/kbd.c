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
#include "tty.h"

#define KBD_OK          0
#define KBD_ERROR       -1
#define KBD_MAX_RETRY   0x01000000

#define KBD_CONTROLLER  0x0064
#define KBD_CTRL_STATUS KBD_CONTROLLER
#define KBD_CTRL_COMMAND KBD_CONTROLLER

/* Keyboard encoder */
#define KBD_ENC            0x0060
#define KBD_ENC_BUF        KBD_ENC
#define KBD_ENC_CMD        KBD_ENC

/* Controller */
#define KBD_CTRL           0x0064
#define KBD_CTRL_STAT      KBD_CTRL
#define KBD_CTRL_CMD       KBD_CTRL

#define KBD_STAT_OBUF 0x01
#define KBD_STAT_IBUF 0x02

#define KBD_ENC_CMD_SETLED      0xed
#define KBD_ENC_CMD_ENABLE      0xF4
#define KBD_ENC_CMD_DISABLE     0xF5

/* LED status */
#define KBD_LED_NONE            0x00000000
#define KBD_LED_SCROLLLOCK      0x00000001
#define KBD_LED_NUMLOCK         0x00000002
#define KBD_LED_CAPSLOCK        0x00000004


/* Commands */
#define KBD_CTRL_CMD_DISABLE    0xad
#define KBD_CTRL_CMD_ENABLE     0xae
#define KBD_CTRL_CMD_SELFTEST   0xaa

/* Status of self test */
#define KBD_CTRL_STAT_SELFTEST_OK   0x55
#define KBD_CTRL_STAT_SELFTEST_NG   0xfc

/* Special scan codes */
#define KBD_KEY_CTRL_LEFT       0x1d
#define KBD_KEY_SHIFT_LEFT      0x2a
#define KBD_KEY_SHIFT_RIGHT     0x36
#define KBD_KEY_CAPS_LOCK       0x3a
#define KBD_KEY_CTRL_RIGHT      0x5a
#define KBD_KEY_UP              0x48
#define KBD_KEY_LEFT            0x4b
#define KBD_KEY_RIGHT           0x4d
#define KBD_KEY_DOWN            0x50
#define KBD_KEY_F1              0x3b
#define KBD_KEY_F2              0x3c
#define KBD_KEY_F3              0x3d
#define KBD_KEY_F4              0x3e
#define KBD_KEY_F5              0x3f
#define KBD_KEY_F6              0x40
#define KBD_KEY_F7              0x41
#define KBD_KEY_F8              0x42
#define KBD_KEY_F9              0x43
#define KBD_KEY_F10             0x44
#define KBD_KEY_F11             0x57
#define KBD_KEY_F12             0x58

/* Ascii code for special scan codes */
#define KBD_ASCII_UP            0x86
#define KBD_ASCII_LEFT          0x83
#define KBD_ASCII_RIGHT         0x84
#define KBD_ASCII_DOWN          0x85

/* Default keymap */
static unsigned char keymap_base[] =
    "  1234567890-=\x08\t"      /* 0x00-0x0f */
    "qwertyuiop[]\r as"         /* 0x10-0x1f */
    "dfghjkl;'` \\zxcv"         /* 0x20-0x2f */
    "bnm,./          "          /* 0x30-0x3f */
    "                "          /* 0x40-0x4f */
    "                "          /* 0x50-0x5f */
    "                "          /* 0x60-0x6f */
    "                ";         /* 0x70-0x7f */
static unsigned char keymap_shift[] =
    "  !@#$%^&*()_+\x08\t"      /* 0x00-0x0f */
    "QWERTYUIOP{}\r AS"         /* 0x10-0x1f */
    "DFGHJKL:\"~ |ZXCV"         /* 0x20-0x2f */
    "BNM<>?          "          /* 0x30-0x3f */
    "                "          /* 0x40-0x4f */
    "                "          /* 0x50-0x5f */
    "                "          /* 0x60-0x6f */
    "                ";         /* 0x70-0x7f */

/*
 * Read control status
 */
static unsigned char
_read_ctrl_status(void)
{
    return driver_in8(KBD_CTRL_STAT);
}

/*
 * Write a cntrol command
 */
static int
_write_ctrl_cmd(unsigned char cmd)
{
    int retry;

    /* Retry until it succeeds or exceeds retry max */
    for ( retry = 0; retry < KBD_MAX_RETRY; retry++ ) {
        if ( 0 == (_read_ctrl_status() & KBD_STAT_IBUF) ) {
            driver_out8(KBD_CTRL_CMD, cmd);
            return KBD_OK;
        }
    }

    return KBD_ERROR;
}

/*
 * Read from the keyboard encoder
 */
static unsigned char
_enc_read_buf(void)
{
    return driver_in8(KBD_ENC_BUF);
}

/*
 * Write command to the keyboard encoder
 */
static int
_enc_write_cmd(unsigned char cmd)
{
    int retry;

    for ( retry = 0; retry < KBD_MAX_RETRY; retry++ ) {
        if ( 0 == (_read_ctrl_status() & KBD_STAT_IBUF) ) {
            driver_out8(KBD_ENC_CMD, cmd);
            return KBD_OK;
        }
    }

    return KBD_ERROR;
}

/*
 * Wait until the output buffer becomes full
 */
static int
_wait_until_outbuf_full(void)
{
    unsigned char stat;
    int retry;

    for ( retry = 0; retry < KBD_MAX_RETRY; retry++ ) {
        stat = _read_ctrl_status();

        if ( KBD_STAT_OBUF == (stat & KBD_STAT_OBUF) ) {
            return KBD_OK;
        }
    }

    return KBD_ERROR;
}

/*
 * Parse the scan code and convert it to an ascii code
 */
static int
_parse_scan_code(kbd_t *kbd, int scan_code)
{
    int ascii;

    ascii = -1;
    if ( scan_code & 0x80 ) {
        /* Release */
        switch ( scan_code & 0x7f ) {
        case KBD_KEY_CTRL_LEFT:
            kbd->state.lctrl = 0;
            break;
        case KBD_KEY_CTRL_RIGHT:
            kbd->state.rctrl = 0;
            break;
        case KBD_KEY_SHIFT_LEFT:
            kbd->state.lshift = 0;
            break;
        case KBD_KEY_SHIFT_RIGHT:
            kbd->state.rshift = 0;
            break;
        case KBD_KEY_CAPS_LOCK:
            kbd->state.capslock = 0;
            break;
        default:
            ;
        }
    } else {
        /* Pressed */
        switch ( scan_code ) {
        case KBD_KEY_CTRL_LEFT:
            kbd->state.lctrl = 1;
            break;
        case KBD_KEY_CTRL_RIGHT:
            kbd->state.rctrl = 1;
            break;
        case KBD_KEY_SHIFT_LEFT:
            kbd->state.lshift = 1;
            break;
        case KBD_KEY_SHIFT_RIGHT:
            kbd->state.rshift = 1;
            break;
        case KBD_KEY_CAPS_LOCK:
            kbd->state.capslock = 1;
            break;
        case KBD_KEY_UP:
            ascii = KBD_ASCII_UP;
            break;
        case KBD_KEY_LEFT:
            ascii = KBD_ASCII_LEFT;
            break;
        case KBD_KEY_RIGHT:
            ascii = KBD_ASCII_RIGHT;
            break;
        case KBD_KEY_DOWN:
            ascii = KBD_ASCII_DOWN;
            break;
        default:
            if ( (kbd->state.lshift || kbd->state.rshift) ) {
                /* w/ shift */
                ascii = keymap_shift[scan_code];
            } else {
                /* w/o shift */
                ascii = keymap_base[scan_code];
            }
        }
    }

    return ascii;
}

/*
 * Initialize the keyboard
 */
int
kbd_init(kbd_t *kbd)
{
    int ret;

    kbd->disabled = 0;

    /* Initialize the key states */
    kbd->state.lctrl = 0;
    kbd->state.rctrl = 0;
    kbd->state.lshift = 0;
    kbd->state.rshift = 0;
    kbd->state.capslock = 0;
    kbd->state.numlock = 0;
    kbd->state.scrplllock = 0;
    kbd->state.insert = 0;

    /* Set LED according to the state */
    ret = kbd_set_led(kbd);
    if ( KBD_OK != ret ) {
        return -1;
    }

    return 0;
}

/*
 * Set LED by the key state
 */
int
kbd_set_led(kbd_t *kbd)
{
    int ret;
    int led;

    /* LED status from the key stateus */
    led = KBD_LED_NONE;
    if ( kbd->state.scrplllock ) {
        led |= KBD_LED_SCROLLLOCK;
    }
    if ( kbd->state.numlock ) {
        led |= KBD_LED_NUMLOCK;
    }
    if ( kbd->state.capslock ) {
        led |= KBD_LED_CAPSLOCK;
    }

    ret = _enc_write_cmd(KBD_ENC_CMD_SETLED);
    ret |= _enc_write_cmd((unsigned char)led);

    return ret;
}

/*
 * Perform self-test
 */
int
kbd_selftest(void)
{
    unsigned char encbuf;
    int stat;

    stat = _write_ctrl_cmd(KBD_CTRL_CMD_SELFTEST);
    if ( KBD_OK != stat ) {
        return stat;
    }

    /* Wait until output buffer becomes full */
    stat = _wait_until_outbuf_full();
    if ( KBD_OK != stat ) {
        return stat;
    }

    /* Check the self-test result */
    encbuf = _enc_read_buf();
    if ( KBD_CTRL_STAT_SELFTEST_OK == encbuf ) {
        /* KBD_OK */
        return stat;
    }

    return KBD_ERROR;
}

/*
 * Get a character from the keyboard
 */
int
kbd_getchar(kbd_t *kbd)
{
    int ret;
    int ascii;
    unsigned char scan_code;

    /* Read the status of the keyboard */
    ret = driver_in8(KBD_CTRL_STAT);
    if ( !(ret & 1) ) {
        /* Not ready */
        return -1;
    }

    /* Read a scan code from the buffer of the keyboard controller */
    scan_code = _enc_read_buf();
    /* Convert the scan code to an ascii code */
    ascii = _parse_scan_code(kbd, scan_code);

    if ( ascii >= 0 ) {
        /* Valid ascii code, then enqueue it to the buffer of the
           character device. */
        if ( kbd->state.lctrl || kbd->state.rctrl ) {
            switch ( ascii ) {
            case 'h':
            case 'H':
                /* Backspace */
                ascii = '\x8';
                break;
            case 'b':
            case 'B':
                /* Backword */
                ascii = KBD_ASCII_LEFT;
                break;
            case 'f':
            case 'F':
                /* Forward */
                ascii = KBD_ASCII_RIGHT;
                break;
            default:
                ;
            }
        }

        if ( '\r' == ascii ) {
            ascii = '\n';
        }
    }

    return ascii;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
