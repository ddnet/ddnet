#!/usr/bin/env python3

import argparse
import multiprocessing
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

os.chdir(os.path.dirname(__file__) + "/..")


def recursive_file_list(path):
	result = []
	for dirpath, _, filenames in os.walk(path):
		result += [os.path.join(dirpath, filename) for filename in filenames]
	return result


IGNORE_FILES = []
IGNORE_DIRS = [
	"src/game/generated",
	"src/rust-bridge",
]


def filter_ignored(filenames):
	result = []
	for filename in filenames:
		real_filename = os.path.realpath(filename)
		if real_filename not in [os.path.realpath(ignore_file) for ignore_file in IGNORE_FILES] and not any(real_filename.startswith(os.path.realpath(subdir) + os.path.sep) for subdir in IGNORE_DIRS):
			result.append(filename)

	return result


def filter_cpp(filenames):
	return [filename for filename in filenames if any(filename.endswith(ext) for ext in ".c .cpp .h".split())]


def find_clang_format(version):
	for binary in (
		"clang-format",
		f"clang-format-{version}",
		f"/opt/clang-format-static/clang-format-{version}",
	):
		try:
			out = subprocess.check_output([binary, "--version"])
		except FileNotFoundError:
			continue
		if f"clang-format version {version}." in out.decode("utf-8"):
			return binary
	print(f"Found no clang-format {version}")
	sys.exit(-1)


clang_format_bin = find_clang_format(20)


def format_or_warn(filenames, dry_run):
	missing_newline = False
	for filename in filenames:
		try:
			with open(filename, "rb+" if not dry_run else "rb") as file:
				try:
					file.seek(-1, os.SEEK_END)
				except OSError:
					continue

				if file.read(1) == b"\n":
					continue

				if dry_run:
					print(f"{filename}: error: missing newline at EOF", file=sys.stderr)
					missing_newline = True
					continue

				file.write(b"\n")
		except OSError as e:
			print(f"Error accessing {filename}: {e}", file=sys.stderr)
			missing_newline = True

	args = [clang_format_bin]
	if dry_run:
		args += ["-Werror", "--dry-run"]
	else:
		args += ["-i"]
	args += filenames

	subprocess.check_call(args)
	proc = subprocess.run(args, capture_output=True)

	if proc.stdout:
		sys.stdout.buffer.write(proc.stdout)
	if proc.stderr:
		sys.stderr.buffer.write(proc.stderr)

	return missing_newline or (proc.returncode != 0)


def main():
	p = argparse.ArgumentParser(description="Check and fix style of changed files")
	p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
	p.add_argument("-j", "--jobs", type=int, default=0, help="Number of jobs")
	args = p.parse_args()

	filenames = filter_ignored(filter_cpp(recursive_file_list("src")))

	max_cpu = multiprocessing.cpu_count()
	if args.jobs > 0:
		max_cpu = args.jobs

	max_jobs = min(max_cpu, len(filenames))
	chunks = [filenames[i::max_jobs] for i in range(max_jobs)]

	with ThreadPoolExecutor(max_workers=max_jobs) as executor:
		results = list(executor.map(lambda c: format_or_warn(c, args.dry_run), chunks))

	sys.exit(1 if any(results) else 0)


if __name__ == "__main__":
	main()
