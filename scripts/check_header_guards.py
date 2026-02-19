#!/usr/bin/env python3
import os
import sys

os.chdir(os.path.dirname(__file__) + "/..")

PATH = "src/"
EXCEPTIONS = [
	"src/base/unicode/confusables.h",
	"src/base/unicode/confusables_data.h",
	"src/base/unicode/tolower.h",
	"src/base/unicode/tolower_data.h",
	"src/tools/config_common.h",
]

def read_file(filename):
	with open(filename, encoding="utf-8") as file:
		return file.read()

def check_file(filename):
	if filename in EXCEPTIONS:
		return False
	guard_name = ("_".join(filename.split(PATH)[1].split("/"))[:-2]).upper() + "_H"
	header_guard_line1 = f"#ifndef {guard_name}"
	header_guard_line2 = f"#define {guard_name}"
	footer1 = f"#endif // {guard_name}"
	footer2 = "#endif"

	lines = read_file(filename).splitlines()

	# check header
	for i, line in enumerate(lines):
		if line == "// This file can be included several times.":
			# file does not need header/footer
			return False
		if line.startswith("//") or line.startswith("/*") or line.startswith("*/") or line.startswith("\t") or line == "":
			continue
		if line.startswith("#ifndef"):
			if line != header_guard_line1:
				print(f"Wrong header guard in {filename}, is: {line}, should be: {header_guard_line1}")
				return True
			next_line = lines[i + 1] if i + 1 < len(lines) else None
			if next_line != header_guard_line2:
				print(f"Wrong header guard in {filename}, is: {next_line}, should be: {header_guard_line2}")
				return True
			break
		else:
			print(f"Missing header guard in {filename}, should be: {header_guard_line1}")
			return True
	else: # executed if the loop wasn't broken out of
		print(f"Missing header guard in {filename}, file is empty?")
		return True

	# check footer
	if lines[-1] != footer1 and lines[-1] != footer2:
		print(f"Wrong footer in {filename}, is: {lines[-1]}, should be: {footer1}")
		return True

	return False


def check_dir(directory):
	errors = 0
	file_list = os.listdir(directory)
	for file in file_list:
		path = directory + file
		if os.path.isdir(path):
			if file not in ("external", "generated", "rust-bridge"):
				errors += check_dir(path + "/")
		elif file.endswith(".h") and file != "keynames.h":
			errors += check_file(path)
	return errors


if __name__ == "__main__":
	sys.exit(int(check_dir(PATH) != 0))
