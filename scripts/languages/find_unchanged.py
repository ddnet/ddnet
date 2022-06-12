#!/usr/bin/env python3
import os
import sys
import twlang

os.chdir(os.path.dirname(__file__) + "/../..")

if len(sys.argv) < 2:
	print("usage: python find_unchanged.py <file>")
	sys.exit()
infile = sys.argv[1]

trans = twlang.translations(infile)
for tran, (_, expr, _) in trans.items():
	if tran == expr:
		print(tran)
