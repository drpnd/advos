#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

include ../../app.mk

.PHONY: all clean

tty: main.o console.o
	$(LD) -T ../../app.ld -o $@ $^

all: tty

clean:
	rm -rf tty
