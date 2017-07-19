#ifndef ENGINE_SHARED_UUID_MANAGER_H
#define ENGINE_SHARED_UUID_MANAGER_H

#include <base/tl/array.h>

enum
{
	UUID_MAXSTRSIZE = 37, // 12345678-0123-5678-0123-567890123456

	UUID_INVALID=-2,
	UUID_UNKNOWN=-1,

	OFFSET_UUID=1<<16,
};

struct CUuid
{
	unsigned char m_aData[16];

	bool operator==(const CUuid &Other);
	bool operator!=(const CUuid &Other);
};

CUuid CalculateUuid(const char *pName);
void FormatUuid(CUuid Uuid, char *pBuffer, unsigned BufferLength);

struct CName
{
	CUuid m_Uuid;
	const char *m_pName;
};

class CPacker;
class CUnpacker;

class CUuidManager
{
	array<CName> m_aNames;
public:
	void RegisterName(int ID, const char *pName);
	CUuid GetUuid(int ID) const;
	const char *GetName(int ID) const;
	int LookupUuid(CUuid Uuid) const;

	int UnpackUuid(CUnpacker *pUnpacker) const;
	int UnpackUuid(CUnpacker *pUnpacker, CUuid *pOut) const;
	void PackUuid(int ID, CPacker *pPacker) const;

	void DebugDump() const;
};

extern CUuidManager g_UuidManager;

#endif // ENGINE_SHARED_UUID_MANAGER_H
