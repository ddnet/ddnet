#include "random_hash.h"

// MurMur3 32 bit finalizer mixer
// Has known good bias and avalanche
// Marginally better 32 bit mixers exist, but this one is tested, and good enough
static uint32_t murmur3mix32(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x85ebca6bu;
	x ^= x >> 13;
	x *= 0xc2b2ae35u;
	x ^= x >> 16;
	return x;
}

static void HashAppend(uint32_t &State, int32_t Value)
{
	uint32_t x = static_cast<uint32_t>(Value);
	State = murmur3mix32(State ^ x);
}

namespace RandomHash
{
	uint32_t HashInts(std::initializer_list<int> Values)
	{
		uint32_t State = 0x1234567u;
		for(int x : Values)
			HashAppend(State, static_cast<int32_t>(x));

		return murmur3mix32(State);
	}

	int SeededRandomIntBelow(int Below, std::initializer_list<int> Values)
	{
		if(Below <= 1)
			return 0;

		uint32_t Hash = HashInts(Values);
		return static_cast<int>(Hash % static_cast<uint32_t>(Below));
	}
} // namespace RandomHash
