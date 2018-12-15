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

#include "arch.h"
#include <stdint.h>

/*
 * invariant_tsc_freq -- resolve the base frequency of invariant TSC
 */
uint64_t
invariant_tsc_freq(void)
{
    uint64_t reg;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t family;
    uint64_t model;

    /* Check Invariant TSC support */
    cpuid(0x80000007, &rbx, &rcx, &rdx);
    if ( !(rdx & 0x100) ) {
        return 0;
    }

    /* Read TSC frequency */
    reg = (rdmsr(MSR_PLATFORM_INFO) & 0xff00) >> 8;

    /* CPUID;EAX=0x01 */
    rax = cpuid(0x01, &rbx, &rcx, &rdx);
    family = ((rax & 0xf00) >> 8) | ((rax & 0xff00000) >> 12);
    model = ((rax & 0xf0) >> 4) | ((rax & 0xf0000) >> 12);
    if ( 0x06 == family ) {
        switch ( model ) {
        case 0x1e: /* Nehalem */
        case 0x1a: /* Nehalem */
        case 0x2e: /* Nehalem */
            /* 133.33 MHz */
            return reg * 133330000;
        case 0x2a: /* SandyBridge */
        case 0x2d: /* SandyBridge-E */
        case 0x3a: /* IvyBridge */
        case 0x3c: /* Haswell */
        case 0x3d: /* Broadwell */
        case 0x46: /* Skylake */
        case 0x4e: /* Skylake */
        case 0x57: /* Xeon Phi */
            /* 100.00 MHz */
            return reg * 100000000;
        default:
            /* Presumedly 100.00 MHz for other processors */
            return reg * 100000000;
        }
    }

    return 0;
}

/*
 * bsp_init -- initialize BSP
 */
void
bsp_init(void)
{

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
