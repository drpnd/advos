#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-fleading-underscore

mbr: mbr.o
	$(LD) -N -T mbr.ld -o $@ $^
bootmon: bootmon.o entry16.o entry32.o entry64.o boot.o
	$(LD) -N -T bootmon.ld -o $@ $^
