#!/bin/bash

if [ $# -lt 2 ]; then
	echo -e "bytes need to be specified.\n"
fi

if [ ! -r "$1" ];then
	echo -e "file not exist or can\'t be read"
	exit 1
fi

size=`ls -l "$1" | awk '{ print $5 }'`

if [ $size -gt $(( $2 - 2 )) ];then
	echo -e "\033[1;31mfile size $size is too big!\033[0m"
	rm "$1"
	exit 1
fi

echo -e "\033[1;31mfile size $size is OK!\033[0m"

exbytes=$(( $2 - $size - 2 ))

for i in $( seq 1 $exbytes )
do
	printf "\0" >> "$1"
done

printf "\x55\xaa" >> "$1"
