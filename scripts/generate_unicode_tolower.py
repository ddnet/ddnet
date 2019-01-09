# Needs UnicodeData.txt in the current directory.
#
# It can be obtained from unicode.org:
# - http://www.unicode.org/Public/<VERSION>/ucd/UnicodeData.txt
#
# If executed as a script, it will generate the contents of the file
# `src/base/unicode/tolower_data.h`.

import unicode

def generate_cases():
    ud = unicode.data()
    return [(unicode.unhex(u["Value"]), unicode.unhex(u["Simple_Lowercase_Mapping"])) for u in ud if u["Simple_Lowercase_Mapping"]]

def main():
    cases = generate_cases()

    print("""\
#include <stdint.h>

struct UPPER_LOWER
{{
\tint32_t upper;
\tint32_t lower;
}};

enum
{{
\tNUM_TOLOWER={},
}};

static const struct UPPER_LOWER tolower[NUM_TOLOWER] = {{""".format(len(cases)))
    for upper_code, lower_code in cases:
        print("\t{{{}, {}}},".format(upper_code, lower_code))
    print("};")

if __name__ == '__main__':
    main()
