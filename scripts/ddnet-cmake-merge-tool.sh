#!/bin/bash

# Run the setup script first to activate this merge driver
# it will then be called on conflicts in the CMakeLists.txt file
#
# ./scripts/setup-merge-tools.sh

RESET='\033[0m'
RED='\033[0;31m'

function color() {
	if [ "$TTY" != "" ]
	then
		return
	fi
	echo -en "$1"
}

if ! git merge-file "$1" "$2" "$3"
then
	file_types='h|cpp|json|rules|png|wv|txt|ttf|ttc|otf|frag|vert|pem|map'
	critical_diff="$(
		diff "$1" "$2" |
			grep -E '^[<>]' |
			grep -v '^[<>] =======$' |
			grep -v '^[<>] >>>>>>> .merge_file_' |
			grep -v '^[<>] <<<<<<< .merge_file_' |
			grep -vE "^[<>][[:space:]]+.*\.($file_types)$"
	)"
	if [ "$critical_diff" != "" ]
	then
		echo "Error: unfixable CMakeLists.txt conflict"
		color "$RED"
		echo "$critical_diff"
		color "$RESET"
		echo -e "\n\n"
		exit 1
	fi
	if [ ! -x "$(command -v tw_cmake)" ]
	then
		echo "Error: you need tw_cmake installed in your path"
		echo "       https://github.com/lib-crash/lib-teeworlds"
		exit 1
	fi
	tw_cmake .
	exit 0
fi

