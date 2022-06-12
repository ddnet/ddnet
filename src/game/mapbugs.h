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

enum
{
	MAPBUGUPDATE_OK,
	MAPBUGUPDATE_NOTFOUND,
	MAPBUGUPDATE_OVERRIDDEN,
};

class CMapBugs
{
	friend CMapBugs GetMapBugs(const char *pName, int Size, SHA256_DIGEST Sha256);
	void *m_pData;
	unsigned int m_Extra;

public:
	bool Contains(int Bug) const;
	int Update(const char *pBug);
	void Dump() const;
};

CMapBugs GetMapBugs(const char *pName, int Size, SHA256_DIGEST Sha256);
#endif // GAME_MAPBUGS_H
