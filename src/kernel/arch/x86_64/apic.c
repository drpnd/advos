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

#include "apic.h"
#include "arch.h"

/*
 * lapic_base_addr -- get the base address for local APIC access by reading
 * APIC machine specific register (MSR)
 */
uint64_t
lapic_base_addr(void)
{
    uint32_t reg;
    uint64_t msr;
    uint64_t apic_base;

    /* Read IA32_APIC_BASE register */
    msr = rdmsr(MSR_APIC_BASE);
    apic_base = msr & 0xfffffffffffff000ULL;

    /* Enable APIC at spurious interrupt vector register: default vector 0xff */
    reg = mfrd32(apic_base + APIC_SIVR);
    reg |= 0x100;       /* Bit 8: APIC Software Enable/Disable */
    mfwr32(apic_base + APIC_SIVR, reg);

    return apic_base;
}

/*
 * lapic_id -- get the local APIC ID of this CPU core
 */
int
lapic_id(void)
{
    uint32_t reg;
    uint64_t apic_base;

    apic_base = lapic_base_addr();
    reg = *(uint32_t *)(apic_base + APIC_LAPIC_ID);

    return reg >> 24;
}

/*
 * lapic_send_init_ipi -- send INIT IPI
 */
void
lapic_send_init_ipi(void)
{
    uint32_t icrl;
    uint32_t icrh;
    uint64_t apic_base;

    apic_base = lapic_base_addr();

    icrl = mfrd32(apic_base + APIC_ICR_LOW);
    icrh = mfrd32(apic_base + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | APIC_ICR_INIT | APIC_ICR_DEST_ALL_EX_SELF;
    icrh = (icrh & 0x000fffff);

    mfwr32(apic_base + APIC_ICR_HIGH, icrh);
    mfwr32(apic_base + APIC_ICR_LOW, icrl);
}

/*
 * lapic_send_startup_ipi -- send start up IPI
 */
void
lapic_send_startup_ipi(uint8_t vector)
{
    uint32_t icrl;
    uint32_t icrh;
    uint64_t apic_base;

    apic_base = lapic_base_addr();

    do {
        icrl = mfrd32(apic_base + APIC_ICR_LOW);
        icrh = mfrd32(apic_base + APIC_ICR_HIGH);
        /* Wait until it's idle */
    } while ( icrl & (APIC_ICR_SEND_PENDING) );

    icrl = (icrl & ~0x000cdfff) | APIC_ICR_STARTUP | APIC_ICR_DEST_ALL_EX_SELF
        | vector;
    icrh = (icrh & 0x000fffff);

    mfwr32(apic_base + APIC_ICR_HIGH, icrh);
    mfwr32(apic_base + APIC_ICR_LOW, icrl);
}

/*
 * lapic_bcast_fixed_ipi -- broadcast fixed IPI
 */
void
lapic_bcast_fixed_ipi(uint8_t vector)
{
    uint32_t icrl;
    uint32_t icrh;
    uint64_t apic_base;

    apic_base = lapic_base_addr();

    icrl = mfrd32(apic_base + APIC_ICR_LOW);
    icrh = mfrd32(apic_base + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | APIC_ICR_FIXED | APIC_ICR_DEST_ALL_EX_SELF
        | vector;
    icrh = (icrh & 0x000fffff);

    mfwr32(apic_base + APIC_ICR_HIGH, icrh);
    mfwr32(apic_base + APIC_ICR_LOW, icrl);
}

/*
 * lapic_send_fixed_ipi -- send a fixed IPI to the specified destination
 */
void
lapic_send_fixed_ipi(int dst, uint8_t vector)
{
    uint32_t icrl;
    uint32_t icrh;
    uint64_t apic_base;

    apic_base = lapic_base_addr();

    icrl = mfrd32(apic_base + APIC_ICR_LOW);
    icrh = mfrd32(apic_base + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | APIC_ICR_FIXED | APIC_ICR_DEST_NOSHORTHAND
        | vector;
    icrh = (icrh & 0x000fffff) | ((uint32_t)dst << 24);

    mfwr32(apic_base + APIC_ICR_HIGH, icrh);
    mfwr32(apic_base + APIC_ICR_LOW, icrl);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
