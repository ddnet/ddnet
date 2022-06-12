#include <gtest/gtest.h>

#include <engine/shared/uuid_manager.h>

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
