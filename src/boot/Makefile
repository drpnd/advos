#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-fleading-underscore -I../include

## MBR
mbr: mbr.o
	$(LD) -N -T mbr.ld -o $@ $^
bootmon: bootmon.o entry16.o entry32.o entry64.o boot.o
	$(LD) -N -T bootmon.ld -o $@ $^

## PXE boot
pxeboot: pxeboot.o entry16.o entry32.o entry64.o boot.o
	$(LD) -N -T pxe.ld -o $@ $^

clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f mbr
	rm -f bootmon
	rm -f pxeboot
