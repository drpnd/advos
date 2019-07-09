#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

.PHONY: clean

CFLAGS=-g -O3 -fleading-underscore -mcmodel=large -nostdlib -nodefaultlibs -fno-builtin -fno-stack-protector -fno-pie -mno-sse -mno-sse2 -mno-avx -I../include

# header dependencies
%.o: kernel.h memory.h proc.h tree.h \
	arch/x86_64/arch.h

# kernel
kernel: arch/x86_64/asm.o \
	kernel.o memory.o kmem.o physmem.o slab.o kmalloc.o strfmt.o \
	proc.o task.o sched.o tree.o syscall.o \
	arch/x86_64/trampoline.o arch/x86_64/ap_entry32.o arch/x86_64/ap_entry64.o \
	arch/x86_64/arch.o arch/x86_64/acpi.o arch/x86_64/apic.o \
	arch/x86_64/desc.o arch/x86_64/i8254.o arch/x86_64/pgt.o \
	arch/x86_64/vconsole.o arch/x86_64/tsc.o arch/x86_64/task.o
	$(LD) -N -T kernel.ld -o $@ $^
	$(LD) -N -T kerneldebug.ld -o kernel.dbg $^

clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f kernel
