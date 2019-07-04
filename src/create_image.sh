#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

## This script takes four arguments.  The options are described below in the error
## message.
if [ -z "$1" -o -z "$2" -o -z "$3" -o -z "$4" -o -z "$5" ];
then
    echo "Usage: $0 <output> <mbr> <boot-monitor> <kernel> <initrd>" >& 2
    exit -1
fi

## Arguments to named variables
outimg=$1
mbr=$2
bootmon=$3
kernel=$4
initrd=$5

## Get the size of each file
mbr_size=`wc -c < $mbr`
bootmon_size=`wc -c < $bootmon`
kernel_size=`wc -c < $kernel`
initrd_size=`wc -c < $initrd`

## Check the size
if [ $mbr_size -ge 446 ];
then
    echo "Error: $mbr is too large (must be <= 446 bytes)" >& 2
    exit 1
fi
if [ $bootmon_size -ge 16384 ];
then
    echo "Error: $bootmon is too large (must be <= 16384 bytes)" >& 2
    exit 1
fi
if [ $kernel_size -ge 131072 ];
then
    echo "Error: $kernel is too large (must be <= 131072 bytes)" >& 2
    exit 1
fi
if [ $initrd -ge 131072 ];
then
    echo "Error: $initrd is too large (must be <= 131072 bytes)" >& 2
    exit 1
fi

## MBR
cp $mbr $outimg
## Boot signature
printf '\125\252' | dd of=$outimg bs=1 seek=510 conv=notrunc > /dev/null 2>&1
## Boot monitor
dd if=$bootmon of=$outimg bs=1 seek=512 conv=notrunc > /dev/null 2>&1
## Kernel
dd if=$kernel of=$outimg bs=1 seek=16896 conv=notrunc > /dev/null 2>&1
## initrd
dd if=$initrd of=$outimg bs=1 seek=147968 conv=notrunc > /dev/null 2>&1

## Increase the image size to the floppy disk
printf '\000' | dd of=advos.img bs=1 seek=1474559 conv=notrunc > /dev/null 2>&1
