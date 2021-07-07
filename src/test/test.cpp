#include "test.h"
#include <gtest/gtest.h>

#include <base/system.h>
#include <engine/storage.h>

#include <algorithm>

CTestInfo::CTestInfo()
{
	const ::testing::TestInfo *pTestInfo =
		::testing::UnitTest::GetInstance()->current_test_info();
	str_format(m_aFilename, sizeof(m_aFilename), "%s.%s-%d.tmp",
		pTestInfo->test_case_name(), pTestInfo->name(), pid());
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
	char m_aData[MAX_PATH_LENGTH];

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
	char m_aCurrentDir[MAX_PATH_LENGTH];
	std::vector<CTestInfoPath> *m_paEntries;
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
	pData->m_paEntries->push_back(Path);
	if(Path.m_IsDirectory)
	{
		CTestCollectData DataRecursive;
		str_copy(DataRecursive.m_aCurrentDir, Path.m_aData, sizeof(DataRecursive.m_aCurrentDir));
		DataRecursive.m_paEntries = pData->m_paEntries;
		fs_listdir(DataRecursive.m_aCurrentDir, TestCollect, 0, &DataRecursive);
	}
	return 0;
}

void CTestInfo::DeleteTestStorageFilesOnSuccess()
{
	if(::testing::Test::HasFailure())
	{
		return;
	}
	std::vector<CTestInfoPath> aEntries;
	CTestCollectData Data;
	str_copy(Data.m_aCurrentDir, m_aFilename, sizeof(Data.m_aCurrentDir));
	Data.m_paEntries = &aEntries;
	fs_listdir(Data.m_aCurrentDir, TestCollect, 0, &Data);

	CTestInfoPath Path;
	Path.m_IsDirectory = true;
	str_copy(Path.m_aData, Data.m_aCurrentDir, sizeof(Path.m_aData));
	aEntries.push_back(Path);

	// Sorts directories after files.
	std::sort(aEntries.begin(), aEntries.end());

	// Don't delete too many files.
	ASSERT_LE(aEntries.size(), 10);
	for(auto &Entry : aEntries)
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

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	net_init();
	if(secure_random_init())
	{
		fprintf(stderr, "random init failed\n");
		return 1;
	}
	return RUN_ALL_TESTS();
}
