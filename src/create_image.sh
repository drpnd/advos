#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

## This script takes two arguments.  The options are described below in the error
## message
if [ -z "$1" -o -z "$2" -o -z "$3" ];
then
    echo "Usage: $0 <output> <mbr> <boot-monitor>" >& 2
    exit -1
fi

## Arguments to named variables
outimg=$1
mbr=$2
bootmon=$3

## Get the size of each file
mbr_size=`wc -c < $mbr`
bootmon_size=`wc -c < $bootmon`

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

## MBR
cp $mbr $outimg
## Boot signature
printf '\125\252' | dd of=$outimg bs=1 seek=510 conv=notrunc > /dev/null 2>&1
## Boot monitor
dd if=$bootmon of=$outimg bs=1 seek=512 conv=notrunc > /dev/null 2>&1

## Increase the image size to the floppy disk
printf '\000' | dd of=advos.img bs=1 seek=1474559 conv=notrunc > /dev/null 2>&1

