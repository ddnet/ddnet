#!/usr/bin/env python3

import csv
from glob import glob
import html
import io
import re
import sys

def parse_arguments(arguments_line, num, name):
	try:
		arguments_line = arguments_line.replace("\\n", "\\\\n")
		parsed = next(csv.reader(io.StringIO(arguments_line), skipinitialspace=True, escapechar='\\'))
		if len(parsed) != num:
			raise RuntimeError(f'Failed to parse {name} arguments: {arguments_line}, got {parsed}')
		return parsed
	except Exception as e:
		raise RuntimeError(f'Failed to parse {name} arguments: {arguments_line}') from e

def parse_commands(lines):
	parsed_commands = []
	for line in lines:
		args = re.match(r'.*Register\((.*)\);', line)
		if args is None or "CFGFLAG_" not in args[1]:
			continue
		parsed = parse_arguments(args[1], num=6, name='command')
		parsed_commands.append({'name': parsed[0], 'flags': parsed[2], 'arguments': parsed[1], 'description': parsed[5]})
	parsed_commands.sort(key=lambda command: command['name'])
	return parsed_commands

def parse_settings(lines):
	def parse_value(value):
		value = value.lstrip()
		value = value.replace("SERVERINFO_LEVEL_MIN", "0")
		value = value.replace("SERVERINFO_LEVEL_MAX", "2")
		value = value.replace("SERVER_MAX_CLIENTS", "64")
		value = value.replace("MAX_CLIENTS", "128")
		try:
			return int(value, base=16) if value.startswith(("0x", "0X")) else int(value)
		except Exception as e:
			raise RuntimeError("Failed to parse setting value: {value}") from e

	names = {}
	parsed_settings = []
	for line in lines:
		args = re.match(r'^MACRO_CONFIG_(.*?)\((.*)\)', line)
		if args is None:
			continue

		if args[1] == 'STR':
			parsed = parse_arguments(args[2], num=6, name='string setting')
			flags = parsed[4]
			setting = {'type': 'string', 'flags': flags, 'name': parsed[1], 'description': parsed[5], 'default': f'"{parsed[3]}"'}
		elif args[1] == 'INT':
			parsed = parse_arguments(args[2], num=7, name='int setting')
			flags = parsed[5]
			setting = {'type': 'int', 'flags': flags, 'name': parsed[1], 'description': parsed[6], 'default': parse_value(parsed[2]),
				'min': parse_value(parsed[3]), 'max': parse_value(parsed[4])}
		elif args[1] == 'COL':
			parsed = parse_arguments(args[2], num=5, name='color setting')
			flags = parsed[3]
			setting = {'type': 'color', 'flags': flags, 'name': parsed[1], 'description': parsed[4], 'default': parse_value(parsed[2])}
		else:
			raise RuntimeError(f'Failed to parse settings type: {args[1]}')

		# Filter duplicates because config_variables.h contains some definitions multiple times with #ifdef
		if setting["name"] in names:
			parsed_settings[names[setting["name"]]] = setting
		else:
			names[setting["name"]] = len(parsed_settings)
			parsed_settings.append(setting)

	parsed_settings.sort(key=lambda setting: setting['name'])
	return parsed_settings

def parse_tunings(lines):
	def parse_value(value):
		value = value.strip()
		try:
			if value.endswith("f / SERVER_TICK_SPEED"):
				return float(value.rstrip("f / SERVER_TICK_SPEED")) / 50.0
			return float(value.strip("f"))
		except Exception as e:
			raise RuntimeError("Failed to parse tuning value: {value}") from e

	parsed_tunings = []
	for line in lines:
		args = re.match(r'^MACRO_TUNING_PARAM\((.*)\)', line)
		if args is None:
			continue
		parsed = parse_arguments(args[1], num=4, name='tuning')
		parsed_tunings.append({'name': parsed[1], 'description': parsed[3], 'default': parse_value(parsed[2])})
	parsed_tunings.sort(key=lambda tuning: tuning['name'])
	return parsed_tunings

def export_commands(parsed_commands):
	output = ['<div style="overflow-x: auto;"><table class="settingscommands">']
	output.append('  <tr><th>Command</th><th>Arguments</th><th>Description</th></tr>')

	for command in parsed_commands:
		output.append(f'  <tr><td>{html.escape(command["name"])}</td><td>{html.escape(command["arguments"])}</td><td>{html.escape(command["description"])}</td></tr>')

	output.append('</table></div>')
	return '\n'.join(output)

