#ifndef STORAGE_H
#define STORAGE_H

#include <base/system.h>
#include <engine/storage.h>
#include <engine/engine.h>

// compiled-in data-dir path
#define DATA_DIR "data"



class CStorage : public IStorage
{
public:
	char m_aApplicationSavePath[512];
	char m_aDatadir[512];

	CStorage()
	{
		m_aApplicationSavePath[0] = 0;
		m_aDatadir[0] = 0;
	}
	
	int FindDatadir(const char *pArgv0);

	virtual void ListDirectory(int Types, const char *pPath, FS_LISTDIR_CALLBACK pfnCallback, void *pUser);
	virtual void ListDirectoryInfo(int Type, const char *pPath, FS_LISTDIR_INFO_CALLBACK pfnCallback, void *pUser) = 0;

	virtual IOHANDLE OpenFile(const char *pFilename, int Flags, int Type, char *pBuffer = 0, int BufferSize = 0);
};

IStorage *CreateStorage(const char *pApplicationName, const char *pArgv0);

#endif
