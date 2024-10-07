/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <algorithm>
#include <base/math.h>

#include <engine/storage.h>

#include "filecollection.h"

void CFileCollection::Init(IStorage *pStorage, const char *pPath, const char *pFileDesc, const char *pFileExt, int MaxEntries)
{
	m_vFileEntries.clear();
	str_copy(m_aFileDesc, pFileDesc);
	m_FileDescLength = str_length(m_aFileDesc);
	str_copy(m_aFileExt, pFileExt);
	m_FileExtLength = str_length(m_aFileExt);
	str_copy(m_aPath, pPath);
	m_pStorage = pStorage;

	m_pStorage->ListDirectory(IStorage::TYPE_SAVE, m_aPath, FilelistCallback, this);
	std::sort(m_vFileEntries.begin(), m_vFileEntries.end(), [](const CFileEntry &Lhs, const CFileEntry &Rhs) { return Lhs.m_Timestamp < Rhs.m_Timestamp; });

	int FilesDeleted = 0;
	for(auto FileEntry : m_vFileEntries)
	{
		if((int)m_vFileEntries.size() - FilesDeleted <= MaxEntries)
			break;

		char aBuf[IO_MAX_PATH_LENGTH];
		if(m_aFileDesc[0] == '\0')
		{
			str_format(aBuf, sizeof(aBuf), "%s/%s", m_aPath, FileEntry.m_aFilename);
		}
		else
		{
			char aTimestring[TIMESTAMP_LENGTH];
			str_timestamp_ex(FileEntry.m_Timestamp, aTimestring, sizeof(aBuf), FORMAT_NOSPACE);
			str_format(aBuf, sizeof(aBuf), "%s/%s_%s%s", m_aPath, m_aFileDesc, aTimestring, m_aFileExt);
		}

		m_pStorage->RemoveFile(aBuf, IStorage::TYPE_SAVE);
		FilesDeleted++;
	}
}

bool CFileCollection::ExtractTimestamp(const char *pTimestring, time_t *pTimestamp)
{
	// Discard anything after timestamp length from pTimestring (most likely the extension)
	char aStrippedTimestring[TIMESTAMP_LENGTH];
	str_copy(aStrippedTimestring, pTimestring);
	return timestamp_from_str(aStrippedTimestring, FORMAT_NOSPACE, pTimestamp);
}

bool CFileCollection::ParseFilename(const char *pFilename, time_t *pTimestamp)
{
	// Check if filename is valid
	if(!str_endswith(pFilename, m_aFileExt))
		return false;

	const char *pTimestring = pFilename;

	if(m_aFileDesc[0] == '\0')
	{
		int FilenameLength = str_length(pFilename);
		if(m_FileExtLength + TIMESTAMP_LENGTH > FilenameLength)
		{
			return false;
		}

		pTimestring += FilenameLength - m_FileExtLength - TIMESTAMP_LENGTH + 1;
	}
	else
	{
		if(str_length(pFilename) != m_FileDescLength + TIMESTAMP_LENGTH + m_FileExtLength ||
			!str_startswith(pFilename, m_aFileDesc))
			return false;

		pTimestring += m_FileDescLength + 1;
	}

	// Extract timestamp
	if(!ExtractTimestamp(pTimestring, pTimestamp))
		return false;

	return true;
}

int CFileCollection::FilelistCallback(const char *pFilename, int IsDir, int StorageType, void *pUser)
{
	CFileCollection *pThis = static_cast<CFileCollection *>(pUser);

	// Try to parse filename and extract timestamp
	time_t Timestamp;
	if(IsDir || !pThis->ParseFilename(pFilename, &Timestamp))
		return 0;

	// Add the entry
	pThis->m_vFileEntries.emplace_back(Timestamp, pFilename);

	return 0;
}
