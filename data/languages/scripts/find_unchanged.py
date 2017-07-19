#!/usr/bin/env python2
import twlang
import sys

if len(sys.argv) < 2:
    print("usage: python find_unchanged.py <file>")
    sys.exit()
infile = sys.argv[1]

trans = twlang.translations(infile)
for tran, (_, expr, _) in trans.iteritems():
    if tran == expr:
        print(tran)
