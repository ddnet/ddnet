#!/usr/bin/env python3
import csv
import os
import twlang

def main():
	for key, (_, _, _, rfc3066, _) in twlang.language_index().items():
		rfc3066 = rfc3066.split(";")[0]
		filename = f"data/languages/{key}.txt"
		with open(f"data/languages_csv/{rfc3066}.csv", "w") as f:
			writer = csv.DictWriter(f, "source context target".split())
			writer.writeheader()
			for (source, context), (_, translation, _) in twlang.translations(filename).items():
				writer.writerow(dict(
					source=source,
					context=context,
					target=translation,
				))

if __name__ == "__main__":
	os.chdir(os.path.dirname(__file__) + "/../..")
	main()
