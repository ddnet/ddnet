import functools
import os
import re
from collections import OrderedDict


class LanguageDecodeError(Exception):
	def __init__(self, message, filename, line):
		error = f'File "{filename}", line {line + 1}: {message}'
		super().__init__(error)


def decode(fileobj, elements_per_key, allow_context=True):
	data = {}
	current_context = ""
	current_key = None
	index = -1
	for index, line in enumerate(fileobj):
		line = line.encode("utf-8").decode("utf-8-sig").rstrip("\r\n")
		if not line or line[:1] == "#":
			current_context = ""
			continue

		if line[0] == "[":
			if line[-1] != "]":
				raise LanguageDecodeError("Invalid context string", fileobj.name, index)
			if not allow_context:
				raise LanguageDecodeError("Context not allowed in this file", fileobj.name, index)
			current_context = line[1:-1]
		elif line[:3] == "== ":
			if len(data[current_key]) >= 1 + elements_per_key:
				raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
			if current_key:
				translation = line[3:]
				data[current_key].extend([translation])
			else:
				raise LanguageDecodeError("Element before key given", fileobj.name, index)
		else:
			if current_key:
				if len(data[current_key]) != 1 + elements_per_key:
					raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
				data[current_key].append(index - 1 if current_context else index)
			if line in data:
				raise LanguageDecodeError("Key defined multiple times: " + line, fileobj.name, index)
			data[(line, current_context)] = [index - 1 if current_context else index]
			current_key = (line, current_context)
	if len(data[current_key]) != 1 + elements_per_key:
		raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
	data[current_key].append(index + 1)
	new_data = {}
	for key, value in data.items():
		if key[0]:
			new_data[key] = value
	return new_data


def check_file(path):
	with open(path, encoding="utf-8") as fileobj:
		matches = re.findall(r"(Localize|Localizable)\s*\(\s*\"((?:(?:\\\")|[^\"])+)\"(?:\s*,\s*\"((?:(?:\\\")|[^\"])+)\")?\s*\)", fileobj.read())
	return matches


@functools.lru_cache(None)
def check_folder(path):
	englishlist = OrderedDict()
	for path2, dirs, files in os.walk(path):
		dirs.sort()
		for f in sorted(files):
			if not any(f.endswith(x) for x in [".cpp", ".c", ".h"]):
				continue
			for sentence in check_file(os.path.join(path2, f)):
				key = (sentence[1:][0].replace('\\"', '"'), sentence[1:][1].replace('\\"', '"'))
				englishlist[key] = None
	return englishlist


def language_index():
	with open("data/languages/index.txt", encoding="utf-8") as f:
		return {key: value for (key, _), value in decode(f, 3, allow_context=False).items()}


def languages():
	index = language_index()
	langs = {"data/languages/" + key + ".txt": [key] + elements for key, elements in index.items()}
	return langs


def translations(filename):
	with open(filename, encoding="utf-8") as f:
		return decode(f, 1)


def localizes():
	english_list = list(check_folder("src"))
	return english_list
