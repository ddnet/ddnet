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
	"src/tools/config_common.h"
]

def check_file(filename):
	if filename in EXCEPTIONS:
		return False
	error = False
	with open(filename, encoding="utf-8") as file:
		for line in file:
			if line == "// This file can be included several times.\n":
				break
			if line[0] == "/" or line[0] == "*" or line[0] == "\r" or line[0] == "\n" or line[0] == "\t":
				continue
			header_guard = "#ifndef " + ("_".join(filename.split(PATH)[1].split("/"))[:-2]).upper() + "_H"
			if line.startswith("#ifndef"):
				if line[:-1] != header_guard:
					error = True
					print(f"Wrong header guard in {filename}, is: {line[:-1]}, should be: {header_guard}")
			else:
				error = True
				print(f"Missing header guard in {filename}, should be: {header_guard}")
			break
	return error

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

if __name__ == '__main__':
	sys.exit(int(check_dir(PATH) != 0))
