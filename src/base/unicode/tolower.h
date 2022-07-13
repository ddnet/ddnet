#include <stdint.h>

struct UPPER_LOWER
{
	int32_t upper = 0;
	int32_t lower = 0;
};

enum
{
	NUM_TOLOWER = 1433,
};

extern const struct UPPER_LOWER tolowermap[];
