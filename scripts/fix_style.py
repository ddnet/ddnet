#!/usr/bin/env python3

from collections import defaultdict
import re
import subprocess
import sys

RE_POSITION=re.compile(rb"^@@ -[0-9]+,[0-9]+ \+(?P<start>[0-9]+),(?P<len>[0-9]+) @@")
def get_line_ranges(git_diff):
	new_filename = None
	result = defaultdict(list)
	for line in git_diff.splitlines():
		if line.startswith(b"+++ "):
			new_filename = line[6:].decode()
			continue
		match = RE_POSITION.match(line)
		if match:
			start = int(match.group("start").decode())
			end = start + int(match.group("len").decode()) - 1
			result[new_filename].append((start, end))
			continue
	return dict(result)

def git_get_changed_lines(margin, base):
	return get_line_ranges(subprocess.check_output([
		"git",
		"diff",
		base,
		"--unified={}".format(margin),
	]))

def filter_cpp(changed_lines):
	return {filename: changed_lines[filename] for filename in changed_lines
		if any(filename.endswith(ext) for ext in ".c .cpp .h".split())}

def reformat(changed_lines):
	for filename in changed_lines:
		subprocess.check_call(["clang-format", "-i"] + ["--lines={}:{}".format(start, end) for start, end in changed_lines[filename]] + [filename])

def warn(changed_lines):
	result = 0
	for filename in changed_lines:
		result = subprocess.call(["clang-format", "-Werror", "--dry-run"] + ["--lines={}:{}".format(start, end) for start, end in changed_lines[filename]] + [filename]) or result
	return result

def get_common_base(base):
	return subprocess.check_output(["git", "merge-base", "HEAD", base]).decode().strip()

def main():
	import argparse
	p = argparse.ArgumentParser(description="Check and fix style of changed lines")
	p.add_argument("--base", default="HEAD", help="Revision to compare to")
	p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
	args = p.parse_args()
	if not args.dry_run:
		last_ranges = None
		ranges = filter_cpp(git_get_changed_lines(1, base=args.base))
		i = 0
		while last_ranges != ranges:
			print("Iteration {}".format(i))
			reformat(ranges)
			last_ranges = ranges
			ranges = filter_cpp(git_get_changed_lines(1, base=args.base))
			i += 1
		print("Done after {} iterations".format(i))
	else:
		sys.exit(warn(filter_cpp(git_get_changed_lines(1, base=args.base))))

if __name__ == "__main__":
	main()
