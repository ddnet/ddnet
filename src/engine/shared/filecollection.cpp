/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <algorithm>
#include <base/math.h>

#include <engine/storage.h>

#include "filecollection.h"

bool CFileCollection::IsFilenameValid(const char *pFilename)
{
	if(!str_endswith(pFilename, m_aFileExt))
		return false;

	if(m_aFileDesc[0] == '\0')
	{
		int FilenameLength = str_length(pFilename);
		if(m_FileExtLength + TIMESTAMP_LENGTH > FilenameLength)
		{
			return false;
		}

		pFilename += FilenameLength - m_FileExtLength - TIMESTAMP_LENGTH;
	}
	else
	{
		if(str_length(pFilename) != m_FileDescLength + TIMESTAMP_LENGTH + m_FileExtLength ||
			!str_startswith(pFilename, m_aFileDesc))
			return false;

		pFilename += m_FileDescLength;
	}

	return pFilename[0] == '_' &&
	       pFilename[1] >= '0' && pFilename[1] <= '9' &&
	       pFilename[2] >= '0' && pFilename[2] <= '9' &&
	       pFilename[3] >= '0' && pFilename[3] <= '9' &&
	       pFilename[4] >= '0' && pFilename[4] <= '9' &&
	       pFilename[5] == '-' &&
	       pFilename[6] >= '0' && pFilename[6] <= '9' &&
	       pFilename[7] >= '0' && pFilename[7] <= '9' &&
	       pFilename[8] == '-' &&
	       pFilename[9] >= '0' && pFilename[9] <= '9' &&
	       pFilename[10] >= '0' && pFilename[10] <= '9' &&
	       pFilename[11] == '_' &&
	       pFilename[12] >= '0' && pFilename[12] <= '9' &&
	       pFilename[13] >= '0' && pFilename[13] <= '9' &&
	       pFilename[14] == '-' &&
	       pFilename[15] >= '0' && pFilename[15] <= '9' &&
	       pFilename[16] >= '0' && pFilename[16] <= '9' &&
	       pFilename[17] == '-' &&
	       pFilename[18] >= '0' && pFilename[18] <= '9' &&
	       pFilename[19] >= '0' && pFilename[19] <= '9';
}

int64_t CFileCollection::ExtractTimestamp(const char *pTimestring)
{
	int64_t Timestamp = pTimestring[0] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[1] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[2] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[3] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[5] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[6] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[8] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[9] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[11] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[12] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[14] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[15] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[17] - '0';
	Timestamp <<= 4;
	Timestamp += pTimestring[18] - '0';

	return Timestamp;
}

void CFileCollection::Init(IStorage *pStorage, const char *pPath, const char *pFileDesc, const char *pFileExt, int MaxEntries)
{
	m_vTimestamps.clear();
	str_copy(m_aFileDesc, pFileDesc);
	m_FileDescLength = str_length(m_aFileDesc);
	str_copy(m_aFileExt, pFileExt);
	m_FileExtLength = str_length(m_aFileExt);
	str_copy(m_aPath, pPath);
	m_pStorage = pStorage;

	m_pStorage->ListDirectory(IStorage::TYPE_SAVE, m_aPath, FilelistCallback, this);
	std::sort(m_vTimestamps.begin(), m_vTimestamps.end(), [](const CFileEntry &lhs, const CFileEntry &rhs) { return lhs.m_Timestamp < rhs.m_Timestamp; });

	int FilesDeleted = 0;
	for(auto FileEntry : m_vTimestamps)
	{
		if((int)m_vTimestamps.size() - FilesDeleted <= MaxEntries)
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

int64_t CFileCollection::GetTimestamp(const char *pFilename)
{
	if(m_aFileDesc[0] == '\0')
	{
		int FilenameLength = str_length(pFilename);
		return ExtractTimestamp(pFilename + FilenameLength - m_FileExtLength - TIMESTAMP_LENGTH + 1);
	}
	else
	{
		return ExtractTimestamp(pFilename + m_FileDescLength + 1);
	}
}

int CFileCollection::FilelistCallback(const char *pFilename, int IsDir, int StorageType, void *pUser)
{
	CFileCollection *pThis = static_cast<CFileCollection *>(pUser);

	// check for valid file name format
	if(IsDir || !pThis->IsFilenameValid(pFilename))
		return 0;

	// extract the timestamp
	int64_t Timestamp = pThis->GetTimestamp(pFilename);

	// add the entry
	pThis->m_vTimestamps.emplace_back(Timestamp, pFilename);

	return 0;
}
