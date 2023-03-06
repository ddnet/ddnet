#include <cstdlib>

#include "tolower.h"

static int compul(const void *a, const void *b)
{
	struct UPPER_LOWER *ul_a = (struct UPPER_LOWER *)a;
	struct UPPER_LOWER *ul_b = (struct UPPER_LOWER *)b;
	return ul_a->upper - ul_b->upper;
}

int str_utf8_tolower(int code)
{
	struct UPPER_LOWER key;
	struct UPPER_LOWER *res;
	key.upper = code;
	res = (UPPER_LOWER *)bsearch(&key, tolowermap, NUM_TOLOWER, sizeof(struct UPPER_LOWER), compul);

	if(res == NULL)
		return code;
	return res->lower;
}

#define TOLOWER_DATA
#include "tolower_data.h"
#undef TOLOWER_DATA