def export_settings(parsed_settings):
	def export_color_value(setting):
		alpha = "CFGFLAG_COLALPHA" in setting['flags']
		if "CFGFLAG_COLLIGHT7" in setting['flags']:
			darkest_lgt = 61.0 / 255.0
		elif "CFGFLAG_COLLIGHT" in setting['flags']:
			darkest_lgt = 0.5
		else:
			darkest_lgt = 0.0
		number = setting['default']
		a = ((number >> 24) & 0xFF) / 255.0 if alpha else 1.0
		h = ((number >> 16) & 0xFF) / 255.0
		s = ((number >> 8) & 0xFF) / 255.0
		l = (number & 0xFF) / 255.0
		l = darkest_lgt + l * (1.0 - darkest_lgt)
		return f'<span class="colorpreview" style="background: hsla({h * 360.0:.3f}, {s * 100.0:.3f}%, {l * 100.0:.3f}%, {a:.3f})"></span> {number}'

	output = ['<div style="overflow-x: auto;"><table class="settingscommands">']
	output.append('  <tr><th>Setting</th><th>Description</th><th>Default</th><th>Min</th><th>Max</th></tr>')

	for setting in parsed_settings:
		line = f'  <tr><td>{html.escape(setting["name"])}</td><td>{html.escape(setting["description"])}</td>'
		if setting['type'] == 'string':
			line = line + f'<td>{html.escape(setting["default"])}</td><td></td><td></td></tr>'
		elif setting['type'] == 'int':
			line = line + f'<td>{setting["default"]}</td><td>{setting["min"]}</td><td>{setting["max"]}</td></tr>'
		elif setting['type'] == 'color':
			line = line + f'<td>{export_color_value(setting)}</td><td></td><td></td></tr>'
		else:
			raise RuntimeError(f'Unhandled setting type: {setting["type"]}')
		output.append(line)

	output.append('</table></div>')
	return '\n'.join(output)

def export_tunings(parsed_tunings):
	output = ['<div style="overflow-x: auto;"><table class="settingscommands">']
	output.append('  <tr><th>Tuning</th><th>Description</th><th>Default</th></tr>')

	for tuning in parsed_tunings:
		output.append(f'  <tr><td>{html.escape(tuning["name"])}</td><td>{html.escape(tuning["description"])}</td><td>{tuning["default"]}</td></tr>')

	output.append('</table></div>')
	return '\n'.join(output)

def export_block(title, content):
	print('<div class="block">')
	print(f'<h2 id="{title.lower().replace(" ", "-")}">{title}</h2>')
	print(content)
	print('</div>')

def read_files(pattern):
	for file in glob(pattern, recursive=True):
		with open(file, 'r', encoding='utf-8', errors='ignore') as f:
			yield from f

def export_settings_commands_table():
	print("<!-- DO NOT EDIT MANUALLY! THIS FILE IS AUTOMATICALLY GENERATED BY https://github.com/ddnet/ddnet/blob/master/scripts/export_settings_commands_table_web.py -->")

	settings = parse_settings(read_files("src/engine/shared/config_variables.h"))
	commands = parse_commands(read_files("src/**/*.cpp"))

	export_block("Map Settings",
		export_settings(
			[setting for setting in settings if "CFGFLAG_SERVER" in setting['flags'] and "CFGFLAG_GAME" in setting['flags']]
		)
		+ "\n" +
		export_commands(
			[command for command in commands if "CFGFLAG_SERVER" in command['flags'] and "CFGFLAG_GAME" in command['flags']]
		)
	)

	export_block("Server Settings",
		export_settings(
			[setting for setting in settings if "CFGFLAG_SERVER" in setting['flags']]
		)
	)

	export_block("Econ Settings",
		export_settings(
			[setting for setting in settings if "CFGFLAG_ECON" in setting['flags']]
		)
	)

	export_block("Server Commands",
		export_commands(
			[command for command in commands if "CFGFLAG_SERVER" in command['flags'] and "CFGFLAG_CHAT" not in command['flags']]
		)
	)

	export_block("Chat Commands",
		export_commands(
			[command for command in commands if "CFGFLAG_SERVER" in command['flags'] and "CFGFLAG_CHAT" in command['flags']]
		)
	)

	export_block("Client Settings",
		export_settings(
			[setting for setting in settings if "CFGFLAG_CLIENT" in setting['flags']]
		)
	)

	export_block("Client Commands",
		export_commands(
			[command for command in commands if "CFGFLAG_CLIENT" in command['flags']]
		)
	)

	export_block("Tunings",
		export_tunings(
			parse_tunings(
				read_files("src/game/tuning.h")
			)
		)
	)

if __name__ == "__main__":
	# Fix UTF-8 encoding of output stream when redirecting to file
	sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
	sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
	export_settings_commands_table()
