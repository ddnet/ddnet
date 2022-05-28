#ifndef ENGINE_SHARED_UUID_MANAGER_H
#define ENGINE_SHARED_UUID_MANAGER_H

#include <vector>

#include <base/system.h>

enum
{
	UUID_MAXSTRSIZE = 37, // 12345678-0123-5678-0123-567890123456

	UUID_INVALID = -2,
	UUID_UNKNOWN = -1,

	OFFSET_UUID = 1 << 16,
};

struct CUuid
{
	unsigned char m_aData[16];

	bool operator==(const CUuid &Other) const;
	bool operator!=(const CUuid &Other) const;
	bool operator<(const CUuid &Other) const { return mem_comp(m_aData, Other.m_aData, sizeof(m_aData)) < 0; }
};

CUuid RandomUuid();
CUuid CalculateUuid(const char *pName);
// The buffer length should be at least UUID_MAXSTRSIZE.
void FormatUuid(CUuid Uuid, char *pBuffer, unsigned BufferLength);
// Returns nonzero on failure.
int ParseUuid(CUuid *pUuid, const char *pBuffer);

struct CName
{
	CUuid m_Uuid;
	const char *m_pName;
};

struct CNameIndexed
{
	CUuid m_Uuid;
	int m_ID;

	bool operator<(const CNameIndexed &Other) const { return m_Uuid < Other.m_Uuid; }
	bool operator==(const CNameIndexed &Other) const { return m_Uuid == Other.m_Uuid; }
};

class CPacker;
class CUnpacker;

class CUuidManager
{
	std::vector<CName> m_vNames;
	std::vector<CNameIndexed> m_vNamesSorted;

public:
	void RegisterName(int ID, const char *pName);
	CUuid GetUuid(int ID) const;
	const char *GetName(int ID) const;
	int LookupUuid(CUuid Uuid) const;
	int NumUuids() const;

	int UnpackUuid(CUnpacker *pUnpacker) const;
	int UnpackUuid(CUnpacker *pUnpacker, CUuid *pOut) const;
	void PackUuid(int ID, CPacker *pPacker) const;

	void DebugDump() const;
};

extern CUuidManager g_UuidManager;

#endif // ENGINE_SHARED_UUID_MANAGER_H
