#include <gtest/gtest.h>

#include <base/system.h>

class Async : public ::testing::Test
{
protected:
	ASYNCIO *m_pAio;
	char m_aFilename[64];
	bool Delete;

	Async()
	{
		const ::testing::TestInfo *pTestInfo =
			::testing::UnitTest::GetInstance()->current_test_info();
		const char *pTestName = pTestInfo->name();

		str_format(m_aFilename, sizeof(m_aFilename), "Async.%s-%d.tmp", pTestName, pid());
		m_pAio = async_new(io_open(m_aFilename, IOFLAG_WRITE));
		Delete = false;
	}

	~Async()
	{
		if(Delete)
		{
			fs_remove(m_aFilename);
		}
	}

	void Write(const char *pText)
	{
		async_write(m_pAio, pText, str_length(pText));
	}

	void Expect(const char *pOutput)
	{
		async_close(m_pAio);
		async_wait(m_pAio);
		async_free(m_pAio);

		char aBuf[64 * 1024];
		IOHANDLE File = io_open(m_aFilename, IOFLAG_READ);
		int Read = io_read(File, aBuf, sizeof(aBuf));

		ASSERT_EQ(str_length(pOutput), Read);
		ASSERT_TRUE(mem_comp(aBuf, pOutput, Read) == 0);
		Delete = true;
	}
};

TEST_F(Async, Empty)
{
	Expect("");
}

TEST_F(Async, Simple)
{
	static const char TEXT[] = "a\n";
	Write(TEXT);
	Expect(TEXT);
}

TEST_F(Async, Long)
{
	char aText[32 * 1024 + 1];
	for(unsigned i = 0; i < sizeof(aText) - 1; i++)
	{
		aText[i] = 'a';
	}
	aText[sizeof(aText) - 1] = 0;
	Write(aText);
	Expect(aText);
}

TEST_F(Async, Pieces)
{
	char aText[64 * 1024 + 1];
	for(unsigned i = 0; i < sizeof(aText) - 1; i++)
	{
		aText[i] = 'a';
	}
	aText[sizeof(aText) - 1] = 0;
	for(unsigned i = 0; i < sizeof(aText) - 1; i++)
	{
		Write("a");
	}
	Expect(aText);
}
