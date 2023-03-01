# Needs UnicodeData.txt and confusables.txt in the current directory.
#
# Those can be obtained from unicode.org:
# - http://www.unicode.org/Public/security/<VERSION>/confusables.txt
# - http://www.unicode.org/Public/<VERSION>/ucd/UnicodeData.txt
#
# If executed as a script, it will generate the contents of the files
# python3 scripts/generate_unicode_confusables_data.py header > `src/base/unicode/confusables.h`,
# python3 scripts/generate_unicode_confusables_data.py data > `src/base/unicode/confusables_data.h`.

import sys
import unicode

def generate_decompositions():
	ud = unicode.data()
	con = unicode.confusables()

	def category(x):
		return {unicode.unhex(u["Value"]) for u in ud if u["General_Category"].startswith(x)}

	# TODO: Is this correct? They changed the decompositioning format
	nfd = {unicode.unhex(u["Value"]): unicode.unhex_sequence(u["Decomposition_Type"]) for u in ud}
	nfd = {k: v for k, v in nfd.items() if v}
	con = {unicode.unhex(c["Value"]): unicode.unhex_sequence(c["Target"]) for c in con}

	# C: Control
	# M: Combining
	# Z: Space
	ignore = category("C") | category("M") | category("Z")

	con[0x006C] = [0x0069] # LATIN SMALL LETTER L -> LATIN SMALL LETTER I
	con[0x00A1] = [0x0069] # INVERTED EXCLAMATION MARK -> LATIN SMALL LETTER I
	con[0x2800] = [] # BRAILLE PATTERN BLANK
	con[0xFFFC] = [] # OBJECT REPLACEMENT CHARACTER

	interesting = ignore | set(nfd) | set(con)

	def apply(l, replacements):
		return [d for c in l for d in replacements.get(c, [c])]

	def gen(c):
		result = [c]
		while True:
			first = apply(result, nfd)
			second = apply(first, con)
			# Apply substitutions until convergence.
			if result == first and result == second:
				break
			result = second
		return [c for c in result if c not in ignore]

	return {c: gen(c) for c in interesting}

def gen_header(decompositions, len_set):
	print("""\
#include <cstdint>

struct DECOMP_SLICE
{
\tuint16_t offset : 13;
\tuint16_t length : 3;
};
""")
	print("enum")
	print("{")
	print(f"\tNUM_DECOMP_LENGTHS = {len(len_set)},")
	print(f"\tNUM_DECOMPS = {len(decompositions)},")
	print("};")
	print()

	print("extern const uint8_t decomp_lengths[NUM_DECOMP_LENGTHS];")
	print("extern const int32_t decomp_chars[NUM_DECOMPS];")
	print("extern const struct DECOMP_SLICE decomp_slices[NUM_DECOMPS];")
	print("extern const int32_t decomp_data[];")

def gen_data(decompositions, decomposition_set, decomposition_offsets, len_set):
	print("""\
#ifndef CONFUSABLES_DATA
#error "This file should only be included in `confusables.cpp`"
#endif
""")

	print("const uint8_t decomp_lengths[NUM_DECOMP_LENGTHS] = {")
	for l in len_set:
		print(f"\t{l},")
	print("};")
	print()

	print("const int32_t decomp_chars[NUM_DECOMPS] = {")
	for k in sorted(decompositions):
		print(f"\t0x{k:x},")
	print("};")
	print()

	print("const struct DECOMP_SLICE decomp_slices[NUM_DECOMPS] = {")
	for k in sorted(decompositions):
		d = decompositions[k]
		i = decomposition_set.index(tuple(d))
		l = len_set.index(len(d))
		print(f"\t{{{decomposition_offsets[i]}, {l}}},")
	print("};")
	print()

	print("const int32_t decomp_data[] = {")
	for d in decomposition_set:
		for c in d:
			print(f"\t0x{c:x},")
	print("};")

def main():
	decompositions = generate_decompositions()

	# Deduplicate
	decomposition_set = sorted(set(tuple(x) for x in decompositions.values()))
	len_set = sorted(set(len(x) for x in decomposition_set))

	if len(len_set) > 8:
		raise ValueError("Can't pack offset (13 bit) together with len (>3bit)")

	cur_offset = 0
	decomposition_offsets = []
	for d in decomposition_set:
		decomposition_offsets.append(cur_offset)
		cur_offset += len(d)

	header = "header" in sys.argv
	data = "data" in sys.argv

	if header:
		gen_header(decompositions, len_set)
	elif data:
		gen_data(decompositions, decomposition_set, decomposition_offsets, len_set)

if __name__ == '__main__':
	main()
