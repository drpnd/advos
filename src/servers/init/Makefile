#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

include ../../app.mk

.PHONY: all clean

init: init.o
	$(LD) -T ../../app.ld -o $@ $^

all: init

clean:
	rm -rf init
