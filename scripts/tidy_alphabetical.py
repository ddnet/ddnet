#!/usr/bin/env python3

# A tool inspired by https://doc.rust-lang.org/nightly/nightly-rustc/tidy/alphabetical/index.html.

# TODO: for whatever reason, `InterpreterPoolExecutor` doesn't work:
# Exception: concurrent.interpreters.NotShareableError: [TidyAlphabeticalError('./CMakeLists.txt', 880, '…')] does not support cross-interpreter data
# from concurrent.futures import InterpreterPoolExecutor as Executor
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor as Executor
import argparse
import os
import sys

os.chdir(os.path.dirname(__file__) + "/..")

START_MARKER = "tidy-alphabetical-start"
END_MARKER = "tidy-alphabetical-end"


class Fix:
	def __init__(self, start, end, replacement):
		self.start = start
		self.end = end
		self.replacement = replacement


class TidyAlphabeticalError(Exception):
	def __init__(self, filename, line, message, fix=None):
		self.filename = filename
		self.line = line
		self.message = message
		self.fix = fix

	def __str__(self):
		return f"{self.filename}:{self.line}: {self.message}"


def sort_lines(lines):
	result = _sort_lines_impl(lines)
	if len(result) != len(lines):
		raise RuntimeError("result must have the same length as start")
	return result


def _sort_lines_impl(lines):
	if len(lines) == 0:
		return lines
	if lines[-1] != "\n":
		raise RuntimeError("must have terminating newline")
	return "".join(sorted(lines.splitlines(keepends=True)))


def check_file(filename, contents):
	# start searching at index 0
	prev_start = -1
	prev_end = -1
	errors = []

	def line(index):
		return 1 + contents.count("\n", 0, index)

	def error(index, message, fix=None):
		errors.append(TidyAlphabeticalError(filename, line(index), message, fix=fix))

	while True:
		start = contents.find(START_MARKER, prev_start + 1)
		end = contents.find(END_MARKER, prev_end + 1)
		if start == -1 and end == -1:
			# done, no more markers
			return errors
		if end == -1:
			error(start, f"found `{START_MARKER}` without matching `{END_MARKER}`")
			return errors
		if start == -1 or end < start:
			error(end, f"found `{END_MARKER}`, expected `{START_MARKER}`")
			return errors
		if start < prev_end:
			error(start, f"found `{START_MARKER}`, expected `{END_MARKER}`")
			return errors
		start_lines = contents.find("\n", start, end)
		end_lines = contents.rfind("\n", start, end)
		if start_lines == -1 or end_lines == -1:
			error(start, f"found `{START_MARKER}` and `{END_MARKER}` on the same line")

		start_lines += 1
		end_lines += 1
		# normal path
		lines = contents[start_lines:end_lines]
		sorted_lines = sort_lines(lines)
		if sorted_lines != lines:
			# error
			for i, (a, b) in enumerate(zip(sorted_lines, lines)):
				if a != b:
					index = i
					break
			else:  # runs when the for is not broken out of
				raise RuntimeError("couldn't find difference")
			error(start_lines + index, f"lines not sorted between `{START_MARKER}` and `{END_MARKER}`", fix=Fix(start_lines, end_lines, sorted_lines))
		prev_start = start
		prev_end = end


def read_and_check_file(filename):
	return check_file(filename, read_file(filename))


def read_file(filename):
	with open(filename, encoding="utf8") as f:
		return f.read()


def write_file(filename, contents):
	with open(filename, "w", encoding="utf8") as f:
		f.write(contents)


def apply_fixes(contents, fixes):
	result = []
	prev_end = 0
	for fix in fixes:
		result.append(contents[prev_end : fix.start])
		result.append(fix.replacement)
		prev_end = fix.end
	result.append(contents[prev_end:])
	return "".join(result)


def apply_fixes_to_file(filename, fixes):
	write_file(filename, apply_fixes(read_file(filename), fixes))


def recursive_file_list(path, toplevel_dirs):
	result = []
	toplevel = True
	for dirpath, dirnames, filenames in os.walk(path):
		if toplevel:
			dirnames[:] = toplevel_dirs
			toplevel = False
		result += [os.path.join(dirpath, filename) for filename in filenames]
	return result


TEXT_EXTS = set(
	"""tidy-alphabetical-start
.adoc
.bat
.c
.cfg
.cmake
.code-workspace
.cpp
.css
.csv
.desktop
.frag
.gradle
.h
.html
.in
.java
.js
.json
.md
.mm
.pro
.properties
.ps1
.py
.rc
.rs
.rules
.sh
.supp
.svg
.toml
.toolchain
.txt
.vert
.vim
.xml
.yml
""".splitlines()[1:]  # tidy-alphabetical-end
)

TOPLEVEL_DIRS = """tidy-alphabetical-start
.github
cmake
data
datsrc
docs
man
other
scripts
src
""".splitlines()[1:]  # tidy-alphabetical-end


def filter_text(filenames):
	def _is_text(filename):
		base, ext = os.path.splitext(os.path.basename(filename))
		return ext in TEXT_EXTS or (ext == "" and base.startswith("."))

	return [filename for filename in filenames if _is_text(filename)]


def main():
	parser = argparse.ArgumentParser(description="Check and fix alphabetically sorted lines in files")
	# tidy-alphabetical-start
	parser.add_argument("-j", "--jobs", type=int, default=0, help="Number of jobs")
	parser.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
	# tidy-alphabetical-end

	args = parser.parse_args()

	files = filter_text(recursive_file_list(".", TOPLEVEL_DIRS))
	files = [file[2:] for file in files]  # strip `./` prefix
	with Executor(max_workers=args.jobs if args.jobs != 0 else None) as executor:
		errors = [error for errors in executor.map(read_and_check_file, files) for error in errors]

	if not args.dry_run:
		fixes = defaultdict(list)
	found_errors = False
	for error in errors:
		if not args.dry_run and error.fix is not None:
			fixes[error.filename].append(error.fix)
		else:
			found_errors = True
			print(f"{error}", file=sys.stderr)
	if not args.dry_run:
		for filename in fixes:
			apply_fixes_to_file(filename, fixes[filename])

	return found_errors


if __name__ == "__main__":
	sys.exit(main())
