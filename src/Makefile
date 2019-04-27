#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	$(MAKE) -C boot mbr
	$(MAKE) -C boot bootmon
	$(MAKE) -C kernel kernel
	./create_image.sh advos.img boot/mbr boot/bootmon kernel/kernel
	$(MAKE) -C boot pxeboot

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	rm -f advos.img
