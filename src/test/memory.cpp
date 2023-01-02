#include <gtest/gtest.h>

#include <base/system.h>

#include <random>
#include <vector>

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

TEST(Memory, PODTypes)
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

struct CDummyClass
{
	int x = 1234;
	int y;
};

bool mem_is_null(CDummyClass *block, size_t size)
{
	const CDummyClass *data = block;
	size_t i;
	for(i = 0; i < size / sizeof(CDummyClass); i++)
	{
		if(data[i].x != 1234 || data[i].y != 0)
		{
			return false;
		}
	}
	return true;
}

TEST(Memory, ConstructibleTypes)
{
	CDummyClass aTest[123];
	mem_fill(aTest, sizeof(aTest));
	EXPECT_FALSE(mem_is_null(aTest, sizeof(aTest)));
	mem_zero(&aTest, sizeof(aTest));
	EXPECT_TRUE(mem_is_null(aTest, sizeof(aTest)));
	mem_fill(aTest, sizeof(aTest));
	EXPECT_FALSE(mem_is_null(aTest, sizeof(aTest)));
	mem_zero(&aTest[0], sizeof(aTest));
	EXPECT_TRUE(mem_is_null(aTest, sizeof(aTest)));

	mem_fill(aTest, sizeof(aTest));
	EXPECT_FALSE(mem_is_null(aTest, sizeof(aTest)));
	mem_zero(aTest, sizeof(aTest));
	EXPECT_TRUE(mem_is_null(aTest, sizeof(aTest)));

	CDummyClass aaTest[2][2];
	mem_fill(aaTest, sizeof(aaTest));
	EXPECT_FALSE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_zero(&aaTest, sizeof(aaTest));
	EXPECT_TRUE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_fill(aaTest, sizeof(aaTest));
	EXPECT_FALSE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_zero(&aaTest[0], sizeof(aaTest));
	EXPECT_TRUE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_fill(aaTest, sizeof(aaTest));
	EXPECT_FALSE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_zero(&aaTest[0][0], sizeof(aaTest));
	EXPECT_TRUE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));

	mem_fill(aaTest, sizeof(aaTest));
	EXPECT_FALSE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_zero(aaTest, sizeof(aaTest));
	EXPECT_TRUE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_fill(aaTest, sizeof(aaTest));
	EXPECT_FALSE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
	mem_zero(aaTest[0], sizeof(aaTest));
	EXPECT_TRUE(mem_is_null((CDummyClass *)aaTest, sizeof(aaTest)));
}

struct CDummyClass2
{
	int x;
	int y;
	~CDummyClass2()
	{
		if(x == 684684 && y == -12345678)
			dbg_break();
	}
};

TEST(Memory, DestructibleTypes)
{
	CDummyClass2 aTest2[123];
	mem_fill(aTest2, sizeof(aTest2));
	EXPECT_FALSE(mem_is_null(aTest2, sizeof(aTest2)));
	mem_zero(&aTest2, sizeof(aTest2));
	EXPECT_TRUE(mem_is_null(aTest2, sizeof(aTest2)));
	mem_fill(aTest2, sizeof(aTest2));
	EXPECT_FALSE(mem_is_null(aTest2, sizeof(aTest2)));
	mem_zero(&aTest2[0], sizeof(aTest2));
	EXPECT_TRUE(mem_is_null(aTest2, sizeof(aTest2)));

	mem_fill(aTest2, sizeof(aTest2));
	EXPECT_FALSE(mem_is_null(aTest2, sizeof(aTest2)));
	mem_zero(aTest2, sizeof(aTest2));
	EXPECT_TRUE(mem_is_null(aTest2, sizeof(aTest2)));

	CDummyClass2 aaTest2[10][20];
	mem_fill(aaTest2, sizeof(aaTest2));
	EXPECT_FALSE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_zero(&aaTest2, sizeof(aaTest2));
	EXPECT_TRUE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_fill(aaTest2, sizeof(aaTest2));
	EXPECT_FALSE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_zero(&aaTest2[0], sizeof(aaTest2));
	EXPECT_TRUE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_fill(aaTest2, sizeof(aaTest2));
	EXPECT_FALSE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_zero(&aaTest2[0][0], sizeof(aaTest2));
	EXPECT_TRUE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));

	mem_fill(aaTest2, sizeof(aaTest2));
	EXPECT_FALSE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_zero(aaTest2, sizeof(aaTest2));
	EXPECT_TRUE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));

	mem_fill(aaTest2, sizeof(aaTest2));
	EXPECT_FALSE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
	mem_zero(aaTest2[0], sizeof(aaTest2));
	EXPECT_TRUE(mem_is_null((CDummyClass2 *)aaTest2, sizeof(aaTest2)));
}

struct CDummyClass3
{
	int x;
	int y;
	std::vector<int> v;
};

void mem_fill(CDummyClass3 *block, size_t size)
{
	size_t i;
	for(i = 0; i < size / sizeof(CDummyClass3); i++)
	{
		block[i].x = gs_randomizer();
		block[i].y = gs_randomizer();
		block[i].v.resize(gs_randomizer() & 127, gs_randomizer());
	}
}

bool mem_is_null(CDummyClass3 *block, size_t size)
{
	const CDummyClass3 *data = block;
	size_t i;
	for(i = 0; i < size / sizeof(CDummyClass3); i++)
	{
		if(data[i].x != 0 || data[i].y != 0 || !data[i].v.empty())
		{
			return false;
		}
	}
	return true;
}

TEST(Memory, ComplexTypes)
{
	CDummyClass3 aTest3[123];
	mem_fill(aTest3, sizeof(aTest3));
	EXPECT_FALSE(mem_is_null(aTest3, sizeof(aTest3)));
	mem_zero(&aTest3, sizeof(aTest3));
	EXPECT_TRUE(mem_is_null(aTest3, sizeof(aTest3)));
	mem_fill(aTest3, sizeof(aTest3));
	EXPECT_FALSE(mem_is_null(aTest3, sizeof(aTest3)));
	mem_zero(&aTest3[0], sizeof(aTest3));
	EXPECT_TRUE(mem_is_null(aTest3, sizeof(aTest3)));

	mem_fill(aTest3, sizeof(aTest3));
	EXPECT_FALSE(mem_is_null(aTest3, sizeof(aTest3)));
	mem_zero(aTest3, sizeof(aTest3));
	EXPECT_TRUE(mem_is_null(aTest3, sizeof(aTest3)));

	CDummyClass3 aaTest3[10][20];
	mem_fill((CDummyClass3 *)aaTest3, sizeof(aaTest3));
	EXPECT_FALSE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_zero(&aaTest3, sizeof(aaTest3));
	EXPECT_TRUE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_fill((CDummyClass3 *)aaTest3, sizeof(aaTest3));
	EXPECT_FALSE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_zero(&aaTest3[0], sizeof(aaTest3));
	EXPECT_TRUE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_fill((CDummyClass3 *)aaTest3, sizeof(aaTest3));
	EXPECT_FALSE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_zero(&aaTest3[0][0], sizeof(aaTest3));
	EXPECT_TRUE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));

	mem_fill((CDummyClass3 *)aaTest3, sizeof(aaTest3));
	EXPECT_FALSE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_zero(aaTest3, sizeof(aaTest3));
	EXPECT_TRUE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_fill((CDummyClass3 *)aaTest3, sizeof(aaTest3));
	EXPECT_FALSE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
	mem_zero(aaTest3[0], sizeof(aaTest3));
	EXPECT_TRUE(mem_is_null((CDummyClass3 *)aaTest3, sizeof(aaTest3)));
}
