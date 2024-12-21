#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

static void TestFileRead(const char *pWritten)
{
	const int WrittenLength = str_length(pWritten);

	char aBuf[64] = {0};
	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, pWritten, WrittenLength), WrittenLength);
	EXPECT_FALSE(io_close(File));

	File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_read(File, aBuf, sizeof(aBuf)), WrittenLength);
	EXPECT_EQ(mem_comp(aBuf, pWritten, WrittenLength), 0);
	EXPECT_FALSE(io_close(File));
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}

TEST(Io, Read1)
{
	TestFileRead("");
}
TEST(Io, Read2)
{
	TestFileRead("abc");
}
TEST(Io, Read3)
{
	TestFileRead("\xef\xbb\xbf");
}
TEST(Io, Read4)
{
	TestFileRead("\xef\xbb\xbfxyz");
}

static void TestFileLength(const char *pWritten)
{
	const int WrittenLength = str_length(pWritten);

	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, pWritten, WrittenLength), WrittenLength);
	EXPECT_FALSE(io_close(File));

	File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_length(File), WrittenLength);
	EXPECT_FALSE(io_close(File));
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}

TEST(Io, Length1)
{
	TestFileLength("");
}
TEST(Io, Length2)
{
	TestFileLength("abc");
}
TEST(Io, Length3)
{
	TestFileLength("\xef\xbb\xbf");
}
TEST(Io, Length4)
{
	TestFileLength("\xef\xbb\xbfxyz");
}

TEST(Io, SeekTellSkip)
{
	const char *pWritten1 = "01234567890123456789";
	const int WrittenLength1 = str_length(pWritten1);
	const char *pWritten2 = "abc";
	const int WrittenLength2 = str_length(pWritten2);
	const char *pWritten3 = "def";
	const int WrittenLength3 = str_length(pWritten3);
	const char *pWritten4 = "ghi";
	const int WrittenLength4 = str_length(pWritten4);
	const char *pWritten5 = "jkl";
	const int WrittenLength5 = str_length(pWritten5);
	const char *pExpectedResult = "01def5ghi9abc3456789jkl";
	const int ExpectedLength = str_length(pExpectedResult);

	char aBuf[64] = {0};
	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, pWritten1, WrittenLength1), WrittenLength1);
	EXPECT_FALSE(io_seek(File, -10, IOSEEK_CUR));
	EXPECT_EQ(io_write(File, pWritten2, WrittenLength2), WrittenLength2);
	EXPECT_FALSE(io_seek(File, 2, IOSEEK_START));
	EXPECT_EQ(io_write(File, pWritten3, WrittenLength3), WrittenLength3);
	EXPECT_FALSE(io_skip(File, 1));
	EXPECT_EQ(io_write(File, pWritten4, WrittenLength4), WrittenLength4);
	EXPECT_FALSE(io_seek(File, 0, IOSEEK_END));
	EXPECT_EQ(io_write(File, pWritten5, WrittenLength5), WrittenLength5);
	EXPECT_FALSE(io_close(File));

	File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_read(File, aBuf, sizeof(aBuf)), ExpectedLength);
	EXPECT_EQ(mem_comp(aBuf, pExpectedResult, ExpectedLength), 0);
	EXPECT_FALSE(io_seek(File, -13, IOSEEK_CUR));
	EXPECT_EQ(io_read(File, aBuf, WrittenLength2), WrittenLength2);
	EXPECT_EQ(mem_comp(aBuf, pWritten2, WrittenLength2), 0);
	EXPECT_FALSE(io_seek(File, 2, IOSEEK_START));
	EXPECT_EQ(io_read(File, aBuf, WrittenLength3), WrittenLength3);
	EXPECT_EQ(mem_comp(aBuf, pWritten3, WrittenLength3), 0);
	EXPECT_FALSE(io_skip(File, 1));
	EXPECT_EQ(io_read(File, aBuf, WrittenLength4), WrittenLength4);
	EXPECT_EQ(mem_comp(aBuf, pWritten4, WrittenLength4), 0);
	EXPECT_FALSE(io_seek(File, -3, IOSEEK_END));
	EXPECT_EQ(io_read(File, aBuf, WrittenLength5), WrittenLength5);
	EXPECT_EQ(mem_comp(aBuf, pWritten5, WrittenLength5), 0);
	EXPECT_FALSE(io_close(File));
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}

TEST(Io, CurrentExe)
{
	IOHANDLE CurrentExe = io_current_exe();
	ASSERT_TRUE(CurrentExe);
	EXPECT_GE(io_length(CurrentExe), 1024);
	io_close(CurrentExe);
}

TEST(Io, SyncWorks)
{
	CTestInfo Info;
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, "abc\n", 4), 4);
	EXPECT_FALSE(io_sync(File));
	EXPECT_FALSE(io_close(File));
	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}

TEST(Io, WriteTruncatesFile)
{
	CTestInfo Info;

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, "0123456789", 10), 10);
	EXPECT_FALSE(io_close(File));

	File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	EXPECT_EQ(io_write(File, "ABCDE", 5), 5);
	EXPECT_FALSE(io_close(File));

	char aBuf[16];
	File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_read(File, aBuf, sizeof(aBuf)), 5);
	EXPECT_TRUE(mem_comp(aBuf, "ABCDE", 5) == 0);
	EXPECT_FALSE(io_close(File));

	EXPECT_FALSE(fs_remove(Info.m_aFilename));
}
