# Needs UnicodeData.txt in the current directory.
#
# It can be obtained from unicode.org:
# - http://www.unicode.org/Public/<VERSION>/ucd/UnicodeData.txt
#
# If executed as a script, it will generate the contents of the file
# python3 scripts/generate_unicode_tolower.py header > `src/base/unicode/tolower.h`,
# python3 scripts/generate_unicode_tolower.py data > `src/base/unicode/tolower_data.h`.

import sys
import unicode

def generate_cases():
	ud = unicode.data()
	return [(unicode.unhex(u["Value"]), unicode.unhex(u["Simple_Lowercase_Mapping"])) for u in ud if u["Simple_Lowercase_Mapping"]]

def gen_header(cases):
	print(f"""\
#include <cstdint>

struct UPPER_LOWER
{{
\tint32_t upper;
\tint32_t lower;
}};

enum
{{
\tNUM_TOLOWER = {len(cases)},
}};

extern const struct UPPER_LOWER tolowermap[];""")

def gen_data(cases):
	print("""\
#ifndef TOLOWER_DATA
#error "This file must only be included in `tolower.cpp`"
#endif

const struct UPPER_LOWER tolowermap[] = {""")
	for upper_code, lower_code in cases:
		print(f"\t{{{upper_code}, {lower_code}}},")
	print("};")

def main():
	cases = generate_cases()

	header = "header" in sys.argv
	data = "data" in sys.argv

	if header:
		gen_header(cases)
	elif data:
		gen_data(cases)

if __name__ == '__main__':
	main()
