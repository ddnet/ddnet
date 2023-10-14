#!/usr/bin/env python3
import os
import sys

def find_unused_header_files(directory):
	header_files = set()
	used_files = set()

	for root, _, files in os.walk(directory):
		for file in files:
			if file.endswith('.h'):
				header_files.add(file)

	for root, _, files in os.walk(directory):
		for file in files:
			with open(os.path.join(root, file), 'r', encoding="utf-8") as f:
				content = f.read()
				for header in header_files:
					if header in content:
						used_files.add(header)

	return header_files - used_files

def main():
	directory = 'src'
	unused_header_files = find_unused_header_files(directory)

	if unused_header_files:
		for file in unused_header_files:
			print(f"Error: Header file '{file}' is unused.")
		return 1
	print("Success: No header files are unused.")
	return 0

if __name__ == "__main__":
	sys.exit(main())
