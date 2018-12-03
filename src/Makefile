#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#
all:
	make -C boot mbr
	make -C boot bootmon
	./create_image.sh advos.img boot/mbr boot/bootmon
