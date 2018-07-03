#!/usr/bin/env python3
import twlang
import sys

if len(sys.argv) < 2:
    print("usage: python find_unchanged.py <file>")
    sys.exit()
infile = sys.argv[1]

trans = twlang.translations(infile)
for tran, (_, expr, _) in trans.items():
    if tran == expr:
        print(tran)
