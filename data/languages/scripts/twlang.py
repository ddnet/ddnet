import os
import re


class LanguageDecodeError(Exception):
    def __init__(self, message, filename, line):
        error = "File \"{1}\", line {2}: {0}".format(message, filename, line+1)
        super(LanguageDecodeError, self).__init__(error)


def decode(fileobj, elements_per_key):
    data = {}
    current_key = None
    for index, line in enumerate(fileobj):
        line = line.decode("utf-8-sig").encode("utf-8")
        line = line[:-1]
        if line and line[-1] == "\r":
            line = line[:-1]
        if not line or line[:1] == "#":
            continue
        if line[:3] == "== ":
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
            data[line] = [index]
            current_key = line
    if len(data[current_key]) != 1+elements_per_key:
        raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
    data[current_key].append(index+1)
    return data


def check_file(path):
    with open(path) as fileobj:
        matches = re.findall("Localize\s*\(\s*\"([^\"]+)\"\s*\)", fileobj.read())
    return matches


def check_folder(path):
    files = os.listdir(path)
    englishlist = set()
    for f in files:
        newpath = os.path.join(path, f)
        if os.path.isdir(newpath):
            englishlist.update(check_folder(newpath))
        elif os.path.isfile(newpath):
            englishlist.update(check_file(newpath))
    return englishlist


def languages():
    index = decode(open("../index.txt"), 2)
    langs = {"../"+key+".txt" : [key]+elements for key, elements in index.iteritems()}
    return langs


def translations(filename):
    translations = decode(open(filename), 1)
    return translations


def localizes():
    englishlist = list(check_folder("../../../src"))
    return englishlist
