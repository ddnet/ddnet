#!/usr/bin/env python3
import argparse
import csv
import os
import twlang


def ddnet_entry(source, context, target, **_kwargs):
	if context != "":
		return f"""\
[{context}]
{source}
== {target}
"""
	else:
		return f"""\
{source}
== {target}
"""


def main():
	parser = argparse.ArgumentParser(description="Convert Weblate CSV files in the specified directory into DDNet translation files.")
	parser.add_argument("in_dir", metavar="IN_DIR", help="Directory with the Weblate CSV files")
	args = parser.parse_args()

	prefix = os.path.dirname(__file__) + "/../.."

	mapping = {f"{rfc3066}.csv": key for key, (_, _, _, rfc3066, _) in twlang.language_index().items() for rfc3066 in rfc3066.split(";")}

	for entry in sorted(os.scandir(args.in_dir), key=lambda e: e.name):
		with open(entry.path, encoding="utf-8") as f:
			out = "\n".join([ddnet_entry(**entry) for entry in csv.DictReader(f)])
		key = mapping[entry.name]
		with open(f"{prefix}/data/languages/{key}.txt", "w", encoding="utf-8") as f:
			f.write(out)


if __name__ == "__main__":
	main()
