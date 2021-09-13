#ifndef TEST_TEST_H
#define TEST_TEST_H

#include <base/system.h>

class IStorage;

class CTestInfo
{
public:
	CTestInfo();
	IStorage *CreateTestStorage();
	void DeleteTestStorageFilesOnSuccess();
	char m_aFilename[IO_MAX_PATH_LENGTH];
};
#endif // TEST_TEST_H
