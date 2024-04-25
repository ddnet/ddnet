/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/shared/jsonwriter.h>

#include <limits>

class JsonFileWriter
{
public:
	CTestInfo m_Info;
	CJsonFileWriter *m_pJson;
	char m_aOutputFilename[IO_MAX_PATH_LENGTH];

	JsonFileWriter() :
		m_pJson(nullptr)
	{
		m_Info.Filename(m_aOutputFilename, sizeof(m_aOutputFilename), "-got.json");
		IOHANDLE File = io_open(m_aOutputFilename, IOFLAG_WRITE);
		EXPECT_TRUE(File);
		m_pJson = new CJsonFileWriter(File);
	}

	void Expect(const char *pExpected)
	{
		ASSERT_TRUE(m_pJson);
		delete m_pJson;
		m_pJson = nullptr;

		IOHANDLE GotFile = io_open(m_aOutputFilename, IOFLAG_READ);
		ASSERT_TRUE(GotFile);
		char *pOutput = io_read_all_str(GotFile);
		io_close(GotFile);
		ASSERT_TRUE(pOutput);
		EXPECT_STREQ(pOutput, pExpected);
		bool Correct = str_comp(pOutput, pExpected) == 0;
		free(pOutput);

		if(!Correct)
		{
			char aFilename[IO_MAX_PATH_LENGTH];
			m_Info.Filename(aFilename, sizeof(aFilename), "-expected.json");
			IOHANDLE ExpectedFile = io_open(aFilename, IOFLAG_WRITE);
			ASSERT_TRUE(ExpectedFile);
			io_write(ExpectedFile, pExpected, str_length(pExpected));
			io_close(ExpectedFile);
		}
		else
		{
			fs_remove(m_aOutputFilename);
		}
	}
};

class JsonStringWriter
{
public:
	CJsonStringWriter *m_pJson;

	JsonStringWriter() :
		m_pJson(nullptr)
	{
		m_pJson = new CJsonStringWriter();
	}

	void Expect(const char *pExpected)
	{
		ASSERT_TRUE(m_pJson);
		const std::string OutputString = m_pJson->GetOutputString();
		EXPECT_STREQ(OutputString.c_str(), pExpected);
		delete m_pJson;
		m_pJson = nullptr;
	}
};

template<typename T>
class JsonWriters : public testing::Test
{
public:
	T Impl;
};

using JsonWriterTestFixures = ::testing::Types<JsonFileWriter, JsonStringWriter>;
TYPED_TEST_SUITE(JsonWriters, JsonWriterTestFixures);

TYPED_TEST(JsonWriters, Empty)
{
	this->Impl.Expect("\n");
}

TYPED_TEST(JsonWriters, EmptyObject)
{
	this->Impl.m_pJson->BeginObject();
	this->Impl.m_pJson->EndObject();
	this->Impl.Expect("{\n}\n");
}

TYPED_TEST(JsonWriters, EmptyArray)
{
	this->Impl.m_pJson->BeginArray();
	this->Impl.m_pJson->EndArray();
	this->Impl.Expect("[\n]\n");
}

TYPED_TEST(JsonWriters, SpecialCharacters)
{
	this->Impl.m_pJson->BeginObject();
	this->Impl.m_pJson->WriteAttribute("\x01\"'\r\n\t");
	this->Impl.m_pJson->BeginArray();
	this->Impl.m_pJson->WriteStrValue(" \"'abc\x01\n");
	this->Impl.m_pJson->EndArray();
	this->Impl.m_pJson->EndObject();
	this->Impl.Expect(
		"{\n"
		"\t\"\\u0001\\\"'\\r\\n\\t\": [\n"
		"\t\t\" \\\"'abc\\u0001\\n\"\n"
		"\t]\n"
		"}\n");
}

TYPED_TEST(JsonWriters, HelloWorld)
{
	this->Impl.m_pJson->WriteStrValue("hello world");
	this->Impl.Expect("\"hello world\"\n");
}

TYPED_TEST(JsonWriters, Unicode)
{
	this->Impl.m_pJson->WriteStrValue("Heizölrückstoßabdämpfung");
	this->Impl.Expect("\"Heizölrückstoßabdämpfung\"\n");
}

TYPED_TEST(JsonWriters, True)
{
	this->Impl.m_pJson->WriteBoolValue(true);
	this->Impl.Expect("true\n");
}

TYPED_TEST(JsonWriters, False)
{
	this->Impl.m_pJson->WriteBoolValue(false);
	this->Impl.Expect("false\n");
}

TYPED_TEST(JsonWriters, Null)
{
	this->Impl.m_pJson->WriteNullValue();
	this->Impl.Expect("null\n");
}

TYPED_TEST(JsonWriters, EmptyString)
{
	this->Impl.m_pJson->WriteStrValue("");
	this->Impl.Expect("\"\"\n");
}

TYPED_TEST(JsonWriters, EscapeNewline)
{
	this->Impl.m_pJson->WriteStrValue("\n");
	this->Impl.Expect("\"\\n\"\n");
}

TYPED_TEST(JsonWriters, EscapeBackslash)
{
	this->Impl.m_pJson->WriteStrValue("\\");
	this->Impl.Expect("\"\\\\\"\n"); // https://www.xkcd.com/1638/
}

TYPED_TEST(JsonWriters, EscapeControl)
{
	this->Impl.m_pJson->WriteStrValue("\x1b");
	this->Impl.Expect("\"\\u001b\"\n");
}

TYPED_TEST(JsonWriters, EscapeUnicode)
{
	this->Impl.m_pJson->WriteStrValue("愛😂");
	this->Impl.Expect("\"愛😂\"\n");
}

TYPED_TEST(JsonWriters, Zero)
{
	this->Impl.m_pJson->WriteIntValue(0);
	this->Impl.Expect("0\n");
}

TYPED_TEST(JsonWriters, One)
{
	this->Impl.m_pJson->WriteIntValue(1);
	this->Impl.Expect("1\n");
}

TYPED_TEST(JsonWriters, MinusOne)
{
	this->Impl.m_pJson->WriteIntValue(-1);
	this->Impl.Expect("-1\n");
}

TYPED_TEST(JsonWriters, Large)
{
	this->Impl.m_pJson->WriteIntValue(std::numeric_limits<int>::max());
	this->Impl.Expect("2147483647\n");
}

TYPED_TEST(JsonWriters, Small)
{
	this->Impl.m_pJson->WriteIntValue(std::numeric_limits<int>::min());
	this->Impl.Expect("-2147483648\n");
}
