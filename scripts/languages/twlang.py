import os
import re
from collections import OrderedDict

class LanguageDecodeError(Exception):
	def __init__(self, message, filename, line):
		error = "File \"{1}\", line {2}: {0}".format(message, filename, line+1)
		super().__init__(error)


# Taken from https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
cfmt = r'''\
(                                  # start of capture group 1
%                                  # literal "%"
(?:                                # first option
(?:[-+0 #]{0,5})                   # optional flags
(?:\d+|\*)?                        # width
(?:\.(?:\d+|\*))?                  # precision
(?:h|l|ll|w|I|I32|I64)?            # size
[cCdiouxXeEfgGaAnpsSZ]             # type
) |                                # OR
%%)                                # literal "%%"
'''


def decode(fileobj, elements_per_key):
	data = {}
	current_context = ""
	current_key = None
	index = -1
	for index, line in enumerate(fileobj):
		line = line.encode("utf-8").decode("utf-8-sig")
		line = line[:-1]
		if line and line[-1] == "\r":
			line = line[:-1]
		if not line or line[:1] == "#":
			current_context = ""
			continue

		if line[0] == "[":
			if line[-1] != "]":
				raise LanguageDecodeError("Invalid context string", fileobj.name, index)
			current_context = line[1:-1]
		elif line[:3] == "== ":
			if len(data[current_key]) >= 1+elements_per_key:
				raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
			if current_key:
				original = current_key[0] # pylint: disable=E1136
				translation = line[3:]
				if translation and [m.group(1) for m in re.finditer(cfmt, original, flags=re.X)] != [m.group(1) for m in re.finditer(cfmt, translation, flags=re.X)]:
					raise LanguageDecodeError("Non-matching formatting string", fileobj.name, index)
				data[current_key].extend([translation])
			else:
				raise LanguageDecodeError("Element before key given", fileobj.name, index)
		else:
			if current_key:
				if len(data[current_key]) != 1+elements_per_key:
					raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
				data[current_key].append(index)
			if line in data:
				raise LanguageDecodeError("Key defined multiple times: " + line, fileobj.name, index)
			data[(line, current_context)] = [index]
			current_key = (line, current_context)
	if len(data[current_key]) != 1+elements_per_key:
		raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
	data[current_key].append(index+1)
	return data


def check_file(path):
	with open(path) as fileobj:
		matches = re.findall(r"Localize\s*\(\s*\"([^\"]+)\"(?:\s*,\s*\"([^\"]+)\")?\s*\)", fileobj.read())
	return matches


def check_folder(path):
	englishlist = OrderedDict()
	for path2, dirs, files in os.walk(path):
		dirs.sort()
		for f in sorted(files):
			if not any(f.endswith(x) for x in ".cpp .c .h".split()):
				continue
			for sentence in check_file(os.path.join(path2, f)):
				englishlist[sentence] = None
	return englishlist


def languages():
	index = decode(open("data/languages/index.txt"), 2)
	langs = {"data/languages/"+key[0]+".txt" : [key[0]]+elements for key, elements in index.items()}
	return langs


def translations(filename):
	return decode(open(filename), 1)


def localizes():
	englishlist = list(check_folder("src"))
	return englishlist
