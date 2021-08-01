#!/usr/bin/env python3
# strip_gfx.py by ChillerDragon
# comments out graphic code to turn any src base into a headless client

import os
import argparse

os.chdir(os.path.dirname(__file__) + "/..")

GFX_FILES = [
	'src/engine/client/graphics_threaded.cpp'
]

def comment_gfx_file(filename, inplace):
	new_file_buffer = ''
	current_function = None
	with open(filename) as file:
		for line in file:
			# pre parser
			if line.startswith('}') and current_function:
				if current_function == 'bool':
					new_file_buffer += "\treturn false;\n"
				elif current_function == 'const char *':
					new_file_buffer += "\treturn \"\";\n"
				elif current_function == 'int':
					new_file_buffer += "\treturn 0;\n"
				current_function = None

			if current_function and not line.startswith("{"):
				new_file_buffer += "//"
				if line != "\n":
					new_file_buffer += " "
			new_file_buffer += line

			# post parser
			if line.startswith('void CGraphics_Threaded::'):
				current_function = 'void'
			elif line.startswith('bool CGraphics_Threaded::'):
				current_function = 'bool'
			elif line.startswith('const char *CGraphics_Threaded::'):
				current_function = 'const char *'
			elif line.startswith('int CGraphics_Threaded::'):
				current_function = 'int'
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
		comment_gfx_file(file, inplace = args.inplace)


if __name__ == '__main__':
	main()
