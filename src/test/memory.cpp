#include <gtest/gtest.h>

#include <base/system.h>

#include <random>

static std::mt19937 gs_randomizer;

void mem_fill(void *block, size_t size)
{
	unsigned char *bytes = (unsigned char *)block;
	size_t i;
	for(i = 0; i < size; i++)
	{
		bytes[i] = gs_randomizer();
	}
}

bool mem_is_null(const void *block, size_t size)
{
	const unsigned char *bytes = (const unsigned char *)block;
	size_t i;
	for(i = 0; i < size; i++)
	{
		if(bytes[i] != 0)
		{
			return false;
		}
	}
	return true;
}

TEST(Memory, BaseTypes)
{
	void *aVoid[123];
	mem_fill(aVoid, sizeof(aVoid));
	EXPECT_FALSE(mem_is_null(aVoid, sizeof(aVoid)));
	mem_zero(&aVoid, sizeof(aVoid));
	EXPECT_TRUE(mem_is_null(aVoid, sizeof(aVoid)));
	mem_fill(aVoid, sizeof(aVoid));
	EXPECT_FALSE(mem_is_null(aVoid, sizeof(aVoid)));
	mem_zero(&aVoid[0], 123 * sizeof(void *));
	EXPECT_TRUE(mem_is_null(aVoid, sizeof(aVoid)));

	mem_fill(aVoid, sizeof(aVoid));
	EXPECT_FALSE(mem_is_null(aVoid, sizeof(aVoid)));
	mem_zero(aVoid, sizeof(aVoid));
	EXPECT_TRUE(mem_is_null(aVoid, sizeof(aVoid)));

	int aInt[512];
	mem_fill(aInt, sizeof(aInt));
	EXPECT_FALSE(mem_is_null(aInt, sizeof(aInt)));
	mem_zero(&aInt, sizeof(aInt));
	EXPECT_TRUE(mem_is_null(aInt, sizeof(aInt)));
	mem_fill(aInt, sizeof(aInt));
	EXPECT_FALSE(mem_is_null(aInt, sizeof(aInt)));
	mem_zero(&aInt[0], sizeof(aInt));
	EXPECT_TRUE(mem_is_null(aInt, sizeof(aInt)));

	mem_fill(aInt, sizeof(aInt));
	EXPECT_FALSE(mem_is_null(aInt, sizeof(aInt)));
	mem_zero(aInt, sizeof(aInt));
	EXPECT_TRUE(mem_is_null(aInt, sizeof(aInt)));

	int *apInt[512];
	mem_fill(apInt, sizeof(apInt));
	EXPECT_FALSE(mem_is_null(apInt, sizeof(apInt)));
	mem_zero(&apInt, sizeof(apInt));
	EXPECT_TRUE(mem_is_null(apInt, sizeof(apInt)));

	mem_fill(apInt, sizeof(apInt));
	EXPECT_FALSE(mem_is_null(apInt, sizeof(apInt)));
	mem_zero(apInt, sizeof(apInt));
	EXPECT_TRUE(mem_is_null(apInt, sizeof(apInt)));

	int aaInt[10][20];
	mem_fill(aaInt, sizeof(aaInt));
	EXPECT_FALSE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_zero(&aaInt, sizeof(aaInt));
	EXPECT_TRUE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_fill(aaInt, sizeof(aaInt));
	EXPECT_FALSE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_zero(&aaInt[0], sizeof(aaInt));
	EXPECT_TRUE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_fill(aaInt, sizeof(aaInt));
	EXPECT_FALSE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_zero(&aaInt[0][0], sizeof(aaInt));
	EXPECT_TRUE(mem_is_null(aaInt, sizeof(aaInt)));

	mem_fill(aaInt, sizeof(aaInt));
	EXPECT_FALSE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_zero(aaInt, sizeof(aaInt));
	EXPECT_TRUE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_fill(aaInt, sizeof(aaInt));
	EXPECT_FALSE(mem_is_null(aaInt, sizeof(aaInt)));
	mem_zero(aaInt[0], sizeof(aaInt));
	EXPECT_TRUE(mem_is_null(aaInt, sizeof(aaInt)));

	int *aapInt[10][20];
	mem_fill(aapInt, sizeof(aapInt));
	EXPECT_FALSE(mem_is_null(aapInt, sizeof(aapInt)));
	mem_zero(&aapInt, sizeof(aapInt));
	EXPECT_TRUE(mem_is_null(aapInt, sizeof(aapInt)));
	mem_fill(aapInt, sizeof(aapInt));
	EXPECT_FALSE(mem_is_null(aapInt, sizeof(aapInt)));
	mem_zero(&aapInt[0], sizeof(aapInt));
	EXPECT_TRUE(mem_is_null(aapInt, sizeof(aapInt)));

	mem_fill(aapInt, sizeof(aapInt));
	EXPECT_FALSE(mem_is_null(aapInt, sizeof(aapInt)));
	mem_zero(aapInt, sizeof(aapInt));
	EXPECT_TRUE(mem_is_null(aapInt, sizeof(aapInt)));
	mem_fill(aapInt, sizeof(aapInt));
	EXPECT_FALSE(mem_is_null(aapInt, sizeof(aapInt)));
	mem_zero(aapInt[0], sizeof(aapInt));
	EXPECT_TRUE(mem_is_null(aapInt, sizeof(aapInt)));
}

