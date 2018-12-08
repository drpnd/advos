#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

CFLAGS=-fleading-underscore -I../include

kernel: kasm.o kernel.o
	$(LD) -N -T kernel.ld -o $@ $^

clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.a" | xargs rm -f
	rm -f kernel
