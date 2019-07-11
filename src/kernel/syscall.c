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

#include <sys/syscall.h>
#include "kernel.h"
#include "proc.h"
#include "kvar.h"

/*
 * Exit a process
 */
void
sys_exit(int status)
{
    task_t *t;

    /* Get the caller task */
    t = this_task();

    /* ToDo: Call atexit() */

    /* Set the state and exit status */
    t->state = TASK_TERMINATED;
    t->proc->exit_status = status;

    /* Infinite loop until this task has been removed */
    for ( ;; ) {
        hlt();
    }
}

/*
 * Create a new process (called from the assembly entry code)
 *
 * SYNOPSIS
 *      int
 *      sys_fork_c(void **task, pid_t *ret0, pd_t *ret1);
 *
 * DESCRIPTION
 *
 * RETURN VALUES
 *      Upon successful completion, the sys_fork() function returns a value of 0
 *      to the child process and returns the process ID of the child process to
 *      the parent process.  Otherwise, a value of -1 is returned to the parent
 *      process and no child process is created.
 */
int
sys_fork_c(void **task, pid_t *ret0, pid_t *ret1)
{
    task_t *t;
    proc_t *proc;
    pid_t pid;
    int i;

    /* Get the currently running task, and the corresponding process */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }

    /* Search an available pid */
    pid = -1;
    for ( i = 0; i < PROC_NR; i++ ) {
        if ( NULL == g_kvar->procs[i] ) {
            pid = i + 1;
            break;
        }
    }
    if ( pid < 0 ) {
        return -1;
    }

    /* Create a new process */
    proc = proc_fork(t->proc, pid);
    if ( NULL == proc ) {
        return -1;
    }

    /* Set the current process to the parent of the new process */
    proc->parent = t->proc;

    /* Set the process to the process table */
    g_kvar->procs[pid - 1] = proc;

    *task = proc->task->arch;
    *ret0 = 0;
    *ret1 = pid;

    return 0;
}

/*
 * Execute a file
 *
 * SYNOPSIS
 *      int
 *      sys_execve(const char *path, char *const argv[], char *const envp[]);
 *
 * DESCRIPTION
 *      The function sys_execve() transforms the calling process into a new
 *      process. The new process is constructed from an ordinary file, whose
 *      name is pointed by path, called the new process file.  In the current
 *      implementation, this file should be an executable object file, whose
 *      text section virtual address starts from 0x80000000.  The design of
 *      relocatable object support is still ongoing.
 *
 * RETURN VALUES
 *      As the function sys_execve() overlays the current process image with a
 *      new process image, the successful call has no process to return to.  If
 *      it does return to the calling process, an error has occurred; the return
 *      value will be -1.
 */
int
sys_execve(const char *path, char *const argv[], char *const envp[])
{
    return -1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
