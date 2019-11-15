#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

## Save the number of arguments
argc=$#

if [ -z "$1" ];
then
    echo "Usage: $0 <output> [list of <path:filename>]" >& 2
fi

## Output file
outfile=$1
shift

## Source directory (working directory)
srcdir="./src"

## Tool check
toolcheck()
{
    name=$1
    which $name > /dev/null 2>&1
    if [ $? -ne 0 ];
    then
	echo "Error: $name not found." 1>&2
	return -1
    fi
    return 0
}
toolcheck 'xxd' || exit
toolcheck 'sed' || exit
toolcheck 'dd' || exit

## Reset the file entries
rm -f $outfile
dd if=/dev/zero of=$outfile seek=0 bs=1 count=4096 conv=notrunc > /dev/null 2>&1

## Print 64-bit little endian value
printle64()
{
    value=$1

    printf "0: %.16x" $value \
       | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/' \
       | xxd -r

    return 0
}

## Process each argument
entry=0
i=2
while [ $i -le $argc ];
do
    ## Parse the argument
    arg=$1
    target=`echo $arg | cut -d : -f 1`
    fname=`echo $arg | cut -d : -f 2`
    ## Check the size of initramfs
    offset=`wc -c < $outfile | tr -d ' '`
    fsize=`wc -c < $target | tr -d ' '`

    ## Check the filename length
    len=`printf "$fname" | wc -c`
    if [ $len -ge 31 ];
    then
	echo "Error: Filename $fname exceeds the maximum allowed length." 1>&2
	exit -1
    fi

    ## Write the filename
    pos=`expr $entry \* 32`
    printf "$fname\000" | dd of=$outfile seek=$pos bs=1 conv=notrunc > /dev/null 2>&1
    ## Write the offset to the file (little endian)
    pos=`expr $entry \* 32 + 16`
    printle64 $offset | dd of=$outfile bs=1 seek=$pos conv=notrunc > /dev/null 2>&1
    ## Write the size of the file (little endian)
    pos=`expr $entry \* 32 + 24`
    printle64 $fsize | dd of=$outfile bs=1 seek=$pos conv=notrunc > /dev/null 2>&1
    ## Write the content of the file
    dd if=$target of=$outfile seek=$offset bs=1 conv=notrunc > /dev/null 2>&1

    ## Go to the next file
    shift
    i=`expr $i + 1`
    entry=`expr $entry + 1`
done

