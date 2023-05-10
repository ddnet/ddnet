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

TMP_OFFSET=$(grep -E -o "\(with offset [0-9A-Fa-f]+\)" "$2" | grep -E -o "[0-9A-Fa-f]*")
if [ -z "$TMP_OFFSET" ]; then
	TMP_OFFSET=$(grep -E -o "^[0-9A-Fa-f]+-[0-9A-Fa-f]+ \S+\.exe" "$2" | grep -E -o "^[0-9A-Fa-f]+")
	if [ -z "$TMP_OFFSET" ]; then
		TMP_OFFSET="0"
		if ! grep -q -E -o "\s\S+\.exe!" "$2"; then
			printf "\e[31m%s\e[30m\n" "Module offset not found; addresses will be absolute"
			echo -en "\e[0m"
		fi
	fi
fi

if [ "$TMP_OFFSET" != "0" ]; then
	echo "Module offset: 0x$TMP_OFFSET"
fi

ADDR_BASE=$(winedump -f "$1" | grep -E -o "image base[ ]*0x[0-9A-Fa-f]*" | grep -E -o "0x[0-9A-Fa-f]+" | tail -1)
echo "Image base: $ADDR_BASE"

ADDR_PC_REGEX='[0-9A-Fa-f]+ [0-9A-Fa-f]+ [0-9A-Fa-f]+ [0-9A-Fa-f]+'
while read -r line
do
	if [[ $line =~ $ADDR_PC_REGEX ]]
	then
		RELATIVE_ADDR=$(echo "$line" | grep -E -o -m 1 "\s\S+\.exe!.*0x([0-9A-Fa-f]+)" | grep -E -o "0x[0-9A-Fa-f]+" | head -1)
		if [ -z "$RELATIVE_ADDR" ]; then
			TMP_ADDR=$(echo "$line" | grep -E -o -m 1 "[0-9A-Fa-f]+ " | head -1)
			REAL_ADDR=$(printf '0x%X\n' "$(((0x$TMP_ADDR-0x$TMP_OFFSET)+ADDR_BASE))")
		else
			REAL_ADDR=$(printf '0x%X\n' "$((RELATIVE_ADDR+ADDR_BASE))")
		fi
		addr2line -e "$1" -a -p -f -C -i "$REAL_ADDR" | sed 's/ [^ ]*\/src\// src\//g'
	fi
done < "$2"
