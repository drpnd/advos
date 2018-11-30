#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#
all:
	@echo "Testing..."
	make -C boot mbr
	cp boot/mbr advos.img
	printf '\125\252' | dd of=advos.img bs=1 seek=510 conv=notrunc
	printf '\000' | dd of=advos.img bs=1 seek=1474559 conv=notrunc
