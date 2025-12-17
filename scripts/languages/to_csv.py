#!/usr/bin/env python3
import argparse
import csv
import os
import twlang


def main():
	parser = argparse.ArgumentParser(description="Convert DDNet translation files into Weblate CSV files in the specified directory.")
	parser.add_argument("out_dir", metavar="OUT_DIR", help="Directory to output the Weblate CSV files in")
	args = parser.parse_args()

	prefix = os.path.dirname(__file__) + "/../.."

	for key, (_, _, _, rfc3066, _) in sorted(twlang.language_index().items()):
		rfc3066 = rfc3066.split(";")[0]
		filename = f"{prefix}/data/languages/{key}.txt"
		with open(f"{args.out_dir}/{rfc3066}.csv", "w", encoding="utf-8", newline="\r\n") as f:
			# Needs to use the "unix" dialect, otherwise Weblate misdetects
			# space as field separator in some of the translation files.
			writer = csv.DictWriter(f, "source context target".split(), dialect="unix")
			writer.writeheader()
			for (source, context), (_, translation, _) in twlang.translations(filename).items():
				writer.writerow({
					"source": source,
					"context": context,
					"target": translation,
				})


if __name__ == "__main__":
	main()