TEST(Memory, PodTypes)
{
	NETADDR aAddr[123];
	mem_fill(aAddr, sizeof(aAddr));
	EXPECT_FALSE(mem_is_null(aAddr, sizeof(aAddr)));
	mem_zero(&aAddr, sizeof(aAddr));
	EXPECT_TRUE(mem_is_null(aAddr, sizeof(aAddr)));
	mem_fill(aAddr, sizeof(aAddr));
	EXPECT_FALSE(mem_is_null(aAddr, sizeof(aAddr)));
	mem_zero(&aAddr[0], sizeof(aAddr));
	EXPECT_TRUE(mem_is_null(aAddr, sizeof(aAddr)));

	mem_fill(aAddr, sizeof(aAddr));
	EXPECT_FALSE(mem_is_null(aAddr, sizeof(aAddr)));
	mem_zero(aAddr, sizeof(aAddr));
	EXPECT_TRUE(mem_is_null(aAddr, sizeof(aAddr)));

	NETADDR *apAddr[123];
	mem_fill(apAddr, sizeof(apAddr));
	EXPECT_FALSE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_zero((NETADDR **)apAddr, sizeof(apAddr));
	EXPECT_TRUE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_fill(apAddr, sizeof(apAddr));
	EXPECT_FALSE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_zero(&apAddr, sizeof(apAddr));
	EXPECT_TRUE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_fill(apAddr, sizeof(apAddr));
	EXPECT_FALSE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_zero(&apAddr[0], sizeof(apAddr));
	EXPECT_TRUE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_fill(apAddr, sizeof(apAddr));
	EXPECT_FALSE(mem_is_null(apAddr, sizeof(apAddr)));
	mem_zero(apAddr, sizeof(apAddr));
	EXPECT_TRUE(mem_is_null(apAddr, sizeof(apAddr)));

	// 2D arrays
	NETADDR aaAddr[10][20];
	mem_fill(aaAddr, sizeof(aaAddr));
	EXPECT_FALSE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_zero(&aaAddr, sizeof(aaAddr));
	EXPECT_TRUE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_fill(aaAddr, sizeof(aaAddr));
	EXPECT_FALSE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_zero(&aaAddr[0], sizeof(aaAddr));
	EXPECT_TRUE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_fill(aaAddr, sizeof(aaAddr));
	EXPECT_FALSE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_zero(&aaAddr[0][0], sizeof(aaAddr));
	EXPECT_TRUE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));

	mem_fill(aaAddr, sizeof(aaAddr));
	EXPECT_FALSE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_zero(aaAddr, sizeof(aaAddr));
	EXPECT_TRUE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_fill(aaAddr, sizeof(aaAddr));
	EXPECT_FALSE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));
	mem_zero(aaAddr[0], sizeof(aaAddr));
	EXPECT_TRUE(mem_is_null((NETADDR *)aaAddr, sizeof(aaAddr)));

	// 2D pointer arrays
	NETADDR *aapAddr[10][20];
	mem_fill(aapAddr, sizeof(aapAddr));
	EXPECT_FALSE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_zero(&aapAddr, sizeof(aapAddr));
	EXPECT_TRUE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_fill(aapAddr, sizeof(aapAddr));
	EXPECT_FALSE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_zero(&aapAddr[0], sizeof(aapAddr));
	EXPECT_TRUE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_fill(aapAddr, sizeof(aapAddr));
	EXPECT_FALSE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_zero(&aapAddr[0][0], sizeof(aapAddr));
	EXPECT_TRUE(mem_is_null(aapAddr, sizeof(aapAddr)));

	mem_fill(aapAddr, sizeof(aapAddr));
	EXPECT_FALSE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_zero(aapAddr, sizeof(aapAddr));
	EXPECT_TRUE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_fill(aapAddr, sizeof(aapAddr));
	EXPECT_FALSE(mem_is_null(aapAddr, sizeof(aapAddr)));
	mem_zero(aapAddr[0], sizeof(aapAddr));
	EXPECT_TRUE(mem_is_null(aapAddr, sizeof(aapAddr)));
}
