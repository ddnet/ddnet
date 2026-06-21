#include "test.h"

#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/shared/linereader.h>

#include <gtest/gtest.h>

void TestFileLineReaderRaw(const char *pWritten, unsigned WrittenLength, std::initializer_list<const char *> pExpectedLines, bool ExpectSuccess, bool WriteBom)
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
		for(const char *pExpectedLine : pExpectedLines)
		{
			const char *pActualLine = LineReader.Get();
			ASSERT_TRUE(pActualLine) << "Line reader returned less lines than expected. Expected next line: '" << pExpectedLine << "'";
			EXPECT_STREQ(pActualLine, pExpectedLine) << "Line reader returned unexpected line";
		}
		const char *pActualLastLine = LineReader.Get();
		EXPECT_FALSE(pActualLastLine) << "Line reader returned more lines than expected. Unexpected last line: '" << pActualLastLine << "'";
	}

	EXPECT_FALSE(fs_remove(Info.m_aFilename));
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

TEST(LineReader, LineFeedLineEndings)
{
	TestFileLineReader("foo\nbar\nbaz", {"foo", "bar", "baz"});
	TestFileLineReader("foo\nbar\nbaz\n", {"foo", "bar", "baz"});
}

TEST(LineReader, CarriageReturnLineFeedLineEndings)
{
	TestFileLineReader("foo\r\nbar\r\nbaz", {"foo", "bar", "baz"});
	TestFileLineReader("foo\r\nbar\r\nbaz\r\n", {"foo", "bar", "baz"});
}

TEST(LineReader, CarriageReturnLineEndings)
{
	// Line ending `\r` not supported
	TestFileLineReader("foo\rbar\rbaz", {});
	TestFileLineReader("foo\rbar\rbaz\r", {});
}

TEST(LineReader, MixedLineEndings)
{
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6", {"1", "2", "3", "4", "5", "6"});
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6\n", {"1", "2", "3", "4", "5", "6"});
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6\r\n", {"1", "2", "3", "4", "5", "6"});
	TestFileLineReader("1\n2\r\n3\n4\n5\r\n6\r", {"1", "2", "3", "4", "5"}); // Line with trailing `\r` is skipped
}

TEST(LineReader, EmptyLines)
{
	TestFileLineReader("\n\r\n\n\n\r\n", {"", "", "", "", ""});
	TestFileLineReader("\n\r\n\n\n\r\n\n", {"", "", "", "", "", ""});
	TestFileLineReader("\n\r\n\n\n\r\n\r\n", {"", "", "", "", "", ""});
	TestFileLineReader("\n\r\n\n\n\r\n\r", {"", "", "", "", ""}); // Line with trailing `\r` is skipped
}

TEST(LineReader, InvalidUtf8)
{
	// Lines containing invalid UTF-8 are skipped
	TestFileLineReader("foo\xff\nbar\xff\nbaz\xff", {});
	TestFileLineReader("foo\xff\nbar\nbaz", {"bar", "baz"});
	TestFileLineReader("foo\nbar\xff\nbaz", {"foo", "baz"});
	TestFileLineReader("foo\nbar\nbaz\xff", {"foo", "bar"});
	TestFileLineReader("foo\nbar1\xff\nbar2\xff\nfoobar\nbar3\xff\nbaz", {"foo", "foobar", "baz"});
}

TEST(LineReader, ControlCharacters)
{
	// Lines containing control characters except `\t` are skipped
	TestFileLineReader(
		"\x01\n\x02\n\x03\n\x04\n\x05\n\x06\n\x07\n\x08\n\x09\n\x0B\n\x0C\n\x0E\n\x0F\n\x10\n" // `\0x0A` and `\0x0D` are `\n` and `\r`
		"\x11\n\x12\n\x13\n\x14\n\x15\n\x16\n\x17\n\x18\n\x19\n\x1A\n\x1B\n\x1C\n\x1D\n\x1E\n\x1F",
		{"\t"});
}

TEST(LineReader, NullBytes)
{
	// Line reader does not read any lines if the file contains null bytes
	TestFileLineReaderRaw("foo\0\nbar\nbaz", 12, {}, false);
	TestFileLineReaderRaw("foo\nbar\0\nbaz", 12, {}, false);
	TestFileLineReaderRaw("foo\nbar\nbaz\0", 12, {}, false);
}
