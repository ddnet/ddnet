#!/bin/bash

if [ -z ${1+x} ]; then
	printf "\e[31m%s\e[30m\n" "Did not pass executable file (full path)"
	printf "\e[31m%s\e[30m\n" "Usage: $0 <executable> <crash_log>"
	echo -en "\e[0m"
	exit 1
fi

if [ -z ${2+x} ]; then
	printf "\e[31m%s\e[30m\n" "Did not pass crash log file (full path)"
	printf "\e[31m%s\e[30m\n" "Usage: $0 <executable> <crash_log>"
	echo -en "\e[0m"
	exit 1
fi

TMP_OFFSET=$(grep -E -o "\(with offset [0-9A-F]+\)" "$2" | grep -E -o "[A-F0-9]*")
if [ -z "$TMP_OFFSET" ]; then
	TMP_OFFSET=$(grep -E -o "^[0-9A-F]+-[0-9A-F]+ .+\.exe" "$2" | grep -E -o "^[A-F0-9]+")
	if [ -z "$TMP_OFFSET" ]; then
		printf "\e[31m%s\e[30m\n" "Module offset not found; addresses will be absolute"
		echo -en "\e[0m"
	fi
fi

ADDR_PC_REGEX='[0-9A-F]+ [0-9A-F]+ [0-9A-F]+ [0-9A-F]+'
while read -r line
do
	if [[ $line =~ $ADDR_PC_REGEX ]]
	then
		TMP_ADDR=$(echo "$line" | grep -E -o -m 1 "[A-F0-9]+ " | head -1)
		ADDR_BASE=$(winedump -f "$1" | grep -E -o "image base[ ]*0x[0-9A-Fa-f]*" | grep -E -o "[0-9A-Fa-f]+" | tail -1)
		REAL_ADDR=$(printf '%X\n' "$(((0x$TMP_ADDR-0x$TMP_OFFSET)+0x$ADDR_BASE))")
		echo "Parsing address: $REAL_ADDR (img base: $ADDR_BASE)"
		addr2line -e "$1" "$REAL_ADDR"
	fi
done < "$2"
