#!/usr/bin/env python3

from collections import defaultdict
import re
import subprocess
import sys

def git_get_changed_files(margin, base):
	return subprocess.check_output([
		"git",
		"diff",
		base,
		"--name-only",
	]).decode().splitlines()

def filter_cpp(changed_files):
	return [filename for filename in changed_files
		if any(filename.endswith(ext) for ext in ".c .cpp .h".split())]

def reformat(changed_files):
	for filename in changed_files:
		subprocess.check_call(["clang-format", "-i", filename])

def warn(changed_files):
	result = 0
	for filename in changed_files:
		result = subprocess.call(["clang-format", "-Werror", "--dry-run", filename]) or result
	return result

def get_common_base(base):
	return subprocess.check_output(["git", "merge-base", "HEAD", base]).decode().strip()

def main():
	import argparse
	p = argparse.ArgumentParser(description="Check and fix style of changed files")
	p.add_argument("--base", default="HEAD", help="Revision to compare to")
	p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
	args = p.parse_args()
	if not args.dry_run:
		reformat(filter_cpp(git_get_changed_files(1, base=args.base)))
	else:
		sys.exit(warn(filter_cpp(git_get_changed_files(1, base=args.base))))

if __name__ == "__main__":
	main()
