/*_
 * Copyright (c) 2018-2019 Hirochika Asai <asai@jar.jp>
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

#ifndef _ADVOS_KERNEL_ARCH_H
#define _ADVOS_KERNEL_ARCH_H

#include <stdint.h>
#include "const.h"
#include "../../kernel.h"
#include "../../console.h"
#include "../../proc.h"

/*
 * TSS (104 bytes)
 */
struct tss {
    uint32_t reserved1;
    uint32_t rsp0l;
    uint32_t rsp0h;
    uint32_t rsp1l;
    uint32_t rsp1h;
    uint32_t rsp2l;
    uint32_t rsp2h;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t ist1l;
    uint32_t ist1h;
    uint32_t ist2l;
    uint32_t ist2h;
    uint32_t ist3l;
    uint32_t ist3h;
    uint32_t ist4l;
    uint32_t ist4h;
    uint32_t ist5l;
    uint32_t ist5h;
    uint32_t ist6l;
    uint32_t ist6h;
    uint32_t ist7l;
    uint32_t ist7h;
    uint32_t reserved4;
    uint32_t reserved5;
    uint16_t reserved6;
    uint16_t iomap;
} __attribute__ ((packed));

/*
 * Stackframe for 64-bit mode
 */
struct stackframe64 {
    /* Segment registers */
    uint16_t gs;
    uint16_t fs;

    /* Base pointer */
    uint64_t bp;

    /* Index registers */
    uint64_t di;
    uint64_t si;

    /* Generic registers */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t dx;
    uint64_t cx;
    uint64_t bx;
    uint64_t ax;

    /* Restored by `iretq' instruction */
    uint64_t ip;            /* Instruction pointer */
    uint64_t cs;            /* Code segment */
    uint64_t flags;         /* Flags */
    uint64_t sp;            /* Stack pointer */
    uint64_t ss;            /* Stack segment */
} __attribute__ ((packed));

/*
 * Task
 */
struct arch_task {
    /* Do not change the first three variables.  These must be on the top.  See
       asm.S and const.h. */

    /* Restart point (stackframe, part of kstack) */
    struct stackframe64 *rp;

    /* SP0 for TSS (TSS_SP0) */
    uint64_t sp0;

    /* CR3 (TASK_CR3) */
    uint64_t cr3;

    /* FPU/SSE registers */
    void *xregs;

    /* Kernel task (architecture-independent data structure) */
    task_t *task;
} __attribute__ ((packed));

/*
 * Processor's task information (must not exceed (256-104) bytes)
 */
struct arch_cpu_data {
    /* Do not change the first three variables.  These must be on the top.  See
       asm.S and const.h. */
    struct arch_task *cur_task;
    struct arch_task *next_task;
    struct arch_task *idle_task;

    /* FPU context */
    struct arch_task *fpu_task;
} __attribute__ ((packed));

#define sfence()        __asm__ __volatile__ ("sfence")

void sti(void);
void cli(void);
uint64_t cpuid(uint64_t, uint64_t *, uint64_t *, uint64_t *);
uint64_t rdtsc(void);
uint64_t rdmsr(uint64_t);
void wrmsr(uint64_t, uint64_t);
uint32_t mfrd32(uintptr_t);
void mfwr32(uintptr_t, uint32_t);
uint8_t inb(uint16_t);
uint16_t inw(uint16_t);
uint32_t inl(uint16_t);
void outb(uint16_t, uint8_t);
void outw(uint16_t, uint16_t);
void outl(uint16_t, uint32_t);
void lgdt(void *, uint64_t);
void sgdt(void *);
void lidt(void *);
void sidt(void *);
void lldt(uint16_t);
void ltr(uint16_t);
void clts(void);
void fxsave64(void *);
void fxrstor64(void *);
void xsave64(void *);
void xrstor64(void *);
void hlt(void);
void pause(void);

void task_replace(void *);
void task_restart(void);

/* Interrupt handlers */
void intr_null(void);
void intr_apic_loc_tmr(void);
void intr_crash(void);
void intr_de(void);
void intr_db(void);
void intr_nmi(void);
void intr_bp(void);
void intr_of(void);
void intr_br(void);
void intr_ud(void);
void intr_nm(void);
void intr_df(void);
void intr_cso(void);
void intr_ts(void);
void intr_np(void);
void intr_ss(void);
void intr_gp(void);
void intr_pf(void);
void intr_mf(void);
void intr_ac(void);
void intr_mc(void);
void intr_xm(void);
void intr_ve(void);
void intr_sx(void);
void intr_irq1(void);
void intr_crash(void);

/* Entry point to the syscall */
void syscall_entry(void);
void syscall_setup(uint64_t, uint64_t);

/* vconsole.c */
console_dev_t * vconsole_init(void);
int vconsole_write(console_dev_t *, const void *, size_t);

/* Defined in task.c */
int arch_task_init(task_t *, void *);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
