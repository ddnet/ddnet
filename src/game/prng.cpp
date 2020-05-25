#include "prng.h"

#include <base/system.h>

// From https://www.mscs.dal.ca/~selinger/random/, 2020-05-25.

#define NAME "glibc-rand-emu"

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

// Handles indices from -32 to +infty correctly.
#define INDEX(i) (((i) + 32) % 32)
#define r(i) (m_aState[INDEX(i)])
void CPrng::Seed(unsigned int Seed)
{
	// The PRNG behaves badly when seeded with 0.
	if(Seed == 0)
	{
		Seed = 1;
	}

	m_Seeded = true;
	str_format(m_aDescription, sizeof(m_aDescription), "%s:%08x", NAME, Seed);

	int s = Seed;

	r(0) = s;
	r(1) = (16807 * (int64)(int)r(0)) % 2147483647;
	for(int i = 1; i <= 30; i++)
	{
		r(i) = (16807 * (int64)r(i - 1)) % 2147483647;
	}
	r(31) = r(0);
	r(32) = r(1);
	r(33) = r(2);

	m_Index = INDEX(34);
	for(int i = 34; i <= 343; i++)
	{
		RandomInt();
	}
}

int CPrng::RandomInt()
{
	dbg_assert(m_Seeded, "prng needs to be seeded before it can generate random numbers");
	int i = m_Index;

	r(i) = r(i - 3) + r(i - 31); // modulo 4294967296 is implicit.

	int Result = r(i) >> 1;
	m_Index = INDEX(i + 1);
	return Result;
}
