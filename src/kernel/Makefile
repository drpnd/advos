#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-g -O3 -fleading-underscore -mcmodel=large -nostdlib -nodefaultlibs -fno-builtin -fno-stack-protector -fno-pie -mno-sse -mno-sse2 -mno-avx -I../include

# header dependencies
%.o: kernel.h memory.h proc.h tree.h \
	arch/x86_64/arch.h

# kernel
KERNEL_SOURCES=arch/x86_64/asm.o
KERNEL_SOURCES+=kernel.o
KERNEL_SOURCES+=memory.o
KERNEL_SOURCES+=kmem.o
KERNEL_SOURCES+=physmem.o
KERNEL_SOURCES+=slab.o
KERNEL_SOURCES+=kmalloc.o
KERNEL_SOURCES+=strfmt.o
KERNEL_SOURCES+=msg.o
KERNEL_SOURCES+=proc.o
KERNEL_SOURCES+=task.o
KERNEL_SOURCES+=sched.o
KERNEL_SOURCES+=tree.o
KERNEL_SOURCES+=syscall.o
KERNEL_SOURCES+=sysdriver.o
KERNEL_SOURCES+=vfs.o
KERNEL_SOURCES+=initramfs.o
KERNEL_SOURCES+=devfs.o
KERNEL_SOURCES+=arch/x86_64/trampoline.o
KERNEL_SOURCES+=arch/x86_64/ap_entry32.o
KERNEL_SOURCES+=arch/x86_64/ap_entry64.o
KERNEL_SOURCES+=arch/x86_64/arch.o
KERNEL_SOURCES+=arch/x86_64/acpi.o
KERNEL_SOURCES+=arch/x86_64/apic.o
KERNEL_SOURCES+=arch/x86_64/desc.o
KERNEL_SOURCES+=arch/x86_64/i8254.o
KERNEL_SOURCES+=arch/x86_64/pgt.o
KERNEL_SOURCES+=arch/x86_64/vconsole.o
KERNEL_SOURCES+=arch/x86_64/tsc.o
KERNEL_SOURCES+=arch/x86_64/task.o
kernel: $(KERNEL_SOURCES)
	$(LD) -N -T kernel.ld -o $@ $^
	$(LD) -N -T kerneldebug.ld -o kernel.dbg $^

PHONY=clean
clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f kernel

.PHONY: $(PHONY)
