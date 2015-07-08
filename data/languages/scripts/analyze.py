import twlang
import sys

if len(sys.argv) > 1:
    langs = sys.argv[1:]
else:
    langs = twlang.languages()
local = twlang.localizes()
table = []
for lang in langs:
    trans = twlang.translations(lang)
    empty = 0
    supported = 0
    unused = 0
    for tran, (_, expr, _) in trans.iteritems():
        if not expr:
            empty += 1
        else:
            if tran in local:
                supported += 1
            else:
                unused += 1
    table.append([lang, len(trans), empty, len(local)-supported, unused])

table.sort(key=lambda l: l[3])
table = [["filename", "total", "empty", "missing", "unused"]] + table
s = [[str(e) for e in row] for row in table]
lens = [max(map(len, col)) for col in zip(*s)]
fmt = "    ".join("{{:{}}}".format(x) for x in lens)
t = [fmt.format(*row) for row in s]
print("\n".join(t))
