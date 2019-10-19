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
#include <sys/fcntl.h>
#include <sys/stat.h>
#include "kernel.h"
#include "proc.h"
#include "vfs.h"
#include "timer.h"
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
 * Read input
 *
 * SYNOPSIS
 *      ssize_t
 *      sys_read(int fildes, void *buf, size_t nbyte);
 *
 * DESCRIPTION
 *      The sys_read() function attempts to read nbyte bytes of data from the
 *      object referenced by the descriptor fildes into the buffer pointed by
 *      buf.
 *
 * RETURN VALUES
 *      If success, the number of bytes actually read is returned.  Upon reading
 *      end-of-file, zero is returned.  Otherwise, a -1 is returned.
 */
ssize_t
sys_read(int fildes, void *buf, size_t nbyte)
{
    task_t *t;
    fildes_t *fd;

    /* Resolve the corresponding fildes_t from the file descriptor number */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }
    if ( NULL == t->proc->fds[fildes] ) {
        /* Not opened */
        return -1;
    }
    fd = t->proc->fds[fildes];

    return -1;
}

/*
 * Write output
 *
 * SYNOPSIS
 *      ssize_t
 *      sys_write(int fildes, const void *buf, size_t nbyte);
 *
 * DESCRIPTION
 *      The sys_write() function attempts to write nbyte bytes of data to the
 *      object referenced by the descriptor fildes from the buffer pointed by
 *      buf.
 *
 * RETURN VALUES
 *      Upon successful completion, the number of bytes which were written is
 *      returned.  Otherwise, a -1 is returned.
 */
ssize_t
sys_write(int fildes, const void *buf, size_t nbyte)
{
    task_t *t;
    fildes_t *fd;

    /* Resolve the corresponding fildes_t from the file descriptor number */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }
    if ( NULL == t->proc->fds[fildes] ) {
        /* Not opened */
        return -1;
    }
    fd = t->proc->fds[fildes];

    return -1;
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
    int fd;
#if 0
    size_t len;
    void *p;
    struct stat stat;
#endif

    /* Open */
    fd = sys_open(path, O_RDONLY);
    if ( fd < 0 ) {
        return -1;
    }

#if 0
    /* Get the file stats */
    fstat(fd, &stat);

    /* MMap */
    len = 0;
    p = sys_mmap((void *)0x80000000, len, PROT_READ | PROT_EXEC, MAP_FIXED, fd
                 0);
#endif

    return -1;
}

/*
 * Open or create a file for reading or writing
 *
 * SYNOPSIS
 *      int
 *      sys_open(const char *path, int oflags);
 *
 * DESCRIPTION
 *      The sys_open() function attempts to open a file specified by path for
 *      reading and/or writing, as specified by the argument oflag.
 *
 *      The flags specified for the oflag argument are formed by or'ing the
 *      following values (to be implemented):
 *
 *              O_RDONLY        open for reading only
 *              O_WRONLY        open for writing only
 *              O_RDWR          open for reading and writing
 *              O_NONBLOCK      do not block on open or for data to become
 *                              available
 *              O_APPEND        append on each write
 *              O_CREAT         create a file if it does not exist
 *              O_TRUNC         truncate size to 0
 *              O_EXCL          error if O_CREAT and the file exists
 *              O_SHLOCK        atomically obtain a shared lock
 *              O_EXLOCK        atomically obtain an exclusive lock
 *              O_NOFOLLOW      do not follow symlinks
 *              O_SYMLINK       allow open of symlinks
 *              O_EVTONLY       descriptor requested for event notifications
 *                              only
 *              O_CLOEXEC       mark as close-on-exec
 *
 * RETURN VALUES
 *      If success, sys_open() returns a non-negative integer, termed a file
 *      descriptor.  It returns -1 on failure.
 */
int
sys_open(const char *path, int oflag, ...)
{
    const char *dir;

    /* Resolve the filesystem */
    dir = path;
    while ( '\0' != *path ) {
        if ( '/' == *path ) {
            /* Delimiter */
            break;
        }
        path++;
    }

    return -1;
}

/*
 * Allocate memory, or map files or devices into memory
 *
 * SYNOPSIS
 *      The sys_mmap() system call causes the pages starting at addr and
 *      continuing for at most len bytes to be mapped from the object described
 *      by fd, starting at byte offset offset.  If offset or len is not a
 *      multiple of the pagesize, the mapped region may extend past the
 *      specified range.  Any extension beyond the end of the mapped object will
 *      be zero-filled.
 *
 *      The addr argument is used by the system to determine the starting
 *      address of the mapping, and its interpretation is dependent on the
 *      setting of the MAP_FIXED flag.  If MAP_FIXED is specified in flags, the
 *      system will try to place the mapping at the specified address, possibly
 *      removing a mapping that already exists at that location.  If MAP_FIXED
 *      is not specified, then the system will attempt to use the range of
 *      addresses starting at addr if they do not overlap any existing mappings,
 *      including memory allocated by malloc(3) and other such allocators.
 *      Otherwise, the system will choose an alternate address for the mapping
 *      (using an implementation dependent algorithm) that does not overlap any
 *      existing mappings.  In other words, without MAP_FIXED the system will
 *      attempt to find an empty location in the address space if the specified
 *      address range has already been mapped by something else.  If addr is
 *      zero and MAP_FIXED is not specified, then an address will be selected by
 *      the system so as not to overlap any existing mappings in the address
 *      space.  In all cases, the actual starting address of the region is
 *      returned.  If MAP_FIXED is specified, a successful mmap deletes any
 *      previous mapping in the allocated address range.  Previous mappings are
 *      never deleted if MAP_FIXED is not specified.
 *
 * RETURN VALUES
 *      Upon successful completion, sys_mmap() returns a pointer to the mapped
 *      region.  Otherwise, a value of MAP_FAILED is returned.
 */
