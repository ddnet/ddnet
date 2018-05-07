#include "mapbugs.h"

#include <base/system.h>

struct CMapDescription
{
	const char *m_pName;
	int m_Size;
	int m_Crc;

	bool operator==(const CMapDescription &Other) const
	{
		return str_comp(m_pName, Other.m_pName) == 0 &&
			m_Size == Other.m_Size &&
			m_Crc == Other.m_Crc;
	}
};

struct CMapBugsInternal
{
	CMapDescription m_Map;
	unsigned int m_BugFlags;
};

static unsigned int BugToFlag(int Bug)
{
	unsigned int Result;
	dbg_assert((unsigned)Bug < 8 * sizeof(Result), "invalid shift");
	return Result = (1 << Bug);
}

static unsigned int IsBugFlagSet(int Bug, unsigned int BugFlags)
{
	return (BugFlags & BugToFlag(Bug)) != 0;
}

static CMapBugsInternal MAP_BUGS[] =
{
	{{"Binary", 2022597, 0x0ae3a3d5}, BugToFlag(BUG_GRENADE_DOUBLEEXPLOSION)}
};

CMapBugs GetMapBugs(const char *pName, int Size, int Crc)
{
	CMapDescription Map = {pName, Size, Crc};
	CMapBugs Result;
	Result.m_Extra = 0;
	for(unsigned int i = 0; i < sizeof(MAP_BUGS) / sizeof(MAP_BUGS[0]); i++)
	{
		if(Map == MAP_BUGS[i].m_Map)
		{
			Result.m_pData = &MAP_BUGS[i];
			return Result;
		}
	}
	Result.m_pData = 0;
	return Result;
}

bool CMapBugs::Contains(int Bug) const
{
	CMapBugsInternal *pInternal = (CMapBugsInternal *)m_pData;
	if(!pInternal)
	{
		return IsBugFlagSet(Bug, m_Extra);
	}
	return IsBugFlagSet(Bug, pInternal->m_BugFlags);
}

int CMapBugs::Update(const char *pBug)
{
	CMapBugsInternal *pInternal = (CMapBugsInternal *)m_pData;
	int Bug = -1;
	if(false) {}
#define MAPBUG(constname, string) else if(str_comp(pBug, string) == 0) { Bug = constname; }
#include "mapbugs_list.h"
#undef MAPBUG
	if(Bug == -1)
	{
		return MAPBUGUPDATE_NOTFOUND;
	}
	if(pInternal)
	{
		return MAPBUGUPDATE_OVERRIDDEN;
	}
	m_Extra |= BugToFlag(Bug);
	return MAPBUGUPDATE_OK;
}

void CMapBugs::Dump() const
{
	CMapBugsInternal *pInternal = (CMapBugsInternal *)m_pData;
	unsigned int Flags;
	if(pInternal)
	{
		Flags = pInternal->m_BugFlags;
	}
	else if(m_Extra)
	{
		Flags = m_Extra;
	}
	else
	{
		return;
	}
	char aBugs[NUM_BUGS + 1] = {0};
	for(int i = 0; i < NUM_BUGS; i++)
	{
		aBugs[i] = IsBugFlagSet(i, Flags) ? 'X' : 'O';
	}

	dbg_msg("mapbugs", "enabling map compatibility mode %s", aBugs);
	if(pInternal)
	{
		dbg_msg("mapbugs", "map='%s' map_size=%d map_crc=%08x",
			pInternal->m_Map.m_pName,
			pInternal->m_Map.m_Size,
			pInternal->m_Map.m_Crc);
	}
}
