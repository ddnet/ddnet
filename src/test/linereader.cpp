#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/linereader.h>

void TestFileLineReaderRaw(const char *pWritten, unsigned WrittenLength, std::initializer_list<const char *> pReads, bool ExpectSuccess, bool WriteBom)
{
	CTestInfo Info;
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	if(WriteBom)
	{
		constexpr const unsigned char UTF8_BOM[] = {0xEF, 0xBB, 0xBF};
		EXPECT_EQ(io_write(File, UTF8_BOM, sizeof(UTF8_BOM)), sizeof(UTF8_BOM));
	}
	EXPECT_EQ(io_write(File, pWritten, WrittenLength), WrittenLength);
	EXPECT_FALSE(io_close(File));

	CLineReader LineReader;
	const bool ActualSuccess = LineReader.OpenFile(io_open(Info.m_aFilename, IOFLAG_READ));
	ASSERT_EQ(ActualSuccess, ExpectSuccess);
	if(ActualSuccess)
	{
		for(const char *pRead : pReads)
		{
			const char *pReadLine = LineReader.Get();
			ASSERT_TRUE(pReadLine) << "Line reader returned less lines than expected";
			EXPECT_STREQ(pReadLine, pRead) << "Line reader returned unexpected line";
		}
		EXPECT_FALSE(LineReader.Get()) << "Line reader returned more lines than expected";
	}

	fs_remove(Info.m_aFilename);
}

void TestFileLineReaderRaw(const char *pWritten, unsigned WrittenLength, std::initializer_list<const char *> pReads, bool ExpectSuccess)
{
	TestFileLineReaderRaw(pWritten, WrittenLength, pReads, ExpectSuccess, false);
	TestFileLineReaderRaw(pWritten, WrittenLength, pReads, ExpectSuccess, true);
}

void TestFileLineReader(const char *pWritten, std::initializer_list<const char *> pReads)
{
	TestFileLineReaderRaw(pWritten, str_length(pWritten), pReads, true);
}

TEST(LineReader, NormalNewline)
{
	TestFileLineReader("foo\nbar\nbaz", {"foo", "bar", "baz"});
	TestFileLineReader("foo\nbar\nbaz\n", {"foo", "bar", "baz"});
}

TEST(LineReader, CRLFNewline)
{
	TestFileLineReader("foo\r\nbar\r\nbaz", {"foo", "bar", "baz"});
	TestFileLineReader("foo\r\nbar\r\nbaz\r\n", {"foo", "bar", "baz"});
}

TEST(LineReader, MixedNewline)
{
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6", {"1", "2", "3", "4", "5", "6"});
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6\n", {"1", "2", "3", "4", "5", "6"});
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6\r\n", {"1", "2", "3", "4", "5", "6"});
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6\r", {"1", "2", "3", "4", "5", "6\r"});
}

TEST(LineReader, EmptyLines)
{
	TestFileLineReader("\n\r\n\n\n\r\n", {"", "", "", "", ""});
	TestFileLineReader("\n\r\n\n\n\r\n\n", {"", "", "", "", "", ""});
	TestFileLineReader("\n\r\n\n\n\r\n\r\n", {"", "", "", "", "", ""});
	TestFileLineReader("\n\r\n\n\n\r\n\r", {"", "", "", "", "", "\r"});
}

TEST(LineReader, Invalid)
{
	// Lines containing invalid UTF-8 are skipped
	TestFileLineReader("foo\xff\nbar\xff\nbaz\xff", {});
	TestFileLineReader("foo\xff\nbar\nbaz", {"bar", "baz"});
	TestFileLineReader("foo\nbar\xff\nbaz", {"foo", "baz"});
	TestFileLineReader("foo\nbar\nbaz\xff", {"foo", "bar"});
	TestFileLineReader("foo\nbar1\xff\nbar2\xff\nfoobar\nbar3\xff\nbaz", {"foo", "foobar", "baz"});
}

TEST(LineReader, NullBytes)
{
	// Line reader does not read any lines if the file contains null bytes
	TestFileLineReaderRaw("foo\0\nbar\nbaz", 12, {}, false);
	TestFileLineReaderRaw("foo\nbar\0\nbaz", 12, {}, false);
	TestFileLineReaderRaw("foo\nbar\nbaz\0", 12, {}, false);
}