void *
sys_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    return (void *)-1;
}

/*
 * Suspend thread execution for an interval measured in nanoseconds
 *
 * SYNOPSIS
 *      int
 *      sys_nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
 *
 * DESCRIPTION
 *      The sys_nanosleep() function causes the calling thread to sleep for the
 *      amount of time specified in ratp (the actual time slept may be longer,
 *      due to system latencies and possible limitations in the timer resolution
 *      of the hardware).  An unmasked signal will cause sys_nanosleep() to
 *      terminate the sleep early, regardless of the SA_RESTART value on the
 *      interrupting signal.
 *
 * RETURN VALUES
 *      If sys_nanosleep() returns because the requested time has elapsed, the
 *      value returned will be zero.
 *
 *      If sys_nanosleep() returns due to the delivery of a signal, the value
 *      returned will be the -1, and the global variable errno will be set to
 *      indicate the interruption.  If rmtp is non-NULL, the timespec structure
 *      it references is updated to contain the unslept amount (the request time
 *      minus the time actually slept).
 */
int
sys_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    task_t *t;
    uint64_t fire;
    uint64_t delta;
    timer_event_t *e;
    timer_event_t **ep;

    /* Get the currently running task */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }

    /* Calculate the time to fire */
    fire = rqtp->tv_sec * HZ + (rqtp->tv_nsec * HZ / 1000000000)
        + g_kvar->jiffies;

    /* Allocate an event */
    e = kmem_slab_alloc("timer_event");
    if ( NULL == e ) {
        return -1;
    }
    e->jiffies = fire;
    e->proc = t->proc;
    e->next = NULL;

    /* Search the appropriate position to inesrt the timer event */
    ep = &g_kvar->timer;
    while ( NULL != *ep && fire < (*ep)->jiffies ) {
        ep = &(*ep)->next;
    }
    /* Insert the event */
    e->next = *ep;
    *ep = e;

    /* Set the task state to blocked */
    t->state = TASK_BLOCKED;
    t->signaled = 0;

    /* Switch the task */
    task_switch();

    /* Will be resumed from here when awake */
    if ( t->signaled ) {
        /* Wake up by another signal */
        if ( NULL != rmtp ) {
            if ( fire < g_kvar->jiffies ) {
                /* No remaining time */
                rmtp->tv_sec = 0;
                rmtp->tv_nsec = 0;
            } else {
                /* Remaining time */
                delta = fire - g_kvar->jiffies;
                rmtp->tv_sec = delta / HZ;
                rmtp->tv_nsec = (delta % HZ) * 1000000000 / HZ;
            }
        }
        t->signaled = 0;
        return -1;
    }

    return 0;
}

/*
 * Mount a filesystem
 *
 * SYNOPSIS
 *      The mount() function grafts a filesystem object onto the system file
 *      tree at the point dir.  The argument data describes the filesystem
 *      object to be mounted.  The argument type tells the kernel how to
 *      interpret data.  The contents of the filesystem become available through
 *      the new mount point dir.  Any files in dir at the time of successful
 *      mount are swept under the carpet so to speak, and are unavailable until
 *      the filesystem is unmounted.
 *
 * RETURN VALUES
 *      The mount() returns the value 0 if the mount was successful, otherwise
 *      -1 is returned.
 */
int
sys_mount(const char *type, const char *dir, int flags, void *data)
{
    return -1;
}

/*
 * Get file status
 *
 * SYNOPSIS
 *      The fstat() function obtains information about an open file known by the
 *      file descriptor fildes.
 *
 * RETURN VALUES
 *      Upon successful completion, a value of 0 is returned.  Otherwise, a
 *      value of -1 is returned.
 */
int
sys_fstat(int fildes, struct stat *buf)
{
    task_t *t;
    fildes_t *fd;

    /* Resolve fildes from the process */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }
    if ( NULL == t->proc->fds[fildes] ) {
        /* Not opened */
        return -1;
    }
    fd = t->proc->fds[fildes];

    return -1;
}

#define INITRAMFS_BASE  0xc0030000
struct initrd_entry {
    char name[16];
    uint64_t offset;
    uint64_t size;
};

/*
 * Execute from initramfs (initrd)
 */
int
sys_initexec(const char *path, char *const argv[], char *const envp[])
{
    struct initrd_entry *e;
    void *start;
    size_t size;
    int i;
    task_t *t;
    int ret;

    /* Get the currently running task */
    t = this_task();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }

    /* Search the file specified by path from initrd */
    e = (void *)INITRAMFS_BASE;
    start = NULL;
    for ( i = 0; i < 128; i++ ) {
        if ( 0 == kstrcmp(path, e->name) ) {
            /* Found */
            start = (void *)INITRAMFS_BASE + e->offset;
            size = e->size;
            break;
        }
        e++;
    }
    if ( NULL == start ) {
        /* Not found */
        return -1;
    }

    /* Initialize the task */
    ret = task_init(t, (void *)PROC_PROG_ADDR);
    if ( ret < 0 ) {
        return -1;
    }

    /* Update the process name */
    kstrlcpy(t->proc->name, path, PATH_MAX);

    /* Copy the program */
    kmemcpy((void *)PROC_PROG_ADDR, start, size);

    /* Execute the task */
    task_exec(t);

    /* will never reach here */
    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
