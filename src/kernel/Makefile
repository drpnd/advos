#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-fleading-underscore -I../include

kernel: arch/x86_64/asm.o kernel.o memory.o
	$(LD) -N -T kernel.ld -o $@ $^

clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f kernel
