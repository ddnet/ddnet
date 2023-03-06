#ifndef GAME_PRNG_H
#define GAME_PRNG_H

#include <cstdint>

class CPrng
{
public:
	// Creates an unseeded instance.
	CPrng();

	// The name of the random number generator including the current seed.
	const char *Description() const;

	// Seeds the random number generator with the given integer. The random
	// sequence obtained by calling `RandomBits()` repeatedly is guaranteed
	// to be the same for the same seed.
	void Seed(uint64_t aSeed[2]);

	// Generates 32 random bits. `Seed()` must be called before calling
	// this function.
	unsigned int RandomBits();

private:
	char m_aDescription[64];

	bool m_Seeded;
	uint64_t m_State;
	uint64_t m_Increment;
};

#endif // GAME_PRNG_H
