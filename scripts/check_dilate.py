#!/usr/bin/env python3

import os
import subprocess
import sys

def check_file(dilate_path, filename):
	executable_path = os.path.join(dilate_path, "dilate")
	try:
		with subprocess.Popen(
			[executable_path, "--dry-run", filename],
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE
		) as process:
			process.communicate()
			not_dilated = process.returncode != 0
			if not_dilated:
				print(f"Error: '{filename}' is not dilated.")
			return not_dilated
	except FileNotFoundError:
		print(f"Error: Executable not found at '{executable_path}'.")
		return 1

def check_dir(dilate_path, directory):
	errors = 0
	for file in os.listdir(directory):
		path = os.path.join(directory, file)
		if os.path.isdir(path):
			errors += check_dir(dilate_path, path)
		elif file.endswith(".png"):
			errors += check_file(dilate_path, path)
	return errors

def main(arguments):
	if len(arguments) != 3:
		print(f"Usage: {arguments[0]} <dilate executable folder> <check folder>")
		return 1

	dilate_path = arguments[1]
	check_path = arguments[2]

	errors = check_dir(dilate_path, check_path)
	if errors > 0:
		return 1

	print(f"Success: All .png files in '{check_path}' are dilated.")
	return 0

if __name__ == "__main__":
	sys.exit(main(sys.argv))
