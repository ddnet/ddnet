#include <cstdint>

struct UPPER_LOWER
{
	int32_t upper;
	int32_t lower;
};

enum
{
	NUM_TOLOWER = 1433,
};

extern const struct UPPER_LOWER tolowermap[];
