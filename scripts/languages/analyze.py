#!/usr/bin/env python3
# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import os
import sys
from pathlib import Path

import twlang

os.chdir(Path(__file__).resolve().parent.parent.parent)

langs = sys.argv[1:] if len(sys.argv) > 1 else twlang.languages()
local = twlang.localizes()
table = []
for lang in langs:
    trans = twlang.translations(lang)
    empty = 0
    supported = 0
    unused = 0
    for tran, (_, expr, _) in trans.items():
        if not expr:
            empty += 1
        elif tran in local:
            supported += 1
        else:
            unused += 1
    table.append([lang, len(trans), empty, len(local) - supported, unused])

table.sort(key=lambda row: row[3])
new_table = [["filename", "total", "empty", "missing", "unused"], *table]
s = [[str(e) for e in row] for row in new_table]
lens = [max(map(len, col)) for col in zip(*s, strict=True)]
fmt = "    ".join(f"{{:{x}}}" for x in lens)
t = [fmt.format(*row) for row in s]
print("\n".join(t))
