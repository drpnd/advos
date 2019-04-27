#!/bin/sh

## Create a new bridge interface and add an IPv4 address
brctl addbr br100
ip a add 192.168.253.1/24 dev br100
ip link set br100 up

## Run dhcp server
touch /var/lib/dhcp/dhcpd.leases
dhcpd -4 -pf /run/dhcpd.pid -cf /etc/dhcp/dhcpd.conf br100

## Run tftp server
in.tftpd --listen --secure /tftpboot

qemu-system-x86_64 -m 1024 -smp cores=4,threads=1,sockets=2 \
	-numa node,nodeid=0,cpus=0-3 \
	-numa node,nodeid=1,cpus=4-7 \
	-option-rom /usr/share/qemu/pxe-e1000.rom \
	-net bridge,br=br100 -net nic,macaddr=52:54:00:12:34:56 \
	-display curses

