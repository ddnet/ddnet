#!/usr/bin/env python3
import os
from copy_fix import copy_fix
import twlang

os.chdir(os.path.dirname(__file__) + "/../..")

for lang in twlang.languages():
    content = copy_fix(lang, delete_unused=True, append_missing=True, delete_empty=False)
    open(lang, "w").write(content)
