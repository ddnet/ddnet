/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_FILECOLLECTION_H
#define ENGINE_SHARED_FILECOLLECTION_H

#include <base/system.h>

#include <stdint.h>

class IStorage;

class CFileCollection
{
	enum
	{
		MAX_ENTRIES = 1001,
		TIMESTAMP_LENGTH = 20, // _YYYY-MM-DD_HH-MM-SS
	};

	int64_t m_aTimestamps[MAX_ENTRIES] = {0};
	int m_NumTimestamps = 0;
	int m_MaxEntries = 0;
	char m_aFileDesc[128] = {0};
	int m_FileDescLength = 0;
	char m_aFileExt[32] = {0};
	int m_FileExtLength = 0;
	char m_aPath[IO_MAX_PATH_LENGTH] = {0};
	IStorage *m_pStorage = nullptr;
	int64_t m_Remove = 0; // Timestamp we want to remove

	bool IsFilenameValid(const char *pFilename);
	int64_t ExtractTimestamp(const char *pTimestring);
	void BuildTimestring(int64_t Timestamp, char *pTimestring);
	int64_t GetTimestamp(const char *pFilename);

public:
	void Init(IStorage *pStorage, const char *pPath, const char *pFileDesc, const char *pFileExt, int MaxEntries);
	void AddEntry(int64_t Timestamp);

	static int FilelistCallback(const char *pFilename, int IsDir, int StorageType, void *pUser);
	static int RemoveCallback(const char *pFilename, int IsDir, int StorageType, void *pUser);
};

#endif
