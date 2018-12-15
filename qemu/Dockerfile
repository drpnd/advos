FROM ubuntu:16.04

MAINTAINER Hirochika Asai <panda@jar.jp>

## Install build-essentials and qemu
RUN apt-get update
RUN apt-get install -y --no-install-recommends build-essential qemu-system

## Copy source to the workdir
COPY src /usr/src
WORKDIR /usr/src
RUN make

## Run the OS with qemu
CMD ["qemu-system-x86_64", "-m", "1024", \
	"-smp", "cores=4,threads=1,sockets=2", \
	"-numa", "node,nodeid=0,cpus=0-3", \
	"-numa", "node,nodeid=1,cpus=4-7", \
	"-drive", "id=disk,format=raw,file=advos.img,if=none", \
	"-device", "ahci,id=ahci", \
	"-device", "ide-drive,drive=disk,bus=ahci.0", \
	"-boot", "a", "-display", "curses"]
