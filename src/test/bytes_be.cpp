#include "test.h"

#include <gtest/gtest.h>

#include <base/system.h>

static const int32_t INT_DATA[] = {0, 1, -1, 32, 64, 256, -512, 12345, -123456, 1234567, 12345678, 123456789, 2147483647, (-2147483647 - 1)};
static const uint32_t UINT_DATA[] = {0u, 1u, 2u, 32u, 64u, 256u, 512u, 12345u, 123456u, 1234567u, 12345678u, 123456789u, 2147483647u, 2147483648u, 4294967295u};

TEST(BytePacking, RoundtripInt)
{
	for(auto i : INT_DATA)
	{
		unsigned char aPacked[sizeof(int32_t)];
		uint_to_bytes_be(aPacked, i);
		EXPECT_EQ(bytes_be_to_uint(aPacked), i);
	}
}

TEST(BytePacking, RoundtripUnsigned)
{
	for(auto u : UINT_DATA)
	{
		unsigned char aPacked[sizeof(uint32_t)];
		uint_to_bytes_be(aPacked, u);
		EXPECT_EQ(bytes_be_to_uint(aPacked), u);
	}
}
