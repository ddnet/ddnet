#include "test.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/logger.h>
#include <base/net.h>
#include <base/os.h>
#include <base/process.h>
#include <base/str.h>

#include <engine/storage.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>

CTestInfo::CTestInfo()
{
	const ::testing::TestInfo *pTestInfo =
		::testing::UnitTest::GetInstance()->current_test_info();

	// Typed tests have test names like "TestName/0" and "TestName/1", which would result in invalid filenames.
	// Replace the string after the first slash with the name of the typed test and use hyphen instead of slash.
	char aTestCaseName[128];
	str_copy(aTestCaseName, pTestInfo->test_case_name());
	for(int i = 0; i < str_length(aTestCaseName); i++)
	{
		if(aTestCaseName[i] == '/')
		{
			aTestCaseName[i] = '-';
			aTestCaseName[i + 1] = '\0';
			str_append(aTestCaseName, pTestInfo->type_param());
			break;
		}
	}
	str_format(m_aFilenamePrefix, sizeof(m_aFilenamePrefix), "%s.%s-%d",
		aTestCaseName, pTestInfo->name(), process_id());
	Filename(m_aFilename, sizeof(m_aFilename), ".tmp");
}

void CTestInfo::Filename(char *pBuffer, size_t BufferLength, const char *pSuffix)
{
	str_format(pBuffer, BufferLength, "%s%s", m_aFilenamePrefix, pSuffix);
}

std::unique_ptr<IStorage> CTestInfo::CreateTestStorage()
{
	bool Error = fs_makedir(m_aFilename);
	EXPECT_FALSE(Error);
	if(Error)
	{
		return nullptr;
	}
	char aTestPath[IO_MAX_PATH_LENGTH];
	str_copy(aTestPath, ::testing::internal::GetArgvs().front().c_str());
	const char *apArgs[] = {aTestPath};
	return CreateTempStorage(m_aFilename, std::size(apArgs), apArgs);
}

class CTestInfoPath
{
public:
	bool m_IsDirectory;
	char m_aData[IO_MAX_PATH_LENGTH];

	bool operator<(const CTestInfoPath &Other) const
	{
		if(m_IsDirectory != Other.m_IsDirectory)
		{
			return m_IsDirectory < Other.m_IsDirectory;
		}
		return str_comp(m_aData, Other.m_aData) > 0;
	}
};

class CTestCollectData
{
public:
	char m_aCurrentDir[IO_MAX_PATH_LENGTH];
	std::vector<CTestInfoPath> *m_pvEntries;
};

int TestCollect(const char *pName, int IsDir, int Unused, void *pUser)
{
	CTestCollectData *pData = (CTestCollectData *)pUser;

	if(str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0)
	{
		return 0;
	}

	CTestInfoPath Path;
	Path.m_IsDirectory = IsDir;
	str_format(Path.m_aData, sizeof(Path.m_aData), "%s/%s", pData->m_aCurrentDir, pName);
	pData->m_pvEntries->push_back(Path);
	if(Path.m_IsDirectory)
	{
		CTestCollectData DataRecursive;
		str_copy(DataRecursive.m_aCurrentDir, Path.m_aData, sizeof(DataRecursive.m_aCurrentDir));
		DataRecursive.m_pvEntries = pData->m_pvEntries;
		fs_listdir(DataRecursive.m_aCurrentDir, TestCollect, 0, &DataRecursive);
	}
	return 0;
}

void TestDeleteTestStorageFiles(const char *pPath)
{
	std::vector<CTestInfoPath> vEntries;
	CTestCollectData Data;
	str_copy(Data.m_aCurrentDir, pPath, sizeof(Data.m_aCurrentDir));
	Data.m_pvEntries = &vEntries;
	fs_listdir(Data.m_aCurrentDir, TestCollect, 0, &Data);

	CTestInfoPath Path;
	Path.m_IsDirectory = true;
	str_copy(Path.m_aData, Data.m_aCurrentDir, sizeof(Path.m_aData));
	vEntries.push_back(Path);

	// Sorts directories after files.
	std::sort(vEntries.begin(), vEntries.end());

	// Don't delete too many files.
	ASSERT_LE(vEntries.size(), 10);
	for(auto &Entry : vEntries)
	{
		if(Entry.m_IsDirectory)
		{
			ASSERT_FALSE(fs_removedir(Entry.m_aData));
		}
		else
		{
			ASSERT_FALSE(fs_remove(Entry.m_aData));
		}
	}
}

CTestInfo::~CTestInfo()
{
	if(!::testing::Test::HasFailure() && m_DeleteTestStorageFilesOnSuccess)
	{
		TestDeleteTestStorageFiles(m_aFilename);
	}
}

