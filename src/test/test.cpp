#include "test.h"
#include <gtest/gtest.h>

#include <base/logger.h>
#include <base/system.h>
#include <engine/storage.h>

#include <algorithm>

CTestInfo::CTestInfo()
{
	const ::testing::TestInfo *pTestInfo =
		::testing::UnitTest::GetInstance()->current_test_info();
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s.%s", pTestInfo->test_case_name(), pTestInfo->name());
	IStorage::FormatTmpPath(m_aFilename, sizeof(m_aFilename), aBuf);
}

IStorage *CTestInfo::CreateTestStorage()
{
	bool Error = fs_makedir(m_aFilename);
	EXPECT_FALSE(Error);
	if(Error)
	{
		return nullptr;
	}
	return CreateTempStorage(m_aFilename);
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
		return str_comp(m_aData, Other.m_aData) < 0;
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

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();
	::testing::InitGoogleTest(&argc, const_cast<char **>(argv));
	net_init();
	if(secure_random_init())
	{
		fprintf(stderr, "random init failed\n");
		return 1;
	}
	int Result = RUN_ALL_TESTS();
	secure_random_uninit();
	return Result;
}
