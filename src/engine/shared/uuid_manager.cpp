#include "uuid_manager.h"

#include <base/hash_ctxt.h>
#include <engine/shared/packer.h>

#include <stdio.h>

static const CUuid TEEWORLDS_NAMESPACE = {{// "e05ddaaa-c4e6-4cfb-b642-5d48e80c0029"
	0xe0, 0x5d, 0xda, 0xaa, 0xc4, 0xe6, 0x4c, 0xfb,
	0xb6, 0x42, 0x5d, 0x48, 0xe8, 0x0c, 0x00, 0x29}};

CUuid RandomUuid()
{
	CUuid Result;
	secure_random_fill(&Result, sizeof(Result));

	Result.m_aData[6] &= 0x0f;
	Result.m_aData[6] |= 0x40;
	Result.m_aData[8] &= 0x3f;
	Result.m_aData[8] |= 0x80;

	return Result;
}

CUuid CalculateUuid(const char *pName)
{
	MD5_CTX Md5;
	md5_init(&Md5);
	md5_update(&Md5, TEEWORLDS_NAMESPACE.m_aData, sizeof(TEEWORLDS_NAMESPACE.m_aData));
	// Without terminating NUL.
	md5_update(&Md5, (const unsigned char *)pName, str_length(pName));
	MD5_DIGEST Digest = md5_finish(&Md5);

	CUuid Result;
	for(unsigned i = 0; i < sizeof(Result.m_aData); i++)
	{
		Result.m_aData[i] = Digest.data[i];
	}

	Result.m_aData[6] &= 0x0f;
	Result.m_aData[6] |= 0x30;
	Result.m_aData[8] &= 0x3f;
	Result.m_aData[8] |= 0x80;
	return Result;
}

void FormatUuid(CUuid Uuid, char *pBuffer, unsigned BufferLength)
{
	unsigned char *p = Uuid.m_aData;
	str_format(pBuffer, BufferLength,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
		p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

int ParseUuid(CUuid *pUuid, const char *pBuffer)
{
	if(str_length(pBuffer) + 1 != UUID_MAXSTRSIZE)
	{
		return 2;
	}
	char aCopy[UUID_MAXSTRSIZE];
	str_copy(aCopy, pBuffer, sizeof(aCopy));
	// 01234567-9012-4567-9012-456789012345
	if(aCopy[8] != '-' || aCopy[13] != '-' || aCopy[18] != '-' || aCopy[23] != '-')
	{
		return 1;
	}
	aCopy[8] = aCopy[13] = aCopy[18] = aCopy[23] = 0;
	if(0 ||
		str_hex_decode(pUuid->m_aData + 0, 4, aCopy + 0) ||
		str_hex_decode(pUuid->m_aData + 4, 2, aCopy + 9) ||
		str_hex_decode(pUuid->m_aData + 6, 2, aCopy + 14) ||
		str_hex_decode(pUuid->m_aData + 8, 2, aCopy + 19) ||
		str_hex_decode(pUuid->m_aData + 10, 6, aCopy + 24))
	{
		return 1;
	}
	return 0;
}

bool CUuid::operator==(const CUuid &Other) const
{
	return mem_comp(this, &Other, sizeof(*this)) == 0;
}

bool CUuid::operator!=(const CUuid &Other) const
{
	return !(*this == Other);
}

static int GetIndex(int ID)
{
	return ID - OFFSET_UUID;
}

static int GetID(int Index)
{
	return Index + OFFSET_UUID;
}

void CUuidManager::RegisterName(int ID, const char *pName)
{
	dbg_assert(GetIndex(ID) == m_aNames.size(), "names must be registered with increasing ID");
	CName Name;
	Name.m_pName = pName;
	Name.m_Uuid = CalculateUuid(pName);
	dbg_assert(LookupUuid(Name.m_Uuid) == -1, "duplicate uuid");

	m_aNames.add(Name);

	CNameIndexed NameIndexed;
	NameIndexed.m_Uuid = Name.m_Uuid;
	NameIndexed.m_ID = GetIndex(ID);
	m_aNamesSorted.add(NameIndexed);
}

CUuid CUuidManager::GetUuid(int ID) const
{
	return m_aNames[GetIndex(ID)].m_Uuid;
}

const char *CUuidManager::GetName(int ID) const
{
	return m_aNames[GetIndex(ID)].m_pName;
}

int CUuidManager::LookupUuid(CUuid Uuid) const
{
	sorted_array<CNameIndexed>::range Pos = ::find_binary(m_aNamesSorted.all(), Uuid);
	if(!Pos.empty())
	{
		return GetID(Pos.front().m_ID);
	}
	return UUID_UNKNOWN;
}

int CUuidManager::NumUuids() const
{
	return m_aNames.size();
}

int CUuidManager::UnpackUuid(CUnpacker *pUnpacker) const
{
	CUuid Temp;
	return UnpackUuid(pUnpacker, &Temp);
}

int CUuidManager::UnpackUuid(CUnpacker *pUnpacker, CUuid *pOut) const
{
	const CUuid *pUuid = (const CUuid *)pUnpacker->GetRaw(sizeof(*pUuid));
	if(pUuid == NULL)
	{
		return UUID_INVALID;
	}
	*pOut = *pUuid;
	return LookupUuid(*pUuid);
}

void CUuidManager::PackUuid(int ID, CPacker *pPacker) const
{
	CUuid Uuid = GetUuid(ID);
	pPacker->AddRaw(&Uuid, sizeof(Uuid));
}

void CUuidManager::DebugDump() const
{
	for(int i = 0; i < m_aNames.size(); i++)
	{
		char aBuf[UUID_MAXSTRSIZE];
		FormatUuid(m_aNames[i].m_Uuid, aBuf, sizeof(aBuf));
		dbg_msg("uuid", "%s %s", aBuf, m_aNames[i].m_pName);
	}
}
