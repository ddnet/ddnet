#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>

void TestFileRead(const char *pWritten)
{
	CTestInfo Info;
	char aBuf[512] = {0};
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	const int WrittenLength = str_length(pWritten);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_write(File, pWritten, WrittenLength), WrittenLength);
	EXPECT_FALSE(io_close(File));
	File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	EXPECT_EQ(io_read(File, aBuf, sizeof(aBuf)), WrittenLength);
	EXPECT_TRUE(mem_comp(aBuf, pWritten, WrittenLength) == 0);
	EXPECT_FALSE(io_close(File));

	fs_remove(Info.m_aFilename);
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
