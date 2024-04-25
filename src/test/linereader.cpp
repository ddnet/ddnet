#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/linereader.h>

void TestFileLineReader(const char *pWritten, bool SkipBom, std::initializer_list<const char *> pReads)
{
	CTestInfo Info;
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, pWritten, str_length(pWritten)), str_length(pWritten));
	EXPECT_FALSE(io_close(File));
	File = io_open(Info.m_aFilename, IOFLAG_READ | (SkipBom ? IOFLAG_SKIP_BOM : 0));
	ASSERT_TRUE(File);
	CLineReader LineReader;
	LineReader.Init(File);
	for(const char *pRead : pReads)
	{
		const char *pReadLine = LineReader.Get();
		ASSERT_TRUE(pReadLine);
		EXPECT_STREQ(pReadLine, pRead);
	}
	EXPECT_FALSE(LineReader.Get());
	EXPECT_FALSE(io_close(File));

	fs_remove(Info.m_aFilename);
}

TEST(LineReader, NormalNewline)
{
	TestFileLineReader("foo\nbar\nbaz\n", false, {"foo", "bar", "baz"});
}

TEST(LineReader, CRLFNewline)
{
	TestFileLineReader("foo\r\nbar\r\nbaz", true, {"foo", "bar", "baz"});
}

TEST(LineReader, Invalid)
{
	// Lines containing invalid UTF-8 are skipped
	TestFileLineReader("foo\xff\nbar\xff\nbaz\xff\n", false, {});
	TestFileLineReader("foo\xff\nbar\nbaz\n", false, {"bar", "baz"});
	TestFileLineReader("foo\nbar\xff\nbaz\n", false, {"foo", "baz"});
	TestFileLineReader("foo\nbar\nbaz\xff\n", false, {"foo", "bar"});
	TestFileLineReader("foo\nbar1\xff\nbar2\xff\nfoobar\nbar3\xff\nbaz\n", false, {"foo", "foobar", "baz"});
}
