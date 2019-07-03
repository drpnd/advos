#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

include ../../app.mk

.PHONY: all clean

init: init.o ../../libc.a
	$(LD) -T ../../app.ld -o $@ $^

all: init
