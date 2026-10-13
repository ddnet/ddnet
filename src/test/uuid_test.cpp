#include <engine/shared/uuid_manager.h>

#include <gtest/gtest.h>

TEST(Uuid, FromToString)
{
	char aUuid[UUID_MAXSTRSIZE];
	CUuid Uuid;
	EXPECT_FALSE(ParseUuid(&Uuid, "8d300ecf-5873-4297-bee5-95668fdff320"));
	FormatUuid(Uuid, aUuid, sizeof(aUuid));
	EXPECT_STREQ(aUuid, "8d300ecf-5873-4297-bee5-95668fdff320");
	EXPECT_FALSE(ParseUuid(&Uuid, "00000000-0000-0000-0000-000000000000"));
	FormatUuid(Uuid, aUuid, sizeof(aUuid));
	EXPECT_STREQ(aUuid, "00000000-0000-0000-0000-000000000000");
	EXPECT_FALSE(ParseUuid(&Uuid, "ffffffff-ffff-ffff-ffff-ffffffffffff"));
	FormatUuid(Uuid, aUuid, sizeof(aUuid));
	EXPECT_STREQ(aUuid, "ffffffff-ffff-ffff-ffff-ffffffffffff");
	EXPECT_FALSE(ParseUuid(&Uuid, "01234567-89aB-cDeF-0123-456789AbCdEf"));
	FormatUuid(Uuid, aUuid, sizeof(aUuid));
	EXPECT_STREQ(aUuid, "01234567-89ab-cdef-0123-456789abcdef");
}

TEST(Uuid, ParseFailures)
{
	CUuid Uuid;
	EXPECT_TRUE(ParseUuid(&Uuid, ""));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab-cdef-0123-456789abcdeg"));
	EXPECT_TRUE(ParseUuid(&Uuid, "0-0-0-0-0"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab-cdef-0123-456789abcde"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab-cdef-0123-456789abcdef0"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567+89ab-cdef-0123-456789abcdef"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab+cdef-0123-456789abcdef"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab-cdef+0123-456789abcdef"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab-cdef-0123+456789abcdef"));
	EXPECT_TRUE(ParseUuid(&Uuid, "01234567-89ab-cdef-0123-456789abcdef "));
	EXPECT_TRUE(ParseUuid(&Uuid, "0x01234567-89ab-cdef-0123-456789abcdef"));
}

TEST(Uuid, ToInts)
{
	CUuid Uuid;
	char aUuid[UUID_MAXSTRSIZE] = "01234567-89ab-cdef-0123-456789abcdef";
	EXPECT_FALSE(ParseUuid(&Uuid, aUuid));

	int aInts[UUID_INT_SIZE];
	UuidToInts(Uuid, aInts);
	// hex values are readable from the string
	int aExpected[UUID_INT_SIZE]{
		0x01234567,
		std::bit_cast<int32_t>(0x89abcdef),
		0x01234567,
		std::bit_cast<int32_t>(0x89abcdef),
	};
	for(int Index = 0; Index < UUID_INT_SIZE; ++Index)
	{
		ASSERT_EQ(aInts[Index], aExpected[Index]);
	}
}

TEST(Uuid, FromInts)
{
	int aInts[UUID_INT_SIZE]{
		0x01234567,
		std::bit_cast<int32_t>(0x89abcdef),
		0x01234567,
		std::bit_cast<int32_t>(0x89abcdef),
	};
	int *pInts = &aInts[0];
	CUuid Uuid1 = UuidFromInts(aInts);
	CUuid Uuid2 = UuidFromInts(pInts);
	ASSERT_EQ(Uuid1, Uuid2);

	char aUuid1[UUID_MAXSTRSIZE];
	char aUuid2[UUID_MAXSTRSIZE];
	FormatUuid(Uuid1, aUuid1, sizeof(aUuid1));
	FormatUuid(Uuid2, aUuid2, sizeof(aUuid2));
	EXPECT_STREQ(aUuid1, "01234567-89ab-cdef-0123-456789abcdef");
	EXPECT_STREQ(aUuid2, "01234567-89ab-cdef-0123-456789abcdef");
}
