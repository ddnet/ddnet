#!/usr/bin/env python3
# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import os
import re
import sys
from pathlib import Path

import twlang

os.chdir(Path(__file__).resolve().parent.parent.parent)

# Taken from https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
cfmt = """
(                                  # start of capture group 1
%                                  # literal "%"
(?:                                # first option
(?:[-+0 #]{0,5})                   # optional flags
(?:\\d+|\\*)?                      # width
(?:\\.(?:\\d+|\\*))?               # precision
(?:h|l|ll|w|I|I32|I64)?            # size
[cCdiouxXeEfgGaAnpsSZ]             # type
) |                                # OR
%%)                                # literal "%%"
"""

total_errors = 0


def print_validation_error(error, filename, error_line) -> None:
    print(f"Invalid: {translated}")
    print(f"- {error} in {filename}:{error_line + 1}\n")


languages = sys.argv[1:] if len(sys.argv) > 1 else twlang.languages()
local = twlang.localizes()

for language in languages:
    translations = twlang.translations(language)

    for (english, _), (line, translated, _) in translations.items():
        if not translated:
            continue

        # Validate c format strings. Strings that move the formatters are not validated.
        english_fmt = re.findall(cfmt, english, flags=re.VERBOSE)
        translated_fmt = re.findall(cfmt, translated, flags=re.VERBOSE)
        if english_fmt != translated_fmt and "1$" not in translated:
            print_validation_error("Non-matching formatting", language, line)
            total_errors += 1

        # Check for elipisis
        if "…" in english and "..." in translated:
            print_validation_error("Usage of ... instead of the … character", language, line)
            total_errors += 1

if total_errors:
    print(f"Found {total_errors} {'error' if total_errors == 1 else 'errors'} ")
    sys.exit(1)
