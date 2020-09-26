#!/usr/bin/env python3

from collections import defaultdict
import os
import re
import subprocess
import sys

os.chdir(os.path.dirname(__file__) + "/..")

ignore_files = ["src/engine/keys.h", "src/engine/client/keynames.h"]

def recursive_file_list(path):
	result = []
	for dirpath, dirnames, filenames in os.walk(path):
		result += filter(lambda p: p not in ignore_files, [os.path.join(dirpath, filename) for filename in filenames])
	return result

def filter_cpp(filenames):
	return [filename for filename in filenames
		if any(filename.endswith(ext) for ext in ".c .cpp .h".split())]

def reformat(filenames):
	subprocess.check_call(["clang-format", "-i"] + filenames)

def warn(filenames):
	return subprocess.call(["clang-format", "-Werror", "--dry-run"] + filenames)

def main():
	import argparse
	p = argparse.ArgumentParser(description="Check and fix style of changed files")
	p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
	args = p.parse_args()
	filenames = filter_cpp(recursive_file_list("src"))
	if not args.dry_run:
		reformat(filenames)
	else:
		sys.exit(warn(filenames))

if __name__ == "__main__":
	main()
