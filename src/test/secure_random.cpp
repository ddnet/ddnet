#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

TEST(SecureRandom, Fill)
{
	unsigned int Bits = 0;
	while(~Bits)
	{
		unsigned int Random;
		secure_random_fill(&Random, sizeof(Random));
		Bits |= Random;
	}
}

TEST(SecureRandom, Below1)
{
	EXPECT_EQ(secure_rand_below(1), 0);
}

TEST(SecureRandom, Below)
{
	const int BOUNDS[] = {2, 3, 4, 5, 10, 100, 127, 128, 129};
	for(auto Below : BOUNDS)
	{
		for(int j = 0; j < 10; j++)
		{
			int Random = secure_rand_below(Below);
			EXPECT_GE(Random, 0);
			EXPECT_LT(Random, Below);
		}
	}
}
