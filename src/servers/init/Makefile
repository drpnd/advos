#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

include ../../app.mk

.PHONY: all clean

init: main.o
	$(LD) -T ../../app.ld -o $@ $^

all: init

clean:
	find . -name "*.o" | xargs rm -f
	rm -rf init
