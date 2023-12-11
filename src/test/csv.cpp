#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/csv.h>

static void Expect(int NumColumns, const char *const *ppColumns, const char *pExpected)
{
	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	CsvWrite(File, NumColumns, ppColumns);
	io_close(File);

	char aBuf[1024];
	File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	int Read = io_read(File, aBuf, sizeof(aBuf));
	io_close(File);
	fs_remove(Info.m_aFilename);

	ASSERT_TRUE(Read >= 1);
	Read -= 1;
	ASSERT_EQ(aBuf[Read], '\n');
	aBuf[Read] = 0;

#if defined(CONF_FAMILY_WINDOWS)
	ASSERT_TRUE(Read >= 1);
	Read -= 1;
	ASSERT_EQ(aBuf[Read], '\r');
	aBuf[Read] = 0;
#endif

	for(int i = 0; i < Read; i++)
	{
		EXPECT_NE(aBuf[i], 0);
	}
	EXPECT_STREQ(aBuf, pExpected);
}

TEST(Csv, Simple)
{
	const char *apCols1[] = {"a", "b"};
	Expect(2, apCols1, "a,b");
	const char *apCols2[] = {"こんにちは"};
	Expect(1, apCols2, "こんにちは");
	const char *apCols3[] = {"я", "", "й"};
	Expect(3, apCols3, "я,,й");
	const char *apCols4[] = {""};
	Expect(1, apCols4, "");
	const char *apCols5[] = {0};
	Expect(0, apCols5, "");
}

TEST(Csv, LetTheQuotingBegin)
{
	const char *apCols1[] = {"\""};
	Expect(1, apCols1, "\"\"\"\"");
	const char *apCols2[] = {","};
	Expect(1, apCols2, "\",\"");
	const char *apCols3[] = {",,", ",\"\"\""};
	Expect(2, apCols3, "\",,\",\",\"\"\"\"\"\"\"");
	const char *apCols4[] = {"\",", " "};
	Expect(2, apCols4, "\"\"\",\", ");
}
