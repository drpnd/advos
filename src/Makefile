#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

.PHONY: all clean initrd

all: initrd
	$(MAKE) -C boot mbr
	$(MAKE) -C boot bootmon
	$(MAKE) -C kernel kernel
	./create_image.sh advos.img boot/mbr boot/bootmon kernel/kernel
	$(MAKE) -C boot pxeboot

# libc
LIBCOBJS=lib/arch/x86_64/libc.o lib/arch/x86_64/libcasm.o
CFLAGS=-g -O3 -fleading-underscore -mcmodel=large -nostdlib -nodefaultlibs -fno-builtin -fno-stack-protector -fno-pie -mno-sse -mno-sse2 -mno-avx -I./include

libc.a: $(LIBCOBJS)
	$(LD) -o $@ $^

initrd: libc.a lib/crt0.o
	$(MAKE) -C servers/init
	./create_initrd.sh initrd servers/init/init:init

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	rm -f libc.a
	rm -f advos.img