class CTestLogger : public ILogger
{
	friend class CTestLogListener;

	std::unique_ptr<ILogger> m_pGlobalLogger;
	std::unique_ptr<CMemoryLogger> m_pMemoryLogger;
	std::mutex m_Lock;

public:
	CTestLogger(std::unique_ptr<ILogger> &&pGlobalLogger) :
		m_pGlobalLogger(std::move(pGlobalLogger))
	{
	}

	void Log(const CLogMessage *pMessage) override
	{
		if(m_Filter.Filters(pMessage))
		{
			return;
		}
		const std::unique_lock Lock(m_Lock);
		if(m_pMemoryLogger != nullptr)
		{
			m_pMemoryLogger->Log(pMessage);
		}
		else
		{
			m_pGlobalLogger->Log(pMessage);
		}
	}

	void GlobalFinish() override
	{
		dbg_assert(m_pMemoryLogger == nullptr, "Memory logger should be unset when test logger finishes");
		m_pGlobalLogger->GlobalFinish();
	}
};

class CTestLogListener : public testing::EmptyTestEventListener
{
	CTestLogger *m_pTestLogger;
	bool m_CaptureOutput;

public:
	CTestLogListener(CTestLogger *pTestLogger, bool CaptureOutput) :
		m_pTestLogger(pTestLogger),
		m_CaptureOutput(CaptureOutput)
	{
	}

	void OnTestStart(const testing::TestInfo &TestInfo) override
	{
		if(!m_CaptureOutput)
		{
			return;
		}

		const std::unique_lock Lock(m_pTestLogger->m_Lock);
		m_pTestLogger->m_pMemoryLogger = std::make_unique<CMemoryLogger>();
	}

	void OnTestEnd(const testing::TestInfo &TestInfo) override
	{
		if(!m_CaptureOutput)
		{
			return;
		}

		const std::unique_lock Lock(m_pTestLogger->m_Lock);
		if(TestInfo.result()->Failed())
		{
			for(const CLogMessage &Line : m_pTestLogger->m_pMemoryLogger->Lines())
			{
				m_pTestLogger->m_pGlobalLogger->Log(&Line);
			}
		}
		m_pTestLogger->m_pMemoryLogger = nullptr;
	}
};

int main(int argc, const char **argv)
{
	CTestLogger *pTestLogger = std::make_unique<CTestLogger>(log_logger_default()).release();
	log_set_global_logger(pTestLogger);

	CCmdlineFix CmdlineFix(&argc, &argv);

	::testing::InitGoogleTest(&argc, const_cast<char **>(argv));
	GTEST_FLAG_SET(death_test_style, "threadsafe");

	bool CaptureOutput = true;
	if(argc >= 2)
	{
		for(int Argument = 1; Argument < argc; ++Argument)
		{
			if(str_comp(argv[Argument], "--no-capture") == 0)
			{
				CaptureOutput = false;
			}
			else
			{
				log_error("testrunner", "Unknown argument: %s", argv[Argument]);
				return -1;
			}
		}
	}

	testing::TestEventListeners &Listeners = testing::UnitTest::GetInstance()->listeners();
	Listeners.Append(new CTestLogListener(pTestLogger, CaptureOutput));

	net_init();

	return RUN_ALL_TESTS();
}

TEST(TestInfo, Sort)
{
	std::vector<CTestInfoPath> vEntries;
	vEntries.resize(3);

	const char aBasePath[] = "test_dir";
	const char aSubPath[] = "test_dir/subdir";
	const char aFilePath[] = "test_dir/subdir/file.txt";

	vEntries[0].m_IsDirectory = true;
	str_copy(vEntries[0].m_aData, aBasePath);

	vEntries[1].m_IsDirectory = true;
	str_copy(vEntries[1].m_aData, aSubPath);

	vEntries[2].m_IsDirectory = false;
	str_copy(vEntries[2].m_aData, aFilePath);

	// Sorts directories after files.
	std::sort(vEntries.begin(), vEntries.end());

	EXPECT_FALSE(vEntries[0].m_IsDirectory);
	EXPECT_EQ(str_comp(vEntries[0].m_aData, aFilePath), 0);
	EXPECT_TRUE(vEntries[1].m_IsDirectory);
	EXPECT_EQ(str_comp(vEntries[1].m_aData, aSubPath), 0);
	EXPECT_TRUE(vEntries[2].m_IsDirectory);
	EXPECT_EQ(str_comp(vEntries[2].m_aData, aBasePath), 0);
}
