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
 * Initialize the task manager
 */
int
task_mgr_init(size_t atsize)
{
    int ret;
    int i;
    int nr;

    /* Allocate the process slab */
    ret = kmem_slab_create_cache(SLAB_PROC, sizeof(proc_t));
    if ( ret < 0 ) {
        return -1;
    }

    /* Allocate the file descriptor slab */
    ret = kmem_slab_create_cache(SLAB_FILDES, sizeof(proc_t));
    if ( ret < 0 ) {
        return -1;
    }

    /* Allocate the kernel stack slab */
    ret = kmem_slab_create_cache(SLAB_TASK_STACK, KSTACK_SIZE);
    if ( ret < 0 ) {
        return -1;
    }

    /* Allocate the task */
    ret = kmem_slab_create_cache(SLAB_TASK, sizeof(task_t) + atsize);
    if ( ret < 0 ) {
        return -1;
    }

    /* Initialize the process table */
    nr = (sizeof(proc_t *) * PROC_NR + MEMORY_PAGESIZE - 1) / MEMORY_PAGESIZE;
    g_kvar->procs = memory_alloc_pages(&g_kvar->mm, nr, MEMORY_ZONE_KERNEL, 0);
    if ( NULL == g_kvar->procs ) {
        return -1;
    }
    for ( i = 0; i < PROC_NR; i++ ) {
        g_kvar->procs[i] = NULL;
    }

    /* Initialize the task manager */
    g_kvar->task_mgr.lock = 0;

    return 0;
}

/*
 * Allocate a task
 */
task_t *
task_alloc(void)
{
    task_t *t;

    /* Prepare a task data structure */
    t = memory_slab_alloc(&g_kvar->slab, SLAB_TASK);
    if ( NULL == t ) {
        return NULL;
    }
    t->arch = (void *)t + sizeof(task_t);

    /* Prepare kernel stack */
    t->kstack = memory_slab_alloc(&g_kvar->slab, SLAB_TASK_STACK);
    if ( NULL == t->kstack ) {
        memory_slab_free(&g_kvar->slab, SLAB_TASK, t);
        return NULL;
    }

    /* Set the initial values */
    t->proc = NULL;
    t->id = 0;
    t->state = TASK_READY;
    t->next = NULL;
    t->credit = 0;

    return t;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
