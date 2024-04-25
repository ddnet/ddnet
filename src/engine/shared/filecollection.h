/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_FILECOLLECTION_H
#define ENGINE_SHARED_FILECOLLECTION_H

#include <base/system.h>
#include <base/types.h>

#include <cstdint>
#include <vector>

class IStorage;

class CFileCollection
{
	enum
	{
		TIMESTAMP_LENGTH = 20, // _YYYY-MM-DD_HH-MM-SS
	};

	struct CFileEntry
	{
		time_t m_Timestamp;
		char m_aFilename[IO_MAX_PATH_LENGTH];
		CFileEntry(int64_t Timestamp, const char *pFilename)
		{
			m_Timestamp = Timestamp;
			str_copy(m_aFilename, pFilename);
		}
	};

	std::vector<CFileEntry> m_vFileEntries;
	char m_aFileDesc[128];
	int m_FileDescLength;
	char m_aFileExt[32];
	int m_FileExtLength;
	char m_aPath[IO_MAX_PATH_LENGTH];
	IStorage *m_pStorage;

	bool ExtractTimestamp(const char *pTimestring, time_t *pTimestamp);
	bool ParseFilename(const char *pFilename, time_t *pTimestamp);

public:
	void Init(IStorage *pStorage, const char *pPath, const char *pFileDesc, const char *pFileExt, int MaxEntries);

	static int FilelistCallback(const char *pFilename, int IsDir, int StorageType, void *pUser);
};

#endif
