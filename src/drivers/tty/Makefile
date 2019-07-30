#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

include ../../app.mk

.PHONY: all clean

tty: main.o tty.o console.o kbd.o
	$(LD) -T ../../app.ld -o $@ $^

all: tty

clean:
	rm -rf tty
