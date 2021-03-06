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

#ifndef _ADVOS_CONST_H
#define _ADVOS_CONST_H

/* Maximum number of supported processors */
#define MAX_PROCESSORS          128
#define MAX_MEMORY_REGIONS      128

/* Trampoline: 0x70 (0x70000) */
#define TRAMPOLINE_VEC          0x70
#define TRAMPOLINE_MAX_SIZE     0x1000

/* Temporary page table */
#define PGT_BOOT                0x69000

/* Physical addresses for application processor variables */
#define APVAR_CR3               0x00068f00
#define APVAR_SP                0x00068f08
#define AP_SPIN_LOCK            0x00005d00
#define AP_SHARED_STACK         0x00005c00

/* GDT and IDT */
#define GDT_ADDR                0xc0074000ULL
#define GDT_MAX_SIZE            0x2000
#define IDT_ADDR                0xc0076000ULL
#define IDT_MAX_SIZE            0x2000

/* Per-core data (256-byte per core) */
#define CPU_DATA_SIZE_SHIFT     8
#define CPU_DATA_SIZE           (1 << CPU_DATA_SIZE_SHIFT)
#define CPU_DATA_BASE           0xc0060000
#define CPU_DATA(i)             (CPU_DATA_BASE  \
                                 + ((uint64_t)(i) << CPU_DATA_SIZE_SHIFT))
#define CPU_TSS_BASE            (CPU_DATA_BASE + 0)
#define CPU_TSS(i)              (CPU_TSS_BASE   \
                                 + ((uint64_t)(i) << CPU_DATA_SIZE_SHIFT))
#define CPU_TASK_BASE           (CPU_DATA_BASE + 104)
#define CPU_TASK(i)             (CPU_TASK_BASE  \
                                 + ((uint64_t)(i) << CPU_DATA_SIZE_SHIFT))

/* struct arch_task */
#define TASK_RP                 0
#define TASK_SP0                8
#define TASK_CR3                16

/* Task (struct arch_cpu_data) */
#define TASK_CUR                0
#define TASK_NEXT               8
#define TASK_IDLE               16

/* GDT selectors */
#define GDT_NR                  7
#define GDT_NULL_SEL            (0 << 3)
#define GDT_RING0_CODE_SEL      (1 << 3)
#define GDT_RING0_DATA_SEL      (2 << 3)
#define GDT_RING3_CODE32_SEL    (3 << 3)
#define GDT_RING3_DATA32_SEL    (4 << 3)
#define GDT_RING3_CODE64_SEL    (5 << 3)
#define GDT_RING3_DATA64_SEL    (6 << 3)
#define GDT_TSS_SEL_BASE        (7 << 3)

/* Temporary GDT for application processors */
#define AP_GDT_CODE64_SEL       0x08    /* Code64 selector */
#define AP_GDT_DATA64_SEL       0x10    /* Data64 selector */
#define AP_GDT_CODE32_SEL       0x18    /* Code32 selector */
#define AP_GDT_DATA32_SEL       0x20    /* Data32 selector */
#define AP_GDT_CODE16_SEL       0x28    /* Code16 selector */
#define AP_GDT_DATA16_SEL       0x30    /* Data16 selector */

/* # of interrupts */
#define IDT_NR                  256

#define KERNEL_RELOCBASE        0xc0000000
#define INITRD_BASE             0xc0030000

/* Machine-specific registers (MSRs) */
#define MSR_APIC_BASE           0x1b
#define MSR_PLATFORM_INFO       0xce
#define MSR_IA32_EFER           0xc0000080
#define MSR_IA32_STAR           0xc0000081
#define MSR_IA32_LSTAR          0xc0000082
#define MSR_IA32_FMASK          0xc0000084

/* Interrupt vectors */
#define IV_LOC_TMR              0x40
#define IV_CRASH                0xfe

/* TSS */
#define TSS_SP0                 4

/* Video RAM */
#define VIDEO_RAM_80X25         0xc00b8000

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
