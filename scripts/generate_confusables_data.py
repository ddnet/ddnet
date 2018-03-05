#!/usr/bin/env python
# Needs UnicodeData.txt and confusables.txt in the current directory.
#
# Those can be obtained from unicode.org:
# - http://www.unicode.org/Public/security/<VERSION>/confusables.txt
# - http://www.unicode.org/Public/<VERSION>/ucd/UnicodeData.txt
#
# If executed as a script, it will generate the contents of the file
# `src/base/confusables_data.h`.

import csv

def confusables():
    with open('confusables.txt', encoding='utf-8-sig') as f:
        # Filter comments
        f = map(lambda line: line.split('#')[0], f)
        return list(csv.DictReader(f, fieldnames=['Value', 'Target', 'Category'], delimiter=';'))

UNICODEDATA_FIELDS = (
    "Value",
    "Name",
    "General_Category",
    "Canonical_Combining_Class",
    "Bidi_Class",
    "Decomposition",
    "Numeric",
    "Bidi_Mirrored",
    "Unicode_1_Name",
    "ISO_Comment",
    "Simple_Uppercase_Mapping",
    "Simple_Lowercase_Mapping",
    "Simple_Titlecase_Mapping",
)

def unicodedata():
    with open('UnicodeData.txt') as f:
        return list(csv.DictReader(f, fieldnames=UNICODEDATA_FIELDS, delimiter=';'))

def unhex(s):
    return int(s, 16)

def unhex_sequence(s):
    return [unhex(x) for x in s.split()] if '<' not in s else None

def generate_decompositions():
    ud = unicodedata()
    con = confusables()

    category = lambda x: {unhex(u["Value"]) for u in ud if u["General_Category"].startswith(x)}

    nfd = {unhex(u["Value"]): unhex_sequence(u["Decomposition"]) for u in ud}
    nfd = {k: v for k, v in nfd.items() if v}
    con = {unhex(c["Value"]): unhex_sequence(c["Target"]) for c in con}

    # C: Control
    # M: Combining
    # Z: Space
    ignore = category("C") | category("M") | category("Z")

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

    print("""\
#include <stdint.h>

struct DECOMP_SLICE
{
\tuint16_t offset : 13;
\tuint16_t length : 3;
};
""")
    print("enum")
    print("{")
    print("\tNUM_DECOMP_LENGTHS={},".format(len(len_set)))
    print("\tNUM_DECOMPS={},".format(len(decompositions)))
    print("};")
    print()

    print("static const uint8_t decomp_lengths[NUM_DECOMP_LENGTHS] = {")
    for l in len_set:
        print("\t{},".format(l))
    print("};")
    print()

    print("static const int32_t decomp_chars[NUM_DECOMPS] = {")
    for k in sorted(decompositions):
        print("\t0x{:x},".format(k))
    print("};")
    print()

    print("static const struct DECOMP_SLICE decomp_slices[NUM_DECOMPS] = {")
    for k in sorted(decompositions):
        d = decompositions[k]
        i = decomposition_set.index(tuple(d))
        l = len_set.index(len(d))
        print("\t{{{}, {}}},".format(decomposition_offsets[i], l))
    print("};")
    print()

    print("static const int32_t decomp_data[] = {")
    for d in decomposition_set:
        for c in d:
            print("\t0x{:x},".format(c))
    print("};")

if __name__ == '__main__':
    main()
