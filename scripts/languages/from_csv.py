#!/usr/bin/env python3
# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import argparse
import csv
import os
from pathlib import Path

import twlang


def ddnet_entry(source, context, target, **_) -> str:
    if context != "":
        return f"""\
[{context}]
{source}
== {target}
"""
    return f"""\
{source}
== {target}
"""


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert Weblate CSV files in the specified directory into DDNet translation files.")
    parser.add_argument("in_dir", metavar="IN_DIR", help="Directory with the Weblate CSV files")
    args = parser.parse_args()

    prefix = Path(__file__).resolve().parent.parent

    mapping = {f"{rfc3066}.csv": key for key, (_, _, _, rfc3066, _) in twlang.language_index().items() for rfc3066 in rfc3066.split(";")}

    for entry in sorted(os.scandir(args.in_dir), key=lambda e: e.name):
        with Path(entry.path).open(encoding="utf-8") as f:
            out = "\n".join([ddnet_entry(**entry) for entry in csv.DictReader(f)])
        key = mapping[entry.name]
        with Path(f"{prefix}/data/languages/{key}.txt").open("w", encoding="utf-8") as f:
            f.write(out)


if __name__ == "__main__":
    main()
