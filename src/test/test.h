#ifndef TEST_TEST_H
#define TEST_TEST_H

class IStorage;

class CTestInfo
{
public:
	CTestInfo();
	~CTestInfo();
	IStorage *CreateTestStorage();
	bool m_DeleteTestStorageFilesOnSuccess = false;
	char m_aFilename[64];
};
#endif // TEST_TEST_H
