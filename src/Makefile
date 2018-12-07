#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	$(MAKE) -C boot mbr
	$(MAKE) -C boot bootmon
	./create_image.sh advos.img boot/mbr boot/bootmon

clean:
	$(MAKE) -C boot clean
	rm -f advos.img
