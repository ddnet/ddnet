#!/usr/bin/env python3
import os
import re
import sys

def read_all_lines(filename):
	with open(filename, 'r', encoding='utf-8') as file:
		return file.readlines()

def parse_config_variables(lines):
	pattern = r'^MACRO_CONFIG_[A-Z]+\((.*?), (.*?),.*'
	matches = {}
	for line in lines:
		match = re.match(pattern, line)
		if match:
			matches[match.group(1)] = match.group(2)
	return matches

def generate_regex(variable_code):
	return fr'(g_Config\.m_{variable_code}\b|Config\(\)->m_{variable_code}\b|m_pConfig->m_{variable_code}\b)'

def find_config_variables(config_variables):
	"""Returns the config variables which were not found."""
	# Set of variables that have yet to be found.
	variables_not_found = set(config_variables)
	# Precompile regex for every config variable (~1.6x speedup).
	regex_cache = {}
	for variable_code in variables_not_found.copy():
		regex_cache[variable_code] = re.compile(generate_regex(variable_code))
	for root, _, files in os.walk('src'):
		if not variables_not_found:
			break
		for file in files:
			if not variables_not_found:
				break
			if file.endswith(('.cpp', '.h')) and not 'external' in root:
				filepath = os.path.join(root, file)
				with open(filepath, 'r', encoding='utf-8') as f:
					content = f.read()
					# Only variables not yet found are searched in the remaining files (~3.6x speedup).
					# Copy set to remove elements while iterating, which is slightly faster than collecting
					# the elements to remove in another set and removing them after the loop.
					for variable_code in variables_not_found.copy():
						if regex_cache[variable_code].search(content):
							variables_not_found.remove(variable_code)
	return variables_not_found

def main():
	lines = read_all_lines('src/engine/shared/config_variables.h')
	config_variables = parse_config_variables(lines)
	config_variables_not_found = find_config_variables(config_variables)
	for variable_code in config_variables_not_found:
		print(f'The config variable \'{config_variables[variable_code]}\' is unused.')
	if config_variables_not_found:
		print('Error: Unused config variables found.')
		return 1
	print('Success: No unused config variables found.')
	return 0

if __name__ == '__main__':
	sys.exit(main())
