#ifndef GAME_SERVER_PRNG_H
#define GAME_SERVER_PRNG_H

class CPrng
{
public:
	// Creates an unseeded instance.
	CPrng();

	// The name of the random number generator including the current seed.
	const char *Description() const;

	// Seeds the random number generator with the given integer. The random
	// sequence obtained by calling `RandomInt()` repeatedly is guaranteed
	// to be the same for the same seed.
	void Seed(unsigned int Seed);

	// Generates a random integer between 0 and 2**31 - 1. `Seed()` must be
	// called before calling this function.
	int RandomInt();

private:
	char m_aDescription[32];

	bool m_Seeded;
	unsigned int m_aState[32];
	int m_Index;
};

#endif // GAME_SERVER_PRNG_H
