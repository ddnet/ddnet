#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

#include <array>

TEST(SwapEndian, Generic)
{
	const std::array<char, 12> aOriginalData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
	const std::array<char, 12> aExpectedDataInt16 = {2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11};
	const std::array<char, 12> aExpectedDataInt32 = {4, 3, 2, 1, 8, 7, 6, 5, 12, 11, 10, 9};

	std::array<char, 12> aData = aOriginalData;
	swap_endian(aData.data(), sizeof(char), aData.size() / sizeof(char));
	EXPECT_EQ(aData, aOriginalData);

	aData = aOriginalData;
	swap_endian(aData.data(), sizeof(int16_t), aData.size() / sizeof(int16_t));
	EXPECT_EQ(aData, aExpectedDataInt16);

	aData = aOriginalData;
	swap_endian(aData.data(), sizeof(int32_t), aData.size() / sizeof(int32_t));
	EXPECT_EQ(aData, aExpectedDataInt32);
}

static int SwapEndianInt(int Number)
{
	swap_endian(&Number, sizeof(Number), 1);
	return Number;
}

TEST(SwapEndian, Int)
{
	EXPECT_EQ(SwapEndianInt(0x00000000), 0x00000000);
	EXPECT_EQ(SwapEndianInt(0xFFFFFFFF), 0xFFFFFFFF);
	EXPECT_EQ(SwapEndianInt(0x7FFFFFFF), 0xFFFFFF7F);
	EXPECT_EQ(SwapEndianInt(0xFFFFFF7F), 0x7FFFFFFF);
	EXPECT_EQ(SwapEndianInt(0x80000000), 0x00000080);
	EXPECT_EQ(SwapEndianInt(0x00000080), 0x80000000);
	EXPECT_EQ(SwapEndianInt(0x12345678), 0x78563412);
	EXPECT_EQ(SwapEndianInt(0x78563412), 0x12345678);
	EXPECT_EQ(SwapEndianInt(0x87654321), 0x21436587);
	EXPECT_EQ(SwapEndianInt(0x21436587), 0x87654321);
}
