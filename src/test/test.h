#ifndef TEST_TEST_H
#define TEST_TEST_H

#include <cstddef>
#include <memory>

class IStorage;

class CTestInfo
{
public:
	CTestInfo();
	~CTestInfo();
	std::unique_ptr<IStorage> CreateTestStorage();
	bool m_DeleteTestStorageFilesOnSuccess = false;
	void Filename(char *pBuffer, size_t BufferLength, const char *pSuffix);
	char m_aFilenamePrefix[128];
	char m_aFilename[128];
};
#endif // TEST_TEST_H
