import os
import re
from collections import OrderedDict

class LanguageDecodeError(Exception):
    def __init__(self, message, filename, line):
        error = "File \"{1}\", line {2}: {0}".format(message, filename, line+1)
        super(LanguageDecodeError, self).__init__(error)


def decode(fileobj, elements_per_key):
    data = {}
    current_context = ""
    current_key = None
    for index, line in enumerate(fileobj):
        line = line.encode("utf-8").decode("utf-8-sig")
        line = line[:-1]
        context = ""
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
                data[current_key].extend([line[3:]])
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
        matches = re.findall("Localize\s*\(\s*\"([^\"]+)\"(?:\s*,\s*\"([^\"]+)\")?\s*\)", fileobj.read())
    return matches


def check_folder(path):
    englishlist = OrderedDict()
    for path, dirs, files in os.walk(path):
        dirs.sort()
        for f in sorted(files):
            if not any(f.endswith(x) for x in ".cpp .c .h".split()):
                continue
            for sentence in check_file(os.path.join(path, f)):
                englishlist[sentence] = None
    return englishlist


def languages():
    index = decode(open("data/languages/index.txt"), 2)
    langs = {"data/languages/"+key+".txt" : [key]+elements for key, elements in index.items()}
    return langs


def translations(filename):
    translations = decode(open(filename), 1)
    return translations


def localizes():
    englishlist = list(check_folder("src"))
    return englishlist
