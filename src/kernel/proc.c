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

#include "kernel.h"
#include "proc.h"
#include "kvar.h"

/*
 * Create a new process
 */
proc_t *
proc_new(pid_t pid)
{
    proc_t *proc;
    int ret;

    /* Allocate proc_t */
    proc = memory_slab_alloc(&g_kvar->slab, SLAB_PROC);
    if ( NULL == proc ) {
        return NULL;
    }
    kmemset(proc, 0, sizeof(proc_t));

    /* Allocate a virtual memory */
    proc->vmem = g_kvar->mm.ifs.new();
    if ( NULL == proc->vmem ) {
        memory_slab_free(&g_kvar->slab, SLAB_PROC, proc);
        return NULL;
    }
    ret = virt_memory_block_add(proc->vmem, PROC_PROG_ADDR,
                                PROC_PROG_ADDR + PROC_PROG_SIZE - 1);
    if ( ret < 0 ) {
        /* ToDo: Free vmem */
        memory_slab_free(&g_kvar->slab, SLAB_PROC, proc);
        return NULL;
    }

    /* Allocate a task */
    proc->task = task_alloc();
    if ( NULL == proc->task ) {
        /* ToDo: Free vmem */
        memory_slab_free(&g_kvar->slab, SLAB_PROC, proc);
        return NULL;

    }

    proc->pid = pid;
    kmemset(proc->name, 0, PATH_MAX);
    proc->parent = NULL;
    proc->uid = 0;
    proc->gid = 0;

    proc->code.addr = 0;
    proc->code.size = 0;
    proc->exit_status = 0;

    return proc;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
