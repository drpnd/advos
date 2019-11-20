#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-g -O3 -fleading-underscore -mcmodel=large -nostdlib -nodefaultlibs -fno-builtin -fno-stack-protector -fno-pie -mno-sse -mno-sse2 -mno-avx -I../include

# header dependencies
HEADERS=kernel.h
HEADERS+=memory.h
HEADERS+=proc.h
HEADERS+=tree.h
HEADERS+=arch/x86_64/arch.h
%.o: $(HEADERS)

# kernel
KOBJS=arch/x86_64/asm.o
KOBJS+=kernel.o
KOBJS+=memory.o
KOBJS+=kmem.o
KOBJS+=physmem.o
KOBJS+=slab.o
KOBJS+=kmalloc.o
KOBJS+=strfmt.o
KOBJS+=msg.o
KOBJS+=proc.o
KOBJS+=task.o
KOBJS+=sched.o
KOBJS+=tree.o
KOBJS+=syscall.o
KOBJS+=sysdriver.o
KOBJS+=vfs.o
KOBJS+=initramfs.o
KOBJS+=devfs.o
KOBJS+=arch/x86_64/trampoline.o
KOBJS+=arch/x86_64/ap_entry32.o
KOBJS+=arch/x86_64/ap_entry64.o
KOBJS+=arch/x86_64/arch.o
KOBJS+=arch/x86_64/acpi.o
KOBJS+=arch/x86_64/apic.o
KOBJS+=arch/x86_64/desc.o
KOBJS+=arch/x86_64/i8254.o
KOBJS+=arch/x86_64/pgt.o
KOBJS+=arch/x86_64/vconsole.o
KOBJS+=arch/x86_64/tsc.o
KOBJS+=arch/x86_64/task.o
kernel: $(KOBJS)
	$(LD) -N -T kernel.ld -o $@ $^
	$(LD) -N -T kerneldebug.ld -o kernel.dbg $^

PHONY=clean
clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f kernel

.PHONY: $(PHONY)
