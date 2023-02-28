#include <cstdint>

struct DECOMP_SLICE
{
	uint16_t offset : 13;
	uint16_t length : 3;
};

enum
{
	NUM_DECOMP_LENGTHS = 8,
	NUM_DECOMPS = 9770,
};

extern const uint8_t decomp_lengths[NUM_DECOMP_LENGTHS];
extern const int32_t decomp_chars[NUM_DECOMPS];
extern const struct DECOMP_SLICE decomp_slices[NUM_DECOMPS];
extern const int32_t decomp_data[];
