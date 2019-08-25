#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

PHONY=all
all:
	make -C src all

PHONY+=clean
clean:
	make -C src clean

PHONY+=test
test: all
	qemu-system-x86_64 -m 1024 \
		-smp cores=4,threads=1,sockets=2 \
		-numa node,nodeid=0,cpus=0-3 \
		-numa node,nodeid=1,cpus=4-7 \
		-drive id=disk,format=raw,file=src/advos.img,if=none \
		-device ahci,id=ahci \
		-device ide-drive,drive=disk,bus=ahci.0 \
		-boot a \
		-display curses

PHONY+=test-single
test-single: all
	qemu-system-x86_64 -m 1024 \
		-smp cores=1,threads=1,sockets=1 \
		-drive id=disk,format=raw,file=src/advos.img,if=none \
		-device ahci,id=ahci \
		-device ide-drive,drive=disk,bus=ahci.0 \
		-boot a \
		-display curses


.PHONY: $(PHONY)

