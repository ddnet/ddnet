#ifndef TEST_TEST_H
#define TEST_TEST_H
class IStorage;

class CTestInfo
{
public:
	CTestInfo();
	IStorage *CreateTestStorage();
	void DeleteTestStorageFilesOnSuccess();
	char m_aFilename[64];
};
#endif // TEST_TEST_H
