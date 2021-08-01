#!/usr/bin/env python3
# strip_gfx.py by ChillerDragon
# comments out graphic code to turn any src base into a headless client

import os
import argparse

os.chdir(os.path.dirname(__file__) + "/..")

GFX_FILES = [
	['src/engine/client/graphics_threaded.cpp', 'CGraphics_Threaded']
]

def comment_gfx_file(filename, class_name, inplace):
	new_file_buffer = ''
	current_function = None
	reached_body = False
	is_pointer = False
	with open(filename) as file:
		for line in file:
			# pre parser
			if line.startswith('}') and current_function:
				if current_function == 'bool':
					new_file_buffer += "\treturn false;\n"
				elif current_function == 'const char':
					new_file_buffer += "\treturn \"\";\n"
				elif current_function == 'int':
					if is_pointer:
						new_file_buffer += "\tstatic int ret = 0;return &ret;\n"
					else:
						new_file_buffer += "\treturn 0;\n"
				current_function = None

			if current_function and reached_body:
				new_file_buffer += "//"
				if line != "\n":
					new_file_buffer += " "
			new_file_buffer += line

			# post parser
			if current_function and not reached_body:
				if line.startswith('{'):
					reached_body = True
			for func_type in ('void', 'bool', 'const char', 'int'):
				if line.startswith(f"{func_type} {class_name}::"):
					is_pointer = False
					current_function = func_type
					reached_body = False
				elif line.startswith(f"{func_type} *{class_name}::"):
					is_pointer = True
					current_function = func_type
					reached_body = False
	if inplace:
		f = open(filename, 'w')
		f.write(new_file_buffer)
		f.close()
	else:
		print(new_file_buffer)
	return current_function is None

def main():
	p = argparse.ArgumentParser(description='Comment out the graphic code to get a headless client')
	p.add_argument('-i', '--inplace', action='store_true', help='Edit files in place instead of printing to stdout')
	args = p.parse_args()
	for file in GFX_FILES:
		comment_gfx_file(file[0], file[1], inplace = args.inplace)


if __name__ == '__main__':
	main()
