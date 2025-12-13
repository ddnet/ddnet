#!/usr/bin/env python3
# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import os
from pathlib import Path

import twlang
from copy_fix import copy_fix

os.chdir(Path(__file__).resolve().parent.parent.parent)

for lang in twlang.languages():
    content = copy_fix(lang, delete_unused=True, append_missing=True, delete_empty=False)
    with Path(lang).open("w", encoding="utf-8") as f:
        f.write(content)
