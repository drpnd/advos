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

#include "../../proc.h"
#include "arch.h"
#include "apic.h"
#include "pgt.h"

/*
 * Initialize the architecture-specific task data structure
 */
int
arch_task_init(task_t *t, void *entry)
{
    struct arch_task *at;

    at = t->arch;
    at->task = t;

    /* Restart point (in the kernel stack) */
    at->rp = t->kstack + KSTACK_SIZE - KSTACK_GUARD
        - sizeof(struct stackframe64);
    kmemset(at->rp, 0, sizeof(struct stackframe64));

    /* Kernel stack on interrupts */
    at->sp0 = (uint64_t)t->kstack + KSTACK_SIZE - KSTACK_GUARD;

    /* Set up the stackframe */
    at->rp->sp = (uint64_t)PROC_PROG_ADDR + PROC_PROG_SIZE - 16;
    at->rp->ip = (uint64_t)entry;
    at->rp->cs = GDT_RING3_CODE64_SEL + 3;
    at->rp->ss = GDT_RING3_DATA64_SEL + 3;
    at->rp->fs = GDT_RING3_DATA64_SEL + 3;
    at->rp->gs = GDT_RING3_DATA64_SEL + 3;
    at->rp->flags = 0x202;

    if ( NULL != t->proc ) {
        at->cr3 = ((pgt_t *)t->proc->vmem->arch)->cr3;
    }

    return 0;
}

/*
 * Get the current task
 */
task_t *
this_task(void)
{
    struct arch_cpu_data *cpu;
    struct arch_task *at;

    cpu = (struct arch_cpu_data *)CPU_TASK(lapic_id());
    at = cpu->cur_task;

    return at->task;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
