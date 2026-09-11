#!/usr/bin/env python3
from pathlib import Path
import os
import re
import sys

# Set of C headers (without .h suffix)
C_HEADER_SET = {
	"assert",
	"complex",
	"ctype",
	"errno",
	"fenv",
	"float",
	"inttypes",
	"iso646",
	"limits",
	"locale",
	"math",
	"setjmp",
	"signal",
	"stdarg",
	"stdbool",
	"stddef",
	"stdint",
	"stdio",
	"stdlib",
	"string",
	"tgmath",
	"time",
	"wchar",
	"wctype",
}
C_HEADER_INCLUDE_PATTERN = re.compile(rf"#include\s+<({'|'.join(C_HEADER_SET)})\.h>")

IGNORE_FILES = [
	"src/net/net.h",
]
IGNORE_DIRS = [
	"src/engine/external",
	"src/masterping",
	"src/mastersrv",
]


def is_ignored(filename):
	real_filename = os.path.realpath(filename)
	return real_filename in [os.path.realpath(ignore_file) for ignore_file in IGNORE_FILES] or any(real_filename.startswith(os.path.realpath(subdir) + os.path.sep) for subdir in IGNORE_DIRS)


def get_cpp_header(c_header: str):
	if c_header == "complex":
		return "complex"
	return f"c{c_header}"


def check_standard_headers_file(filename: Path):
	errors = False
	with open(filename, encoding="utf-8") as f:
		content = f.read()
	# First check if the file includes any C headers for more efficiency when no C header is used
	if C_HEADER_INCLUDE_PATTERN.search(content):
		# Check each C header individually to print an error message with the appropriate replacement C++ header
		for c_header in C_HEADER_SET:
			if re.search(rf"#include\s+<{c_header}\.h>", content):
				print(f"Error: '{filename}' includes C header '{c_header}.h'. Include the C++ header '{get_cpp_header(c_header)}' instead.")
				errors += 1
	return errors


def check_standard_headers_directory(path: Path):
	errors = 0
	for file in Path.iterdir(path):
		if is_ignored(file):
			continue
		if file.is_dir():
			errors += check_standard_headers_directory(file)
		elif file.name.endswith((".cpp", ".h")):
			errors += check_standard_headers_file(file)
	return errors


def main():
	if check_standard_headers_directory(Path("src")) != 0:
		return 1
	print("Success: No standard C headers are used.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
