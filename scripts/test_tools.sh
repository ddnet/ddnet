#!/bin/bash

set -euo pipefail

arg_verbose=0
errors=0

check_dep() {
	local tool="$1"
	[ -x "$(command -v "$tool")" ] && return 0

	printf 'Error: missing dependency %s\n' "$tool" 1>&2
	exit 1
}

# non posix tool not preinstalled on macOS
check_dep sha256sum

for arg in "$@"; do
	if [ "$arg" == "-h" ] || [ "$arg" == "--help" ]; then
		echo "usage: $(basename "$0") [OPTION..]"
		echo "description:"
		echo "  Runs a simple integration tests for the ddnet tools."
		echo "  Using binaries from the current build directory."
		echo "options:"
		echo "  --help|-h             show this help"
		echo "  --verbose|-v          verbose output"
		echo "  --valgrind-memcheck   use valgrind's memcheck to run server and client"
		exit 0
	elif [ "$arg" == "-v" ] || [ "$arg" == "--verbose" ]; then
		arg_verbose=1
	else
		echo "Error: unknown argument '$arg'"
		exit 1
	fi
done

get_input_file() {
	local filename="$1"
	local flags='-q'
	[ "$arg_verbose" -gt 0 ] && flags=''
	wget $flags "https://github.com/ChillerDragon/ddnet-test-tools/raw/refs/heads/master/data/$filename"
}

# @param actual
# @param expected
# @param [message]
assert_eq() {
	local actual="$1"
	local expected="$2"
	local message="${3:-}"
	if [ "$actual" = "$expected" ]; then
		printf '.'
		return 0
	fi

	printf 'assertion error! %s\n' "$message" 1>&2
	printf ' expected: %s\n' "$expected" 1>&2
	printf '      got: %s\n' "$actual" 1>&2
	errors="$((errors + 1))"
}

if [ ! -f dilate ]; then
	echo "[-] Error: tool binary 'dilate' not found"
	exit 1
fi

if ! TMP_DIR="$(mktemp -d tmp_test_tools_XXXXXXXXX)"; then
	echo "Error: failed to create tmp directory"
	exit 1
fi
TMP_DIR="$PWD/$TMP_DIR"

cleanup() {
	rm -rf "$TMP_DIR"
}

trap cleanup EXIT

cd "$TMP_DIR"

# shellcheck disable=SC2016
printf 'add_path $CURRENTDIR\n' > storage.cfg

#
# demo_extract_chat
#

get_input_file dm1.demo
if ! actual="$(../demo_extract_chat dm1.demo)"; then
	printf "%s\n" "$actual"
	echo "Error: demo_extract_chat failed with non zero exit code"
	exit 1
fi
assert_eq "$actual" '[00:00] chat: ChillerDragon: hi'

#
# dummy_map
#

mkdir -p maps
if ! output="$(../dummy_map)"; then
	printf "%s\n" "$output"
	echo "Error: dummy_map failed with non zero exit code"
	exit 1
fi

if ! actual="$(sha256sum maps/dummy3.map | cut -d' ' -f1)"; then
	printf "%s\n" "$actual"
	echo "Error: failed to get sha256sum of dummy map"
	exit 1
fi
assert_eq "$actual" e2d518b3dbda373c639e625fe8a9c581266c99a05f21baf0839e85b682df71e4

#
# uuid
#

if ! actual="$(../uuid foo | cut -d' ' -f3-)"; then
	printf "%s\n" "$actual"
	echo "Error: uuid failed with non zero exit code"
	exit 1
fi
assert_eq "$actual" 'I uuid: 09ea064a-7bb3-3557-bf2f-7ae3db95106a'

if [ "$errors" -gt 0 ]; then
	printf "\nFailed with %d errors.\n" "$errors"
	exit 1
else
	printf "\nAll testes passed.\n"
fi
