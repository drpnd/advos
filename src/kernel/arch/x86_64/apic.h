/*_
 * Copyright (c) 2018 Hirochika Asai <asai@jar.jp>
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

#ifndef _ADVOS_KERNEL_APIC_H
#define _ADVOS_KERNEL_APIC_H

#include <stdint.h>
#include "const.h"

#define APIC_LAPIC_ID                   0x020
#define APIC_SIVR                       0x0f0
#define APIC_ICR_LOW                    0x300
#define APIC_ICR_HIGH                   0x310

/* ICR delivery mode */
#define APIC_ICR_FIXED                  0x00000000
#define APIC_ICR_INIT                   0x00000500
#define APIC_ICR_STARTUP                0x00000600
/* ICR status */
#define APIC_ICR_SEND_PENDING           0x00001000
/* ICR level */
#define APIC_ICR_LEVEL_ASSERT           0x00004000
/* ICR destination */
#define APIC_ICR_DEST_NOSHORTHAND       0x00000000
#define APIC_ICR_DEST_SELF              0x00040000
#define APIC_ICR_DEST_ALL_INC_SELF      0x00080000
#define APIC_ICR_DEST_ALL_EX_SELF       0x000c0000

void lapic_send_init_ipi(void);
void lapic_send_startup_ipi(uint8_t);
void lapic_bcast_fixed_ipi(uint8_t);
void lapic_send_fixed_ipi(int, uint8_t);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
