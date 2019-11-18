#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

PHONY=all
all: initrd
	$(MAKE) -C boot mbr
	$(MAKE) -C boot bootmon
	$(MAKE) -C kernel kernel
	./create_image.sh advos.img boot/mbr boot/bootmon kernel/kernel initrd
	$(MAKE) -C boot pxeboot

# libc
LIBCOBJS=lib/string.o lib/arch/x86_64/libc.o lib/arch/x86_64/libcasm.o \
	lib/arch/x86_64/libadvos.o
CFLAGS=-g -O3 -fleading-underscore -mcmodel=large -nostdlib -nodefaultlibs -fno-builtin -fno-stack-protector -fno-pie -mno-avx -I./include

initrd: $(LIBCOBJS) lib/crt0.o
	$(MAKE) -C servers/init
	$(MAKE) -C drivers/tty
	./create_initrd.sh initrd servers/init/init:init drivers/tty/tty:tty

PHONY+=clean
clean:
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	$(MAKE) -C servers/init clean
	rm -f libc.a
	rm -f initrd
	rm -f advos.img

.PHONY: $(PHONY)
