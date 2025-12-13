#!/usr/bin/env python3
# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import os
import sys
from pathlib import Path

import twlang

os.chdir(Path(__file__).resolve().parent.parent.parent)

if len(sys.argv) < 2:
    print("usage: python find_unchanged.py <file>")
    sys.exit()
infile = sys.argv[1]

trans = twlang.translations(infile)
for tran, (_, expr, _) in trans.items():
    if tran == expr:
        print(tran)
