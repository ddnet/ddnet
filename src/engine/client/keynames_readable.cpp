/* AUTO GENERATED! DO NOT EDIT MANUALLY! See scripts/gen_keys.py */

#include "keynames_readable.h"

#include "keyboard.h"

/**
 * Do not use directly! Use the @link KeyNameHumanReadable @endlink function.
 */
char (*KeyNamesHumanReadable())[20]
{
	static char s_aaGeneratedKeyNames[512][20];
	static bool s_Generated = [] {
		for(int i = 0; i < 512; ++i)
			GenerateKeyNameHumanReadable(s_aaGeneratedKeyNames[i], i);
		return true;
	}();
	(void)s_Generated;
	return s_aaGeneratedKeyNames;
}
