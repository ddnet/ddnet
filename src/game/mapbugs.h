#ifndef GAME_MAPBUGS_H
#define GAME_MAPBUGS_H

#include <base/hash.h>

enum
{
#define MAPBUG(constname, string) constname,
#include "mapbugs_list.h"
#undef MAPBUG
	NUM_BUGS,
};

enum class EMapBugUpdate
{
	OK,
	NOTFOUND,
	OVERRIDDEN,
};

class CMapBugs
{
	void *m_pData = nullptr;
	unsigned int m_Extra = 0;

public:
	static CMapBugs Create(const char *pName, int Size, SHA256_DIGEST Sha256);
	bool Contains(int Bug) const;
	EMapBugUpdate Update(const char *pBug);
	void Dump() const;
};

#endif // GAME_MAPBUGS_H
