/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_STORAGE_H
#define ENGINE_STORAGE_H

#include "kernel.h"
#include <string>
#include <vector>

enum
{
	MAX_PATHS = 16,
	MAX_PATH_LENGTH = 512
};

class IStorage : public IInterface
{
	MACRO_INTERFACE("storage", 0)
public:
	enum
	{
		TYPE_SAVE = 0,
		TYPE_ALL = -1,
		TYPE_ABSOLUTE = -2,

		STORAGETYPE_BASIC = 0,
		STORAGETYPE_SERVER,
		STORAGETYPE_CLIENT,

		INTEGRITY_DISABLED = 0,
		INTEGRITY_PENDING,
		INTEGRITY_DIRTY,
		INTEGRITY_PURE
	};

	virtual bool DIStartCheck(class IEngine *pEngine) = 0;
	virtual int DIStatus() const = 0;

	// Only safe to call after you've verified that DIStatus() != INTEGRITY_PENDING
	virtual const std::vector<std::string> &DIExtraFiles() const = 0;
	virtual std::vector<std::string> DIMissingFiles() = 0;
	virtual std::vector<std::string> DIModifiedFiles() = 0;

	virtual void ListDirectory(int Type, const char *pPath, FS_LISTDIR_CALLBACK pfnCallback, void *pUser) = 0;
	virtual void ListDirectoryInfo(int Type, const char *pPath, FS_LISTDIR_INFO_CALLBACK pfnCallback, void *pUser) = 0;
	virtual IOHANDLE OpenFile(const char *pFilename, int Flags, int Type, char *pBuffer = 0, int BufferSize = 0) = 0;
	virtual bool FindFile(const char *pFilename, const char *pPath, int Type, char *pBuffer, int BufferSize) = 0;
	virtual bool RemoveFile(const char *pFilename, int Type) = 0;
	virtual bool RenameFile(const char *pOldFilename, const char *pNewFilename, int Type) = 0;
	virtual bool CreateFolder(const char *pFoldername, int Type) = 0;
	virtual void GetCompletePath(int Type, const char *pDir, char *pBuffer, unsigned BufferSize) = 0;

	virtual bool RemoveBinaryFile(const char *pFilename) = 0;
	virtual bool RenameBinaryFile(const char *pOldFilename, const char *pNewFilename) = 0;
	virtual const char *GetBinaryPath(const char *pFilename, char *pBuffer, unsigned BufferSize) = 0;

	static void StripPathAndExtension(const char *pFilename, char *pBuffer, int BufferSize);
};

extern IStorage *CreateStorage(const char *pApplicationName, int StorageType, int NumArgs, const char **ppArguments);
extern IStorage *CreateLocalStorage();

#endif
