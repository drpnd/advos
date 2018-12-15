#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-fleading-underscore -mcmodel=large -nostdlib -nodefaultlibs -fno-builtin -fno-stack-protector -I../include

kernel: arch/x86_64/asm.o kernel.o memory.o strfmt.o \
	arch/x86_64/trampoline.o arch/x86_64/ap_entry32.o \
	arch/x86_64/arch.o arch/x86_64/acpi.o arch/x86_64/apic.o
	$(LD) -N -T kernel.ld -o $@ $^

clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f kernel
