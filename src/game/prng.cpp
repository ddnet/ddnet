#include "prng.h"

#include <base/system.h>

// From https://en.wikipedia.org/w/index.php?title=Permuted_congruential_generator&oldid=901497400#Example_code.
//
// > The generator recommended for most users is PCG-XSH-RR with 64-bit state
// > and 32-bit output.

#define NAME "pcg-xsh-rr"

CPrng::CPrng() :
	m_Seeded(false)
{
}

const char *CPrng::Description() const
{
	if(!m_Seeded)
	{
		return NAME ":unseeded";
	}
	return m_aDescription;
}

static unsigned int RotateRight32(unsigned int x, int Shift)
{
	return (x >> Shift) | (x << (-Shift & 31));
}

void CPrng::Seed(uint64_t aSeed[2])
{
	m_Seeded = true;
	str_format(m_aDescription, sizeof(m_aDescription), "%s:%08x%08x:%08x%08x", NAME,
		(unsigned)(aSeed[0] >> 32), (unsigned)aSeed[0],
		(unsigned)(aSeed[1] >> 32), (unsigned)aSeed[1]);

	m_Increment = (aSeed[1] << 1) | 1;
	m_State = aSeed[0] + m_Increment;
	RandomBits();
}

unsigned int CPrng::RandomBits()
{
	dbg_assert(m_Seeded, "prng needs to be seeded before it can generate random numbers");

	uint64_t x = m_State;
	unsigned int Count = x >> 59;

	static const uint64_t s_Multiplier = 6364136223846793005u;
	m_State = x * s_Multiplier + m_Increment;
	x ^= x >> 18;
	return RotateRight32(x >> 27, Count);
}
