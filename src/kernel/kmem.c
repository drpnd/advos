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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "memory.h"
#include "kernel.h"

union kmem_data {
    virt_memory_data_t vmem;
    union kmem_data *next;
};

virt_memory_data_t * kmem_data_alloc(virt_memory_t *);
void kmem_data_free(virt_memory_t *, virt_memory_data_t *);

/*
 * Allocate virtual memory data
 */
union virt_memory_data *
kmem_data_alloc(virt_memory_t *vmem)
{
    union virt_memory_data *data;

    if ( NULL == vmem->lists ) {
        return NULL;
    }
    data = vmem->lists;
    vmem->lists = data->next;

    /* Zeros */
    kmemset(data, 0, sizeof(union virt_memory_data));

    return data;
}

/*
 * Free virtual memory data
 */
void
kmem_data_free(virt_memory_t *vmem, union virt_memory_data *data)
{
    data->next = vmem->lists;
    vmem->lists = data;
}

/*
 * Initialize kernel memory
 */
int
kmem_init(phys_memory_t *phys, uintptr_t p2v)
{
    virt_memory_allocator_t allocator;
    union kmem_data *data;
    size_t nr;
    size_t i;

    /* Allocate 8 MiB for page management */
    data = phys_mem_alloc(phys, 11, MEMORY_ZONE_KERNEL, 0);
    if ( NULL == data ) {
        return -1;
    }
    /* P2V */
    data = (void *)data + p2v;
    nr = (MEMORY_PAGESIZE << 9) / sizeof(union kmem_data);
    for ( i = 1; i < nr; i++ ){
        data[i - 1].next = &data[i];
    }
    data[nr - 1].next = NULL;
    allocator.allocator = data;
    allocator.alloc = kmem_data_alloc;
    allocator.free = kmem_data_free;

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
