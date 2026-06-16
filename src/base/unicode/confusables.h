#include <cstddef>
#include <cstdint>

struct DECOMP_SLICE
{
	uint16_t offset : 13;
	uint16_t length : 3;
};

static constexpr size_t NUM_DECOMP_LENGTHS = 8;
static constexpr size_t NUM_DECOMPS = 10093;

extern const uint8_t decomp_lengths[NUM_DECOMP_LENGTHS];
extern const int32_t decomp_chars[NUM_DECOMPS];
extern const struct DECOMP_SLICE decomp_slices[NUM_DECOMPS];
extern const int32_t decomp_data[];
