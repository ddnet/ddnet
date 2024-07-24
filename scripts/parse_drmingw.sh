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

EXE_FILE=$1
EXE_FILE_FILENAME=$(basename "$EXE_FILE")
EXE_FILE_FILENAME_REGEX=$(printf '%s\n' "$EXE_FILE_FILENAME" | sed 's/\./\\./g')
CRASH_LOG_FILE=$2

# Determine module offset
MODULE_OFFSET=$(grep -E -o "\(with offset [0-9A-Fa-f]+\)" "$CRASH_LOG_FILE" | grep -E -o "[0-9A-Fa-f]*")
if [ -z "$MODULE_OFFSET" ]; then
	MODULE_OFFSET=$(grep -E -o "^[0-9A-Fa-f]+-[0-9A-Fa-f]+ ${EXE_FILE_FILENAME_REGEX}" "$CRASH_LOG_FILE" | grep -E -o "^[0-9A-Fa-f]+")
	if [ -z "$MODULE_OFFSET" ]; then
		MODULE_OFFSET="0"
		if ! grep -q -E -o "\s${EXE_FILE_FILENAME_REGEX}!" "$CRASH_LOG_FILE"; then
			printf "\e[31m%s\e[30m\n" "Module offset not found; addresses will be absolute"
			echo -en "\e[0m"
		fi
	fi
fi

if [ "$MODULE_OFFSET" != "0" ]; then
	echo "Module offset: 0x$MODULE_OFFSET"
fi

# Determine base address
ADDR_BASE=0x$(objdump -x "$EXE_FILE" | grep -E -o -m 1 "^ImageBase\s+[0-9A-Fa-f]+$" | grep -E -o "[0-9A-Fa-f]+$")
echo "Image base: $ADDR_BASE"
echo

function prine_line_for_address() {
	addr2line -e "$EXE_FILE" -a -p -f -C -i "$1" | sed 's/ [^ ]*\/src\// src\//g'
}

ADDR_PC_REGEX='[0-9A-Fa-f]+ [0-9A-Fa-f]+ [0-9A-Fa-f]+ [0-9A-Fa-f]+'
while read -r line; do
	if [[ $line =~ $ADDR_PC_REGEX ]]; then
		# Check for main executable file with address information
		EXE_FILE_RELATIVE_ADDR=$(echo "$line" | grep -E -o -m 1 "\s${EXE_FILE_FILENAME_REGEX}!.*0x[0-9A-Fa-f]+" | grep -E -o "0x[0-9A-Fa-f]+" | head -1)
		if [ -n "$EXE_FILE_RELATIVE_ADDR" ]; then
			prine_line_for_address "$(printf '0x%X\n' "$((EXE_FILE_RELATIVE_ADDR + ADDR_BASE))")"
			continue
		fi

		# Check for other module with defined symbol name and print the line literally
		MODULE_WITH_SYMBOL_NAME=$(echo "$line" | grep -E -o -m 1 "\S+\.\S+![A-Za-z0-9_]+\+0x[0-9A-Fa-f]+")
		if [ -n "$MODULE_WITH_SYMBOL_NAME" ]; then
			echo "$MODULE_WITH_SYMBOL_NAME"
			continue
		fi

		# Compatibilty with old crash logs: use the raw address and assume it belongs to the main executable
		RAW_ADDR=$(echo "$line" | grep -E -o -m 1 "[0-9A-Fa-f]+ " | head -1)
		prine_line_for_address "$(printf '0x%X\n' "$(((0x$RAW_ADDR - 0x$MODULE_OFFSET) + ADDR_BASE))")"
	fi
done < "$CRASH_LOG_FILE"
